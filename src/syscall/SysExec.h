#ifndef SYS_EXEC_H
#define SYS_EXEC_H

#include "Kernel.h"
#include "HAL.h"

// [SysCall 2] exec - 새 태스크 실행
// Stack Args: [WaitOption, CmdAddr, ArgAddr] (Pop 순서: ArgAddr, CmdAddr, WaitOption)
// WaitOption: 1=Sync(Wait), 0=Async
inline void Syscall_exec(Task* t) {
  // 1. 스택에서 주소 꺼내기
  // 주의: 스택은 LIFO이므로, 나중에 PUSH한 것이 먼저 나옴.
  // Shell.asm에서 PUSH 순서: [ArgAddr] -> [CmdAddr] -> [WaitOption]
  // 따라서 POP 순서: WaitOption -> CmdAddr -> ArgAddr
  int wait_opt = t->stack[t->sp--]; 
  int cmd_addr = t->stack[t->sp--];
  int arg_addr = t->stack[t->sp--];
  
  // [주소 변환] 가상 주소를 물리 주소로 변환
  cmd_addr = Kernel_getPhysAddr(t, cmd_addr);
  if (arg_addr != 0) arg_addr = Kernel_getPhysAddr(t, arg_addr);

  // 2. cmd 문자열 (파일 이름) 읽기
  char cmd_str[32];
  memset(cmd_str, 0, sizeof(cmd_str));
  for (int i = 0; i < 31; i++) {
    if (cmd_addr + i >= GLOBAL_HEAP_SIZE) break;
    int val = global_heap[cmd_addr + i];
    if (val == 0 || val == ' ') break;
    cmd_str[i] = (char)val;
  }

  // 3. arg 문자열 (옵션 인자) 읽기 (주소가 0이 아닐 때만)
  char arg_str[32];
  memset(arg_str, 0, sizeof(arg_str));
  if (arg_addr != 0) {
    for (int i = 0; i < 31; i++) {
      if (arg_addr + i >= GLOBAL_HEAP_SIZE) break;
      int val = global_heap[arg_addr + i];
      if (val == 0) break;
      arg_str[i] = (char)val;
    }
  }

  // 4. 빈 태스크 슬롯 찾기 (0번은 쉘이므로 1번부터)
  int free_slot = -1;
  for (int i = 1; i < TASK_COUNT; i++) {
    // if (!tasks[i].is_active) { -> [수정] isActive() 대신 setFree() 상태 확인
    if (tasks[i].task_state == TASK_FREE) {
      free_slot = i;
      break;
    }
  }

  // 5. 로딩 및 실행
  if (free_slot != -1) {
    // Kernel_loadTask(id, filename, args, parent_cwd, parent_arg_str)
    // [수정] 로딩 성공 여부 확인
    bool success = Kernel_loadTask(free_slot, cmd_str, NULL, t->cwd, (arg_addr != 0) ? (const char*)arg_str : NULL); 
    
    if (success) {
        // [동기화 로직 추가]
        if (wait_opt == 1) {
            // 동기 실행: 부모는 자식을 기다림 (자식 ID를 상태 변수에 저장)
            t->waitForChild(free_slot); 
        } else {
            // 비동기 실행: 부모는 계속 실행 (RUNNING 유지)
            // 자식은 그냥 RUNNING 상태로 독립 실행됨
        }
    } else {
        // 로딩 실패 시: 부모는 계속 실행됨 (에러 메시지는 Kernel_loadTask에서 출력됨)
    }

  } else {
    HAL_write(FD_STDERR, "Error: No free task slots.\n");
  }
}

#endif

