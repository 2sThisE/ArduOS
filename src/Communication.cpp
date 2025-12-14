#include "Communication.h"
#include "Protocol.h"
#include <StreamProtocol.h>
#include "HAL.h" // Serial 사용

// --- 버퍼 정의 ---
#define RX_BUFFER_SIZE 256
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint32_t rx_index = 0;

// (Tx 버퍼는 필요할 때 생성하거나 여기서 관리)

void Comm_init() {
    memset(rx_buffer, 0, RX_BUFFER_SIZE);
    rx_index = 0;
}

// --- 헬퍼: 힙에 문자열 복사 ---
// 0번 태스크의 힙(가상 주소 1)에 데이터를 복사하고 길이를 반환
// [수정] 가상 주소 0은 NULL로 간주될 수 있으므로 1번지부터 사용
int Comm_copyToHeap(Task* t, const uint8_t* data, uint32_t len) {
    // 0번 태스크는 heap_base가 설정되어 있어야 함 (Kernel_init에서 수행됨)
    if (t->heap_base == -1) return -1;
    
    // 안전장치
    int max_len = t->heap_limit * sizeof(int);
    if ((int)len >= max_len - 1) len = max_len - 2; // offset 1 고려

    // 복사 (offset 1)
    int phys_base = t->heap_base + 1;
    for(uint32_t i=0; i<len; i++) {
        if (phys_base + i >= GLOBAL_HEAP_SIZE) {
             HAL_write(FD_STDERR, "[DEBUG] Error: Comm_copyToHeap Overflow!\n");
             break;
        }
        global_heap[phys_base + i] = data[i];
    }
    // NULL Terminate
    if (phys_base + len < GLOBAL_HEAP_SIZE) {
        global_heap[phys_base + len] = 0;
    }
    
    return 1; // 가상 주소 1번지 리턴
}

// [후처리 헬퍼] 힙에서 문자열 읽어서 전송
void Comm_sendHeapString(Task* t, int offset, int max_len) {
    if (t->heap_base == -1) return;
    
    int phys_base = t->heap_base + offset;
    char buf[128]; // 임시 버퍼 (스택)
    int i = 0;
    
    // int 힙 -> char 변환
    while (i < max_len && i < 127) { // 127 to leave room for NULL
        if (phys_base + i >= GLOBAL_HEAP_SIZE) break;
        int val = global_heap[phys_base + i];
        if (val == 0) break; // NULL 종료
        buf[i] = (char)val;
        i++;
    }
    buf[i] = '\0';
    
    if (i > 0) {
        HAL_write(FD_STDOUT, buf);
    }
}

void Comm_process(Task* t) {
  // 1. HAL에서 패킷 읽기 (비동기)
  // rx_buffer를 Payload 버퍼로 재사용
  uint16_t cmd_id = 0;
  int payload_len = HAL_readPacket(&cmd_id, rx_buffer, RX_BUFFER_SIZE);

  if (payload_len >= 0) {
      

      // [A] 시스템 콜 처리
      if (cmd_id < 100) {
          // 공통: 인자(Payload)가 있다면 힙에 복사 (주소 1 리턴)
          int heap_data_addr = 0;
          if (payload_len > 0) {
              heap_data_addr = Comm_copyToHeap(t, rx_buffer, payload_len);
              
          } else {
              if(t->heap_base != -1) global_heap[t->heap_base + 1] = 0;
              heap_data_addr = 1;
          }

          // 스택 초기화
          t->sp = -1;

          if (cmd_id == SYS_LS) {
                  // [수정] 스택 Push 순서 변경 (Syscall_ls의 Pop 순서에 맞춤)
                  // Syscall_ls Pop 순서: Addr -> Size -> Selector
                  // 따라서 Push 순서: Selector -> Size -> Addr (LIFO)
                  
                  int selector = 0;
                  // [수정] 인자(Path) 처리
                  if (payload_len > 0) {
                      // Payload를 t->args에 복사 (최대 31자)
                      uint32_t len = payload_len;
                      if (len > 31) len = 31;
                      
                      // memcpy 대신 루프 사용
                      for(uint32_t i=0; i<len; i++) t->args[i] = rx_buffer[i];
                      t->args[len] = 0; // NULL Terminate
                      
                      selector = 1;   // TargetSelector (1=Args)
                  } else {
                      selector = 0;   // TargetSelector (0=CWD)
                  }

                  t->stack[++t->sp] = 0;        // Buffer Addr (먼저 Push -> 나중에 Pop)
                  t->stack[++t->sp] = 128;      // Buffer Size
                  t->stack[++t->sp] = selector; // Selector (나중에 Push -> 먼저 Pop)
                  
                  Kernel_systemCall(t, SYS_LS);
                  
                  // [수정] 결과 확인 (에러 코드 -1 체크)
                  if (t->heap_base != -1 && global_heap[t->heap_base + 0] == -1) {
                      HAL_write(FD_STDERR, "Error: ls failed (Invalid directory)\n");
                  } else {
                      // [후처리] 결과 전송
                      Comm_sendHeapString(t, 0, 128);
                  }

          } else if (cmd_id == SYS_EXEC) {
                  int wait_opt = 0;
                  int cmd_addr = 1; // Heap 1
                  int arg_addr = 0; // NULL
                  
                  // 1. 첫 번째 공백 찾기 (WaitOption 구분)
                  int first_space = -1;
                  for(int i=0; i<payload_len; i++) {
                      if (rx_buffer[i] == ' ') {
                          first_space = i;
                          break;
                      }
                  }
                  
                  if (first_space != -1) {
                      // WaitOption 파싱
                      if (rx_buffer[0] == '1') wait_opt = 1;
                      else wait_opt = 0;
                      
                      // 2. 두 번째 공백 찾기
                      int second_space = -1;
                      for(int i=first_space+1; i<payload_len; i++) {
                          if (rx_buffer[i] == ' ') {
                              second_space = i;
                              break;
                          }
                      }
                      
                      if (second_space == -1) {
                          // Arg 없음
                          int cmd_len = payload_len - (first_space + 1);
                          if (cmd_len > 0 && t->heap_base != -1) {
                              int phys_base = t->heap_base + 1;
                              for(int i=0; i<cmd_len; i++) {
                                  if (phys_base + i >= GLOBAL_HEAP_SIZE) break;
                                  global_heap[phys_base + i] = rx_buffer[first_space + 1 + i];
                              }
                              if (phys_base + cmd_len < GLOBAL_HEAP_SIZE) global_heap[phys_base + cmd_len] = 0;
                          }
                      } else {
                          // Arg 있음
                          int cmd_len = second_space - (first_space + 1);
                          if (cmd_len > 0 && t->heap_base != -1) {
                              int phys_base = t->heap_base + 1;
                              for(int i=0; i<cmd_len; i++) {
                                  if (phys_base + i >= GLOBAL_HEAP_SIZE) break;
                                  global_heap[phys_base + i] = rx_buffer[first_space + 1 + i];
                              }
                              if (phys_base + cmd_len < GLOBAL_HEAP_SIZE) global_heap[phys_base + cmd_len] = 0;
                          }
                          
                          int arg_len = payload_len - (second_space + 1);
                          if (arg_len > 0 && t->heap_base != -1) {
                              int phys_base = t->heap_base + 64; // Heap 64
                              for(int i=0; i<arg_len; i++) {
                                  if (phys_base + i >= GLOBAL_HEAP_SIZE) break;
                                  global_heap[phys_base + i] = rx_buffer[second_space + 1 + i];
                              }
                              if (phys_base + arg_len < GLOBAL_HEAP_SIZE) global_heap[phys_base + arg_len] = 0;
                              arg_addr = 64;
                          }
                      }
                  } else {
                      // 공백 없음 (형식 오류) -> Fallback
                      Comm_copyToHeap(t, rx_buffer, payload_len);
                  }

                  t->stack[++t->sp] = arg_addr; 
                  t->stack[++t->sp] = cmd_addr; 
                  t->stack[++t->sp] = wait_opt; 
                  
                  Kernel_systemCall(t, SYS_EXEC);
                  HAL_write(FD_STDOUT, "Exec started.\n");

          } else if (cmd_id == SYS_CHDIR) {
                  t->stack[++t->sp] = 64; // Buffer Addr (Result)
                  t->stack[++t->sp] = heap_data_addr;  // PathAddr (Input - Heap 1번지)
                  
                  Kernel_systemCall(t, SYS_CHDIR);
                  
                  // [수정] 결과 확인 (에러 코드 -1 체크)
                  // 버퍼 주소는 64번지로 고정되어 있음
                  if (t->heap_base != -1 && global_heap[t->heap_base + 64] == -1) {
                      HAL_write(FD_STDERR, "Error: Invalid directory\n");
                  } else {
                      // [수정] t->cwd를 직접 전송 (불필요한 힙 복사 제거)
                      HAL_write(FD_STDOUT, t->cwd);
                      HAL_write(FD_STDOUT, "\n");
                  }

          } else if (cmd_id == SYS_GETCWD) {
                  t->stack[++t->sp] = 0; // BufferAddr
                  
                  Kernel_systemCall(t, SYS_GETCWD);
                  
                  // [수정] t->cwd를 직접 전송
                  HAL_write(FD_STDOUT, t->cwd);
                  HAL_write(FD_STDOUT, "\n");
          } else {
                  // Unknown SysCall ID
          }
      }
  }
}
