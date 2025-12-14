#ifndef SYSCALL_LS_H
#define SYSCALL_LS_H

#include "Kernel.h"
#include "HAL.h"

// [SysCall 1] ls - 특정 디렉터리 내용을 힙 버퍼에 저장
// Stack Args: [TargetDirPathSelector, BufferSize, BufferAddr]
inline void Syscall_ls(Task* t) {
  // 0. 인자 가져오기 (Stack LIFO: Push 순서의 역순으로 Pop)
  // Push: Addr -> Size -> Selector(Top)
  int target_dir_path_selector = t->stack[t->sp--]; // Pop Target Path Selector
  int buf_size = t->stack[t->sp--]; // Pop Size
  int buf_addr = t->stack[t->sp--]; // Pop Addr

  // [경로 결정]
  char target_path[128]; // 최종 절대 경로
  memset(target_path, 0, sizeof(target_path));

  if (target_dir_path_selector == 0) { // 0이면 현재 CWD 사용
      strcpy(target_path, t->cwd);
  } else if (target_dir_path_selector == 1) { // 1이면 t->args 사용 (상대 경로일 수 있음 -> resolve_path)
      if (strlen(t->args) > 0) {
          resolve_path(t, t->args, target_path);
      } else { // t->args도 비어있으면 CWD 사용
          strcpy(target_path, t->cwd);
      }
  } else { // 그 외의 값은 오류 처리 (CWD fallback)
      strcpy(target_path, t->cwd);
  }

  // [주소 변환] 커널도 태스크의 가상 주소를 물리 주소로 바꿔야 함
  int phys_addr = Kernel_getPhysAddr(t, buf_addr);
  buf_addr = phys_addr; // (변수 재활용)

  // 1. 현재 실행 중인 파일 위치 저장 (파일 태스크일 경우만)
  uint32_t current_pos = 0;
  bool is_file_task = t->file.isOpen();
  
  if (is_file_task) {
      current_pos = t->file.curPosition();
      t->file.close();
  }

  // 2. 디렉터리 열기 (resolve_path로 변환된 절대 경로 사용)
  File32 dir = sd.open(target_path);
  int written_len = 0;

  if (dir) {
    // 버퍼 초기화
    for(int i=0; i<buf_size; i++) {
        if (buf_addr + i < GLOBAL_HEAP_SIZE) global_heap[buf_addr + i] = 0;
    }

    // 파일 목록 읽기
    for (;;) {
      File32 entry = dir.openNextFile();
      if (!entry) break;

      char name[32];
      memset(name, 0, sizeof(name));
      entry.getName(name, sizeof(name));
      
      // 디렉토리면 '/' 추가
      if (entry.isDirectory()) {
        int len = strlen(name);
        if (len < 30) { name[len] = '/'; name[len+1] = 0; }
      }
      
      // 힙에 쓰기 (한 글자씩 int형으로 저장 - 현재 힙 구조상)
      for (int k = 0; name[k] != 0; k++) {
        if (written_len < buf_size - 1) { // NULL 공간 남겨둠
           if (buf_addr + written_len < GLOBAL_HEAP_SIZE) {
             global_heap[buf_addr + written_len] = (int)name[k];
             written_len++;
           }
        }
      }
      
      // 개행 문자 추가
      if (written_len < buf_size - 1) {
       if (buf_addr + written_len < GLOBAL_HEAP_SIZE) {
         global_heap[buf_addr + written_len] = '\n';
         written_len++;
       }
      }

      entry.close();
    }
    dir.close();
    
    // NULL Terminate
    if (buf_addr + written_len < GLOBAL_HEAP_SIZE) {
        global_heap[buf_addr + written_len] = 0;
    }

  } else {
    // [수정] 모든 태스크에 대해 에러 플래그(-1) 반환 (죽이지 않음)
    if (buf_addr < GLOBAL_HEAP_SIZE) global_heap[buf_addr] = -1;
    return; 
  }

  // 3. 파일 다시 열고 위치 복구 (Resume execution)
  if (is_file_task) {
      t->file = sd.open(t->filename, FILE_READ);
      
      if (t->file) { // 열기 성공?
        t->file.seek(current_pos);
      } else { // 열기 실패?
        t->setFree();
      }
  }
}
#endif

