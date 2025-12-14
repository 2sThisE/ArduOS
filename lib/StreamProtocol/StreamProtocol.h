#ifndef STREAM_PROTOCOL_H
#define STREAM_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 헤더/플래그 상수 */
#define SP_HEADER_SIZE 8U
#define SP_FRAGED      0x01U
#define SP_UNFRAGED    0x00U

/* 에러 코드 */
typedef enum sp_result_e {
    SP_OK = 0,
    SP_ERR_BUFFER_TOO_SMALL,
    SP_ERR_PAYLOAD_TOO_LARGE,
    SP_ERR_INVALID_ARGUMENT,
    SP_ERR_LENGTH_MISMATCH,
    SP_ERR_CRC_MISMATCH
} sp_result_t;

/* 파싱된 패킷 정보 */
typedef struct sp_parsed_packet_s {
    uint8_t  protocol_version;
    uint64_t packet_length;     /* 헤더 + 페이로드 + CRC */
    uint8_t  fragment_flag;
    uint8_t  payload_type;
    uint16_t user_field;
    const uint8_t* payload;     /* 페이로드 시작 포인터 (입력 버퍼 내부를 가리킴) */
    uint32_t payload_length;    /* [수정] size_t -> uint32_t (AVR 호환성 및 오버플로우 방지) */
} sp_parsed_packet_t;

/**
 * [수정됨] 페이로드를 패킷으로 인코딩하여 **제공된 버퍼**에 씁니다. (No malloc)
 *
 * @param payload         페이로드 바이트
 * @param payload_length  페이로드 길이 (uint32_t)
 * @param frag_flag       SP_FRAGED 또는 SP_UNFRAGED
 * @param payload_type    0~15 (4비트)
 * @param user_field      0~1023 (10비트)
 * @param out_buffer      결과를 쓸 버퍼
 * @param out_buffer_cap  버퍼의 최대 크기 (Bytes)
 * @param out_length      실제 쓰여진 패킷 길이가 저장될 포인터
 * @return                sp_result_t 코드
 */
sp_result_t sp_encode_packet_buffer(const uint8_t* payload,
                                    uint32_t payload_length,
                                    uint8_t frag_flag,
                                    uint8_t payload_type,
                                    uint16_t user_field,
                                    uint8_t* out_buffer,
                                    uint32_t out_buffer_cap,
                                    uint32_t* out_length);

/**
 * 인코딩된 패킷을 파싱합니다.
 *
 * @param packet       패킷 바이트 배열 (헤더 + 페이로드 + CRC)
 * @param packet_len   패킷 길이 (uint32_t)
 * @param out_packet   결과를 채울 구조체 포인터
 *                     out_packet->payload 는 입력 packet 내부를 가리킵니다.
 * @return             sp_result_t 코드
 */
sp_result_t sp_parse_packet(const uint8_t* packet,
                            uint32_t packet_len,
                            sp_parsed_packet_t* out_packet);

#ifdef __cplusplus
}
#endif

#endif // STREAM_PROTOCOL_H
