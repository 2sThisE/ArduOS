#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdint.h>
#include "Kernel.h" // Task 구조체를 알기 위해

// --- 통신 모듈 초기화 ---
void Comm_init();

// --- 통신 처리 (스케줄러나 태스크에서 호출) ---
// Task* t는 "시스템 콜 대리 호출"을 위한 주체 태스크
void Comm_process(Task* t);

#endif
