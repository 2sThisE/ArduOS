#include "HAL.h"

// -----------------------------------------------------------------
// [1] 전역 객체 정의
// -----------------------------------------------------------------
// SD카드 설정 (SoftSpi)
SoftSpiDriver<12, 11, 13> softSpi;
SdFat32 sd;
#define SD_CONFIG SdSpiConfig(10, DEDICATED_SPI, SD_SCK_MHZ(0), &softSpi)

// 시스템 시간
volatile unsigned long system_ticks = 0;

// [신규] HAL 전용 송신 버퍼 (512로 증설)
static uint8_t hal_tx_buffer[512];

// [신규] 패킷 파싱용 버퍼 및 상태
#define HAL_RX_RAW_SIZE 256
static uint8_t hal_raw_buffer[HAL_RX_RAW_SIZE];
static uint32_t hal_raw_index = 0;

// 파싱 완료된 패킷 (Consumer용)
static uint8_t hal_pkt_payload[256];
static uint32_t hal_pkt_len = 0;
static uint16_t hal_pkt_cmd = 0;
static uint32_t hal_pkt_read_pos = 0; // HAL_read가 읽은 위치
static bool hal_pkt_ready = false;

// -----------------------------------------------------------------
// [2] 초기화 함수
// -----------------------------------------------------------------
void HAL_init() {
  Serial.begin(9600);
  pinMode(13, OUTPUT);
  
  hal_raw_index = 0;
  hal_pkt_ready = false;

  // SD카드 초기화
  if (!sd.begin(SD_CONFIG)) {
    // 패킷 시스템 초기화 전이라 그냥 보냄 (또는 에러 패킷 전송 시도)
    // Serial.println("SD Init Failed!"); 
    // HAL_write는 아직 초기화 전이라 위험할 수 있지만 시도해봄
    HAL_write(FD_STDERR, "SD Init Failed!\n");
  }
}

// -----------------------------------------------------------------
// [3] 타이머 및 인터럽트
// -----------------------------------------------------------------
void HAL_setupTimer() {
  noInterrupts();
  TCCR1A = 0; TCCR1B = 0; TCNT1 = 0;
  OCR1A = 249; // 1ms
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS11) | (1 << CS10);
  TIMSK1 |= (1 << OCIE1A);
  interrupts();
}

// 심장 박동 (ISR)
ISR(TIMER1_COMPA_vect) {
  system_ticks++;
  // LED 깜빡임 제거 (SD카드 충돌 방지)
}

// -----------------------------------------------------------------
// [4] 표준 입출력 구현 (StreamProtocol 적용)
// -----------------------------------------------------------------

// 내부 헬퍼: 패킷 전송
static void send_packet(uint16_t cmd, const char* payload) {
    uint32_t payload_len = strlen(payload);
    uint32_t packet_len = 0;
    
    // PT_STRING = 1
    sp_result_t res = sp_encode_packet_buffer(
        (const uint8_t*)payload, payload_len, 
        SP_UNFRAGED, PT_STRING, cmd, 
        hal_tx_buffer, sizeof(hal_tx_buffer), &packet_len
    );

    if (res == SP_OK) {
        Serial.write(hal_tx_buffer, packet_len);
        Serial.flush(); // 즉시 전송 보장
    }
}

// [쓰기] 문자열 출력
void HAL_write(int fd, const char* text) {
  uint16_t cmd = (fd == FD_STDERR) ? CMD_STDERR : CMD_STDOUT;
  send_packet(cmd, text);
}

// [쓰기] 숫자 출력
void HAL_write(int fd, int num) {
  char buf[16];
  itoa(num, buf, 10); // 정수 -> 문자열 변환
  
  uint16_t cmd = (fd == FD_STDERR) ? CMD_STDERR : CMD_STDOUT;
  send_packet(cmd, buf);
}

// [쓰기] 문자 하나 출력
void HAL_writeChar(int fd, char c) {
  char buf[2];
  buf[0] = c;
  buf[1] = '\0';
  
  uint16_t cmd = (fd == FD_STDERR) ? CMD_STDERR : CMD_STDOUT;
  send_packet(cmd, buf);
}

// --- 내부: 시리얼 처리 및 패킷 파싱 ---
static void process_serial() {
    // 패킷이 이미 준비되어 있고 아직 소비되지 않았으면 더 읽지 않음 (Flow Control)
    // (만약 덮어쓰고 싶다면 이 조건 제거)
    // 여기서는 HAL_readPacket이나 HAL_read가 다 읽을 때까지 기다림.
    if (hal_pkt_ready) return;

    while (Serial.available() > 0) {
        int b = Serial.read();
        if (b < 0) break;

        if (hal_raw_index >= HAL_RX_RAW_SIZE) hal_raw_index = 0; // Overflow Reset
        hal_raw_buffer[hal_raw_index++] = (uint8_t)b;

        if (hal_raw_index >= SP_HEADER_SIZE + 4) {
            sp_parsed_packet_t packet;
            sp_result_t res = sp_parse_packet(hal_raw_buffer, hal_raw_index, &packet);

            if (res == SP_OK) {
                // 패킷 완성!
                hal_pkt_cmd = packet.user_field;
                hal_pkt_len = packet.payload_length;
                if (hal_pkt_len > 255) hal_pkt_len = 255; // Cap to buffer size

                memcpy(hal_pkt_payload, packet.payload, hal_pkt_len);
                hal_pkt_read_pos = 0;
                hal_pkt_ready = true;

                // 버퍼 정리 (Sticky Packet)
                uint32_t consumed = (uint32_t)packet.packet_length;
                if (hal_raw_index > consumed) {
                    uint32_t remaining = hal_raw_index - consumed;
                    memmove(hal_raw_buffer, hal_raw_buffer + consumed, remaining);
                    hal_raw_index = remaining;
                } else {
                    hal_raw_index = 0;
                }
                
                return; // 패킷 하나 완성되면 리턴 (처리 기회 제공)
            } else if (res == SP_ERR_BUFFER_TOO_SMALL) {
                // 더 읽어야 함
            } else {
                // 에러 -> 리셋
                hal_raw_index = 0;
            }
        }
    }
}

// [읽기] 입력 (패킷의 Payload를 1바이트씩 제공)
// VM용 (쉘 등)
int HAL_read(int fd) {
  if (fd == FD_STDIN) {
      process_serial(); // 데이터 갱신

      if (hal_pkt_ready) {
          if (hal_pkt_read_pos < hal_pkt_len) {
              return hal_pkt_payload[hal_pkt_read_pos++];
          } else {
              // 다 읽었으면 패킷 소비 완료 처리
              hal_pkt_ready = false;
              // 만약 Payload가 비어있는 패킷이었다면? -> 다음 패킷 대기
              // 여기선 -1 리턴해서 루프 돌게 함
          }
      }
  }
  return -1; // 데이터 없음
}

// [신규] 통신 데몬용 패킷 통째로 읽기
int HAL_readPacket(uint16_t* cmd_out, uint8_t* payload_out, uint32_t max_len) {
    process_serial(); // 데이터 갱신

    if (hal_pkt_ready) {
        *cmd_out = hal_pkt_cmd;
        
        uint32_t len = hal_pkt_len;
        if (len > max_len) len = max_len;
        
        if (len > 0) memcpy(payload_out, hal_pkt_payload, len);
        
        // 소비 완료 처리
        hal_pkt_ready = false;
        hal_pkt_read_pos = 0;
        
        return (int)len;
    }
    return -1; // 패킷 없음
}

// (구) 호환성 유지용 (더미)
void HAL_pushInput(const uint8_t* data, uint32_t len) {}
