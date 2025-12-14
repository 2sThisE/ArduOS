#include <Arduino.h> // PlatformIO일 경우 필요, 아두이노 IDE면 있어도 상관없음
#include "OSConfig.h"
#include "HAL.h"
#include "Kernel.h"

// -----------------------------------------------------------------
// [ArduOS Main Entry]
// 메인 파일은 이제 단순히 "조립"만 하는 역할입니다.
// -----------------------------------------------------------------

void setup() {
  // 1. 하드웨어(LCD, SD, Serial) 초기화
  HAL_init();
  
  // 부팅 메시지 출력 (표준 출력 사용)
  HAL_write(FD_STDOUT, "ArduOS v2.0 Booting...\n");

  // 2. 커널 초기화 (메모리, 태스크 테이블 등)
  Kernel_init();

  // 3. 쉘(Shell) 프로그램 로딩 -> [삭제] 통신 데몬(Task 0) 사용을 위해 로드하지 않음
  // Kernel_loadTask(0, "shell.bin", NULL, NULL, NULL); // parent_arg_str = NULL
  
  // 테스트용: 알람 등 다른 프로그램을 1번에 올리고 싶으면 주석 해제
  // Kernel_loadTask(1, "alarm.bin");

  // 4. 타이머 인터럽트 시작 (OS 심장 가동)
  HAL_setupTimer();
  
  HAL_write(FD_STDOUT, "System Ready.\n");
}

void loop() {
  // 5. 커널 스케줄러 실행
  // (라운드 로빈으로 태스크들을 번갈아 가며 실행)
  Kernel_runScheduler();
  
  // CPU 점유율 조절 (너무 빠르면 시리얼 통신이 씹힐 수 있음)
  // delay(1); 
}