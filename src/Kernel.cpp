#include "Kernel.h"
#include "HAL.h"
#include "VirtualMachine.h"
#include "Communication.h" // [신규] 통신 모듈

// Task table
Task tasks[TASK_COUNT];

// Global heap for VM
int global_heap[GLOBAL_HEAP_SIZE];
uint8_t heap_bitmap[GLOBAL_HEAP_SIZE / 8];

// Function prototypes
int Kernel_malloc(Task* t, int size);

// --- init ---
void Kernel_init() {
  memset(global_heap, 0, sizeof(global_heap));
  memset(heap_bitmap, 0, sizeof(heap_bitmap));
  
  // 통신 초기화
  Comm_init();

  for (int i = 0; i < TASK_COUNT; i++) {
    tasks[i].id = i;
    // tasks[i].is_active = false; -> [수정]
    tasks[i].setFree();
    
    tasks[i].sp = -1;
    tasks[i].wake_up_time = 0;
    
    // 가상 메모리 초기화
    tasks[i].heap_base = -1;
    tasks[i].heap_limit = 0;

    for (int j = 0; j < MAX_ALLOCATIONS; j++) {
      tasks[i].alloc_table[j].ptr = -1;
    }
    strcpy(tasks[i].cwd, "/");
  }

  // [Task 0] 통신 데몬 전용 설정
  // 미리 힙을 조금 할당해둠 (파라미터 전달용)
  // 예: 256 바이트 (int 128개)
  tasks[0].setRunning(); // 항상 실행 상태
  int comm_heap = Kernel_malloc(&tasks[0], 128); // 128 ints = 256 bytes (AVR int=2) -> 512 bytes? 
  // AVR int=2byte, so 128 size = 256 bytes.
  if (comm_heap != -1) {
      tasks[0].heap_base = comm_heap;
      tasks[0].heap_limit = 128;
  }
}

// ... (resolve_path, Kernel_initMemory, Kernel_malloc, Kernel_free, Kernel_loadTask, Kernel_terminateTask, Kernel_yield, Kernel_getPhysAddr implementation remains same) ...
// (Here I will just keep the existing implementations, assuming 'replace' tool context matching works)

void resolve_path(Task* t, const char* input, char* output) {
  // 1. 만약 입력이 '/'로 시작하면 (절대 경로)
  if (input[0] == '/') {
    strcpy(output, input);
  } 
  // 2. 아니면 (상대 경로) -> 현재위치 + 입력
  else {
    strcpy(output, t->cwd);
    // cwd가 '/'로 끝나지 않으면 붙여줌 (안전장치)
    int len = strlen(output);
    if (len > 0 && output[len-1] != '/') {
      strcat(output, "/");
    }
    strcat(output, input);
  }
}

void Kernel_initMemory() {
  memset(heap_bitmap, 0, sizeof(heap_bitmap));
}

// bitmap helpers
bool is_allocated(int idx) {
  return (heap_bitmap[idx / 8] & (1 << (idx % 8)));
}

void set_allocated(int idx, bool allocated) {
  if (allocated) heap_bitmap[idx / 8] |= (1 << (idx % 8));
  else           heap_bitmap[idx / 8] &= ~(1 << (idx % 8));
}

void set_bit(int index) {
  heap_bitmap[index / 8] |= (1 << (index % 8));
}

void clear_bit(int index) {
  heap_bitmap[index / 8] &= ~(1 << (index % 8));
}

// heap alloc (first-fit)
int Kernel_malloc(Task* t, int size) {
  if (size <= 0) return -1;

  int consecutive_free = 0;
  int start_index = -1;

  for (int i = 0; i < GLOBAL_HEAP_SIZE; i++) {
    if (!is_allocated(i)) {
      if (consecutive_free == 0) start_index = i;
      consecutive_free++;

      if (consecutive_free == size) {
        int slot = -1;
        for (int k = 0; k < MAX_ALLOCATIONS; k++) {
          if (t->alloc_table[k].ptr == -1) {
            slot = k;
            break;
          }
        }

        if (slot == -1) {
          HAL_write(FD_STDERR, "Err: Alloc table full\n");
          return -1;
        }

        for (int k = 0; k < size; k++) {
          set_allocated(start_index + k, true);
        }

        t->alloc_table[slot].ptr = start_index;
        t->alloc_table[slot].size = size;

        return start_index;
      }
    } else {
      consecutive_free = 0;
    }
  }

  return -1;
}

// heap free
void Kernel_free(Task* t, int ptr) {
  for (int i = 0; i < MAX_ALLOCATIONS; i++) {
    if (t->alloc_table[i].ptr == ptr) {
      int size = t->alloc_table[i].size;

      for (int k = 0; k < size; k++) {
        set_allocated(ptr + k, false);
      }

      t->alloc_table[i].ptr = -1;
      return;
    }
  }
}

bool Kernel_loadTask(int id, const char* input_name, const char* arg_str, const char* parent_cwd, const char* parent_arg_str) { // parent_arg_str 추가
  Task* t = &tasks[id];
  if (t->file) t->file.close(); 
  
  // 인자 저장
  memset(t->args, 0, 32);
  // (기존 arg_str 처리)
  if (arg_str != NULL) {
      strncpy(t->args, arg_str, 31);
  }
  // [부모 인자 상속] 부모 인자 문자열이 있으면 자식의 t->args를 초기화
  if (parent_arg_str != NULL) {
      strncpy(t->args, parent_arg_str, 31);
  }

  // [CWD 상속] 부모 CWD가 있으면 자식의 CWD를 초기화
  memset(t->cwd, 0, 32);
  if (parent_cwd != NULL) {
      strncpy(t->cwd, parent_cwd, 31);
  } else {
      strcpy(t->cwd, "/"); // 기본값 루트
  }

  // ---------------------------------------------------------
  // [파일 찾기 로직 3단계]
  // ---------------------------------------------------------
  char path_buffer[32]; // 경로 조립용 버퍼
  memset(path_buffer, 0, 32);

  // [시도 1] 입력한 이름 그대로 (예: "my_script.txt")
  strcpy(path_buffer, input_name);
  t->file = sd.open(path_buffer, FILE_READ);

  // [시도 2] .bin 붙이기 (예: "ls" -> "ls.bin")
  if (!t->file) {
    strcpy(path_buffer, input_name);
    strcat(path_buffer, ".bin");
    t->file = sd.open(path_buffer, FILE_READ);
  }

  // [시도 3] /bin/ 폴더 뒤지기 (예: "ls" -> "/bin/ls.bin")
  if (!t->file) {
    strcpy(path_buffer, "/bin/");
    strcat(path_buffer, input_name);
    strcat(path_buffer, ".bin");
    t->file = sd.open(path_buffer, FILE_READ);
  }

  // ---------------------------------------------------------
  // [결과 처리]
  // ---------------------------------------------------------
  if (t->file) {
    // [헤더 읽기 및 힙 크기 결정]
    // 모든 실행 파일은 반드시 헤더(4바이트)를 가져야 함
    uint8_t header[4];
    int size = DEFAULT_TASK_HEAP_SIZE;
    
    if (t->file.read(header, 4) == 4) {
      if (header[0] == 0xAD && header[1] == 0x01) {
        // 헤더 발견! (Magic: 0xAD, Ver: 0x01)
        size = header[2] | (header[3] << 8);
      } else {
        HAL_write(FD_STDERR, "Err: Invalid exec format (Bad Magic)\n");
        t->file.close();
        t->setFree();
        return false;
      }
    } else {
        HAL_write(FD_STDERR, "Err: Invalid exec format (Too short)\n");
        t->file.close();
        t->setFree();
        return false;
    }

    // ---------------------------------------------------------
    // [가상 메모리 할당]
    // 태스크 실행 전에 전용 힙 공간(Segment)을 확보합니다.
    // ---------------------------------------------------------
    int consecutive_free = 0;
    int start_index = -1;
    bool alloc_success = false;

    for (int i = 0; i < GLOBAL_HEAP_SIZE; i++) {
      if (!is_allocated(i)) {
        if (consecutive_free == 0) start_index = i;
        consecutive_free++;
        if (consecutive_free == size) {
          // 찾았다! 할당 수행
          for (int k = 0; k < size; k++) set_allocated(start_index + k, true);
          t->heap_base = start_index;
          t->heap_limit = size;
          alloc_success = true;
          break;
        }
      } else {
        consecutive_free = 0;
      }
    }

    if (!alloc_success) {
        HAL_write(FD_STDERR, "Error: Out of Memory (Cannot alloc task heap)\n");
        t->file.close();
        // t->is_active = false; -> [수정]
        t->setFree();
        return false;
    }

    // 성공! 최종 경로 저장
    memset(t->filename, 0, 128);
    // 파일명이 너무 길면(8.3 제한) 잘릴 수 있으니 주의
    // 여기선 전체 경로 대신 파일명만 남기거나, 필요하면 전체 경로 저장
    // 일단 간단하게 input_name을 저장하거나, 찾은 path_buffer를 저장
    strncpy(t->filename, path_buffer, 127);

    t->buffer_index = 0;
    // 코드 버퍼 읽기 (이미 헤더 4바이트 읽었으므로 포인터는 4에 있음)
    t->file.read(t->code_buffer, CODE_BUFFER_SIZE);
    
    // t->is_active = true; -> [수정]
    t->setRunning();
    
    t->sp = -1;
    t->wake_up_time = 0;

    HAL_write(FD_STDOUT, "\n");
    return true;
  } else {
    HAL_write(FD_STDERR, "Error: Command not found\n"); 
    return false;
  }
}

// scheduler
void Kernel_runScheduler() {
  while(1) {
    for (int i = 0; i < TASK_COUNT; i++) {
        Task* t = &tasks[i];
        
        // 실행 가능한 상태(RUNNING)가 아니면 건너뜀 (PAUSED 포함)
        if (!t->isRunnable()) continue;

        // [Task 0] 통신 데몬 (VM 대신 C++ 코드 실행)
        if (i == 0) {
            Comm_process(t);
            continue;
        }

        if (t->wake_up_time > 0) {
            if (system_ticks < t->wake_up_time) continue;
            else t->wake_up_time = 0;
        }

        VM_runStep(t);
    }
  }
}

// IO bridge
void Kernel_stdWrite(int fd, int val) { HAL_write(fd, val); }
void Kernel_stdWrite(int fd, const char* str) { HAL_write(fd, str); }
void Kernel_stdWriteChar(int fd, char c) { HAL_writeChar(fd, c); }
int  Kernel_stdRead(int fd) { return HAL_read(fd); }

// code buffer helpers
void Kernel_refillBuffer(Task* t) {
  if (t->file.available()) {
    t->file.read(t->code_buffer, CODE_BUFFER_SIZE);
    t->buffer_index = 0;
  } else {
    Kernel_terminateTask(t->id);
  }
}

void Kernel_jump(Task* t, int addr) {
  // [수정] 헤더(4바이트)를 고려하여 오프셋 추가
  t->file.seek(addr + 4);
  t->buffer_index = CODE_BUFFER_SIZE;
}

void Kernel_terminateTask(int id) {
  Task* t = &tasks[id];

  // 1. 추가 할당된 메모리 해제 (alloc_table)
  for (int i = 0; i < MAX_ALLOCATIONS; i++) {
    int ptr = t->alloc_table[i].ptr;
    if (ptr != -1) {
      Kernel_free(t, ptr);
      HAL_write(FD_STDOUT, "GC: Freed addr ");
      HAL_write(FD_STDOUT, ptr);
      HAL_write(FD_STDOUT, "\n");
    }
  }

  // 2. 태스크 기본 힙 해제 (Virtual Memory Segment)
  if (t->heap_base != -1) {
      for(int k=0; k < t->heap_limit; k++) {
          set_allocated(t->heap_base + k, false);
      }
      // HAL_write(FD_STDOUT, "GC: Freed Task Heap Base ");
      // HAL_write(FD_STDOUT, t->heap_base);
      // HAL_write(FD_STDOUT, "\n");
      t->heap_base = -1;
      t->heap_limit = 0;
  }

  // [수정] 종료 전, 나를 기다리는 부모가 있다면 깨워준다!
  for (int i = 0; i < TASK_COUNT; i++) {
    if (tasks[i].getWaitingFor() == id) {
      tasks[i].setRunning(); // 부모 깨움
    }
  }

  // t->is_active = false; -> [수정]
  t->setFree();
  t->file.close();
  // HAL_write(FD_STDOUT, "Task Exit.\n"); // 출력 제거
}

void Kernel_yield(Task* t) {
  t->wake_up_time = system_ticks + t->wake_up_time;
}

// 가상 주소 -> 물리 주소 변환
int Kernel_getPhysAddr(Task* t, int virt_addr) {
  if (virt_addr >= 0 && virt_addr < t->heap_limit) {
    return t->heap_base + virt_addr;
  }
  // MALLOC 등으로 얻은 절대 주소는 그대로 반환
  return virt_addr;
}

