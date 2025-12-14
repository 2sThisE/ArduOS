#ifndef OS_CONFIG_H
#define OS_CONFIG_H

#include <Arduino.h>

// --- 시스템 설정 ---
#define TASK_COUNT 1                // 동시 실행 프로그램 수
#define CODE_BUFFER_SIZE 32         // 코드 스트리밍 탄창 크기
#define VM_STACK_SIZE 128           // VM 스택 크기 (확장됨)
#define GLOBAL_HEAP_SIZE 1024       // 공유 힙 int 1024개
#define DEFAULT_TASK_HEAP_SIZE 256  // [수정] 태스크당 기본 할당 힙 크기 (int 단위)

// --- 표준 스트림 ID ---
#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2

// --- 명령어표 (Opcode) ---
enum VMOpcode : uint8_t {
  OP_EXIT   = 0x00,
  
  // 입출력
  OP_PRINT  = 0x01, // 숫자 출력
  OP_READ   = 0x02, // 입력
  OP_PRTC   = 0x03, // 문자 출력
  OP_PRTE   = 0x04, // 에러 출력
  OP_PRTS   = 0x05, // [신규] 문자열 출력 (NULL 종료)

  // 연산
  OP_PUSH   = 0x10,
  OP_ADD    = 0x11,
  OP_SUB    = 0x12,
  OP_EQ     = 0x13,
  OP_DUP    = 0x14, // [신규] 복제
  OP_POP    = 0x15,

  // 제어
  OP_JMP    = 0x20,
  OP_JIF    = 0x21,
  
  // 시스템
  OP_SYS    = 0x30,
  
  // 하드웨어 제어
  OP_PIN_MODE = 0x40,
  OP_D_WRITE  = 0x41,
  OP_SLEEP    = 0x42,  // [신규] 수면

  //동적 메모리 관리
  OP_MALLOC = 0x50, // 메모리 할당 요청
  OP_LOAD   = 0x51, // 힙에서 읽기 (주소 기반)
  OP_STORE  = 0x52  // 힙에 쓰기 (주소 기반)
};

#endif