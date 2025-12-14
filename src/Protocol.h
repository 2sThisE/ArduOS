#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// --- Payload Types (4-bit: 0~15) ---
#define PT_NONE     0x00 // 데이터 없음
#define PT_STRING   0x01 // NULL 종료 문자열 (UTF-8)
#define PT_BYTES    0x02 // 바이너리 데이터
#define PT_U8       0x03 // 1바이트 정수 (uint8_t)
#define PT_U16      0x04 // 2바이트 정수 (uint16_t, LE)
#define PT_U32      0x05 // 4바이트 정수 (uint32_t, LE)
#define PT_I32      0x06 // 4바이트 정수 (int32_t, LE)

// --- System Call / Command IDs (10-bit User Field) ---
// [0 ~ 99] ArduOS System Calls (Kernel_systemCall ID와 일치)
#define SYS_LS          1
#define SYS_EXEC        2
#define SYS_CHDIR       3
#define SYS_GETCWD      4

// [100 ~ ] Special Commands
#define CMD_STDIN       100 // PC -> Arduino: 키보드 입력
#define CMD_STDOUT      101 // Arduino -> PC: 화면 출력
#define CMD_STDERR      102 // Arduino -> PC: 에러 출력
#define CMD_PING        200 // 생존 확인
#define CMD_PONG        201 // 응답

#endif // PROTOCOL_H
