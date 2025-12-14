#ifndef SYS_CHDIR_H
#define SYS_CHDIR_H

#include "Kernel.h"
#include "HAL.h"

// [SysCall 3] cd (디렉터리 이동)
// Stack Args: [BufferAddr, PathAddr] (Pop: PathAddr, BufferAddr)
inline void Syscall_chdir(Task* t) {
  // 1. 인자 가져오기
  int arg_addr = t->stack[t->sp--];
  int buf_addr = t->stack[t->sp--]; // [신규] 결과 버퍼 주소

  if (arg_addr == 0) {
    // HAL_write(FD_STDERR, "Usage: cd <path>\n");
    return;
  }

  // 2. 힙에서 경로 문자열 읽기
  char path_str[32]; 
  memset(path_str, 0, 32);
  
  // 가상 -> 물리 변환
  int phys_arg_addr = Kernel_getPhysAddr(t, arg_addr);

  for (int i = 0; i < 31; i++) {
    if (phys_arg_addr + i >= GLOBAL_HEAP_SIZE) break;
    int val = global_heap[phys_arg_addr + i];
    if (val == 0) break;
    path_str[i] = (char)val;
  }
  
  // ---------------------------------------------------------
  // [로직] 경로 계산 ('..' 처리)
  // ---------------------------------------------------------
  char target_path[64];
  memset(target_path, 0, 64);

  // Case A: ".." (상위 폴더 이동)
  if (strcmp(path_str, "..") == 0) {
      // 현재 위치(cwd) 복사
      strcpy(target_path, t->cwd);
      
      // 1. 맨 뒤에 슬래시('/')가 있으면 제거 (예: "/bin/" -> "/bin")
      int len = strlen(target_path);
      if (len > 1 && target_path[len-1] == '/') {
          target_path[len-1] = '\0';
      }

      // 2. 뒤에서부터 슬래시 찾기
      char* last_slash = strrchr(target_path, '/');
      
      if (last_slash != NULL) {
          if (last_slash == target_path) {
              // 슬래시가 맨 앞이면 루트임 (예: "/bin" -> "/")
              strcpy(target_path, "/");
          } else {
              // 그 외엔 슬래시 뒤를 잘라냄 (예: "/usr/bin" -> "/usr")
              *last_slash = '\0';
          }
      } else {
          // 슬래시가 없으면? 그냥 루트로
          strcpy(target_path, "/");
      }
  }
  // Case B: "." (현재 폴더) -> 아무것도 안 함
  else if (strcmp(path_str, ".") == 0) {
      // 그래도 현재 경로 복사는 해줘야 함
      strcpy(target_path, t->cwd);
  }
  // Case C: 일반 경로 이동
  else {
      resolve_path(t, path_str, target_path); 
  }

  // ---------------------------------------------------------
  // [이동 실행]
  // ---------------------------------------------------------
  // 실제 디렉터리가 존재하는지 확인 (SD 라이브러리 사용)
  // 주의: target_path가 "/"인 경우는 항상 성공
  bool success = false;
  if (strcmp(target_path, "/") == 0) {
      success = true;
  } else {
      File32 dir = sd.open(target_path);
      if (dir && dir.isDirectory()) {
          success = true;
          dir.close();
      }
  }

  if (success) {
      // 성공! cwd 갱신
      memset(t->cwd, 0, 32);
      strncpy(t->cwd, target_path, 31);
      
      // cwd가 '/'로 끝나지 않고 루트도 아니면 뒤에 '/' 붙여줌 (보기 좋게)
      int len = strlen(t->cwd);
      if (len > 1 && t->cwd[len-1] != '/') {
          strcat(t->cwd, "/");
      }
      
      // [신규] 변경된 경로를 사용자 버퍼에 복사 (피드백)
      if (buf_addr != 0) {
          int phys_buf_addr = Kernel_getPhysAddr(t, buf_addr);
          int new_len = strlen(t->cwd);
          for(int i=0; i < new_len; i++) {
              if (phys_buf_addr + i < GLOBAL_HEAP_SIZE) {
                  global_heap[phys_buf_addr + i] = t->cwd[i];
              }
          }
          // NULL Terminate
          if (phys_buf_addr + new_len < GLOBAL_HEAP_SIZE) {
              global_heap[phys_buf_addr + new_len] = 0;
          }
      }

  } else {
      // 실패 시 결과 버퍼에 -1 기록
      if (buf_addr != 0) {
           int phys_buf = Kernel_getPhysAddr(t, buf_addr);
           if (phys_buf < GLOBAL_HEAP_SIZE) global_heap[phys_buf] = -1;
      }
  }
}

#endif