#ifndef TASK_H
#define TASK_H

#include "OSConfig.h"
#include <SdFat.h>

#define MAX_ALLOCATIONS 4 

// --- 상태 상수 정의 (1바이트 최적화) ---
// -2: FREE, -1: RUNNING, 0+: PAUSED (Waiting for Child ID)
#define TASK_FREE     -2
#define TASK_RUNNING  -1

// --- Task Control Block (TCB) ---
struct Task {
  int id;                 
  
  // [수정] bool is_active 대신 상태 변수 사용
  int8_t task_state;       

  char filename[128];                              // 파일명 보관 (확장됨)
  unsigned long wake_up_time;  
  char args[32];                                  //인자값
  char cwd[32];                                   //작업 디렉토리 기본은 루트

  // 실행 상태
  int stack[VM_STACK_SIZE]; 
  int sp;                 
  // int fp; // (현재 미사용)

  // [가상 메모리 정보]
  int heap_base;  // 실제 물리 메모리 시작 주소 (Global Heap Index)
  int heap_limit; // 할당된 힙 크기 (Limit)

  struct {
    int ptr;  // 시작 주소 (인덱스)
    int size; // 크기
  } alloc_table[MAX_ALLOCATIONS];
  
  // 코드 스트리밍
  File32 file;            
  uint8_t code_buffer[CODE_BUFFER_SIZE]; 
  int buffer_index;       

  // --- [Helper Methods] ---

  // 1. 활성 여부 확인 (기존 is_active 대체)
  bool isActive() const {
    return task_state != TASK_FREE;
  }

  // 2. 실행 가능 상태인지 확인 (스케줄러용)
  bool isRunnable() const {
    return task_state == TASK_RUNNING;
  }

  // 3. 부모 ID 가져오기 (0 이상이면 PAUSED 상태)
  int getWaitingFor() const {
    return (task_state >= 0) ? task_state : -1;
  }

  // 4. 상태 설정 (자식 기다리기)
  void waitForChild(int child_id) {
    task_state = (int8_t)child_id; // 해당 자식 ID를 저장하며 PAUSED 상태로 전환
  }

  // 5. 상태 설정 (깨우기/실행)
  void setRunning() {
    task_state = TASK_RUNNING;
  }

  // 6. 상태 설정 (종료/해제)
  void setFree() {
    task_state = TASK_FREE;
  }
};

#endif