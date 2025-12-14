#ifndef SYS_GETCWD_H
#define SYS_GETCWD_H

#include "Kernel.h"
#include "HAL.h"

// [SysCall 4] getcwd - 현재 작업 디렉터리 경로 반환
// Stack Args: [BufferAddr] (Pop: BufferAddr)
inline void Syscall_getcwd(Task* t) {
  int buf_addr = t->stack[t->sp--];
  
  if (buf_addr == 0) return; // 버퍼 없으면 무시

  int phys_buf_addr = Kernel_getPhysAddr(t, buf_addr);
  int len = strlen(t->cwd);

  for(int i=0; i < len; i++) {
      if (phys_buf_addr + i < GLOBAL_HEAP_SIZE) {
          global_heap[phys_buf_addr + i] = t->cwd[i];
      }
  }
  // NULL Terminate
  if (phys_buf_addr + len < GLOBAL_HEAP_SIZE) {
      global_heap[phys_buf_addr + len] = 0;
  }
}

#endif
