#include "StreamProtocol.h"

/* 45비트 길이 필드의 최대 값 */
#define SP_MAX_HEADER_LENGTH_VALUE 0x1FFFFFFFFFFFULL

/* 내부 CRC32 구현 (Java/C++ 버전과 동일 폴리노미얼) */
static uint32_t sp_crc32(const uint8_t* data, uint32_t length) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            uint32_t mask = 0UL - (crc & 1UL);
            crc = (crc >> 1) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}

sp_result_t sp_encode_packet_buffer(const uint8_t* payload,
                                    uint32_t payload_length,
                                    uint8_t frag_flag,
                                    uint8_t payload_type,
                                    uint16_t user_field,
                                    uint8_t* out_buffer,
                                    uint32_t out_buffer_cap,
                                    uint32_t* out_length) {
    
    if (!out_buffer || !out_length) {
        return SP_ERR_INVALID_ARGUMENT;
    }

    if (payload == NULL && payload_length > 0) {
        return SP_ERR_INVALID_ARGUMENT;
    }

    /* fragment flag 검증 */
    if (frag_flag != SP_FRAGED && frag_flag != SP_UNFRAGED) {
        return SP_ERR_INVALID_ARGUMENT;
    }

    /* payload type 검증 (4비트) */
    if (payload_type > 0x0F) {
        return SP_ERR_INVALID_ARGUMENT;
    }

    /* user field 검증 (10비트) */
    if (user_field > 0x3FF) {
        return SP_ERR_INVALID_ARGUMENT;
    }

    /* 패킷 전체 길이 계산 */
    uint64_t total_len_64 = (uint64_t)SP_HEADER_SIZE + (uint64_t)payload_length + 4ULL;

    /* 버퍼 크기 체크 */
    if (total_len_64 > (uint64_t)out_buffer_cap) {
        return SP_ERR_BUFFER_TOO_SMALL;
    }

    /* 프로토콜 한계 체크 */
    if (total_len_64 > SP_MAX_HEADER_LENGTH_VALUE) {
        return SP_ERR_PAYLOAD_TOO_LARGE;
    }

    uint32_t total_len = (uint32_t)total_len_64;

    /* 64비트 헤더 구성 (little-endian) */
    uint8_t protocol_version = 1U;
    uint64_t header_value = 0;
    
    // [중요] 비트 연산 시 ULL 접미사 및 캐스팅 필수!
    header_value |= ((uint64_t)protocol_version & 0x0FULL) << 0;
    header_value |= (total_len_64 & 0x1FFFFFFFFFFFULL) << 4;
    header_value |= ((uint64_t)frag_flag & 0x01ULL) << 49;
    header_value |= ((uint64_t)payload_type & 0x0FULL) << 50;
    header_value |= ((uint64_t)user_field & 0x3FFULL) << 54;

    for (uint32_t i = 0; i < SP_HEADER_SIZE; ++i) {
        out_buffer[i] = (uint8_t)((header_value >> (i * 8)) & 0xFFU);
    }

    /* 페이로드 복사 */
    if (payload_length > 0) {
        for (uint32_t i = 0; i < payload_length; ++i) {
            out_buffer[SP_HEADER_SIZE + i] = payload[i];
        }
    }

    /* CRC 계산 (헤더 + 페이로드) */
    uint32_t crc = sp_crc32(out_buffer, SP_HEADER_SIZE + payload_length);
    uint32_t crc_offset = total_len - 4U;
    
    out_buffer[crc_offset + 0] = (uint8_t)((crc >> 0) & 0xFFU);
    out_buffer[crc_offset + 1] = (uint8_t)((crc >> 8) & 0xFFU);
    out_buffer[crc_offset + 2] = (uint8_t)((crc >> 16) & 0xFFU);
    out_buffer[crc_offset + 3] = (uint8_t)((crc >> 24) & 0xFFU);

    *out_length = total_len;
    return SP_OK;
}

sp_result_t sp_parse_packet(const uint8_t* packet,
                            uint32_t packet_len,
                            sp_parsed_packet_t* out_packet) {
    if (!packet || !out_packet) {
        return SP_ERR_INVALID_ARGUMENT;
    }

    if (packet_len < SP_HEADER_SIZE + 4U) {
        return SP_ERR_BUFFER_TOO_SMALL;
    }

    /* 64비트 헤더 읽기 (little-endian) */
    uint64_t header_value = 0;
    for (uint32_t i = 0; i < SP_HEADER_SIZE; ++i) {
        header_value |= ((uint64_t)packet[i]) << (i * 8);
    }

    uint8_t proto_version = (uint8_t)((header_value >> 0) & 0x0FU);
    uint64_t packet_length64 = (header_value >> 4) & 0x1FFFFFFFFFFFULL;
    uint8_t fragment_flag = (uint8_t)((header_value >> 49) & 0x01U);
    uint8_t payload_type = (uint8_t)((header_value >> 50) & 0x0FU);
    uint16_t user_field = (uint16_t)((header_value >> 54) & 0x3FFU);

    if (packet_length64 < SP_HEADER_SIZE + 4U) {
        return SP_ERR_BUFFER_TOO_SMALL;
    }
    
    /* 길이 불일치 확인 (입력받은 버퍼 크기와 헤더의 길이 비교) */
    // [수정] 스트림 처리를 위해 '부족한지'만 확인하고, 남는 건 허용함.
    if ((uint64_t)packet_len < packet_length64) {
        return SP_ERR_BUFFER_TOO_SMALL;
    }

    uint32_t packet_length = (uint32_t)packet_length64;

    /* CRC 추출 (마지막 4바이트, little-endian) */
    uint32_t crc_offset = packet_length - 4U;
    uint32_t received_crc = 0;
    received_crc |= ((uint32_t)packet[crc_offset + 0]) << 0;
    received_crc |= ((uint32_t)packet[crc_offset + 1]) << 8;
    received_crc |= ((uint32_t)packet[crc_offset + 2]) << 16;
    received_crc |= ((uint32_t)packet[crc_offset + 3]) << 24;

    /* 헤더 + 페이로드에 대한 CRC 계산 */
    uint32_t computed_crc = sp_crc32(packet, packet_length - 4U);
    if (computed_crc != received_crc) {
        return SP_ERR_CRC_MISMATCH;
    }

    /* 결과 구조체 채우기 (payload는 입력 버퍼 내부를 가리킴) */
    out_packet->protocol_version = proto_version;
    out_packet->packet_length = packet_length64;
    out_packet->fragment_flag = fragment_flag;
    out_packet->payload_type = payload_type;
    out_packet->user_field = user_field;
    out_packet->payload = packet + SP_HEADER_SIZE;
    out_packet->payload_length = packet_length - SP_HEADER_SIZE - 4U;

    return SP_OK;
}
