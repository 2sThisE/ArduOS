#include "VirtualMachine.h"
#include "OSConfig.h"

// 외부 함수
extern void Kernel_refillBuffer(Task* t);
extern void Kernel_jump(Task* t, int addr);
extern void Kernel_stdWrite(int fd, int val);
extern void Kernel_stdWrite(int fd, const char* str);
extern void Kernel_stdWriteChar(int fd, char c);
extern int  Kernel_stdRead(int fd);
extern void Kernel_systemCall(Task* t, int sys_id);
extern void Kernel_yield(Task* t);
extern void Kernel_terminateTask(int id);
extern void Kernel_raiseException(Task* t, int error_code);
extern int Kernel_malloc(Task* t, int size);
extern int Kernel_getPhysAddr(Task* t, int virt_addr);
extern int global_heap[];

// --- [안전장치] 매크로 ---
#define CHECK_STACK_OVERFLOW(t) \
  if (t->sp >= VM_STACK_SIZE - 1) { \
    Kernel_stdWrite(FD_STDERR, "Err: Stack Overflow\n"); \
    Kernel_terminateTask(t->id); \
    return; \
  }

#define CHECK_STACK_UNDERFLOW(t, count) \
  if (t->sp < (count - 1)) { \
    Kernel_stdWrite(FD_STDERR, "Err: Stack Underflow\n"); \
    Kernel_terminateTask(t->id); \
    return; \
  }

// ============================================================
// [핵심 해결책] 안전하게 1바이트 읽어오는 함수
// 경계선(32byte)을 넘어가면 알아서 재장전합니다.
// ============================================================
uint8_t VM_fetchByte(Task* t) {
  if (t->buffer_index >= CODE_BUFFER_SIZE) {
    Kernel_refillBuffer(t);
    if (!t->isActive()) return 0; // 파일 끝
  }
  return t->code_buffer[t->buffer_index++];
}

// [신규] 2바이트 정수 읽기 (Little Endian)
int VM_fetchInt(Task* t) {
  uint8_t low = VM_fetchByte(t);
  uint8_t high = VM_fetchByte(t);
  return (int)(low | (high << 8));
}

// ------------------------------------------------
// VM 메인 루프
// ------------------------------------------------
void VM_runStep(Task* t) {
  // 1. [Fetch] 안전한 함수 사용!
  uint8_t opcode = VM_fetchByte(t);
  
  if (!t->isActive()) return;

  // 2. [Execute]
  switch (opcode) {
    case OP_PUSH: {
      CHECK_STACK_OVERFLOW(t);
      int val = VM_fetchInt(t); // 2바이트 읽기
      t->stack[++t->sp] = val;
      break;
    }
    case OP_ADD: {
      CHECK_STACK_UNDERFLOW(t, 2);
      int a = t->stack[t->sp--];
      int b = t->stack[t->sp--];
      t->stack[++t->sp] = a + b;
      break;
    }
    case OP_SUB: {
      CHECK_STACK_UNDERFLOW(t, 2);
      int b = t->stack[t->sp--];
      int a = t->stack[t->sp--];
      t->stack[++t->sp] = a - b;
      break;
    }
    case OP_EQ: {
      CHECK_STACK_UNDERFLOW(t, 2);
      int a = t->stack[t->sp--];
      int b = t->stack[t->sp--];
      t->stack[++t->sp] = (a == b) ? 1 : 0;
      break;
    }
    case OP_DUP: {
      CHECK_STACK_UNDERFLOW(t, 1);
      CHECK_STACK_OVERFLOW(t);
      int val = t->stack[t->sp];
      t->stack[++t->sp] = val;
      break;
    }
    case OP_POP: {
      CHECK_STACK_UNDERFLOW(t, 1);
      t->sp--; 
      break;
    }
    
    // --- 제어 흐름 ---
    case OP_JMP: {
      int target = VM_fetchInt(t); // 2바이트 읽기
      Kernel_jump(t, target);
      break;
    }
    case OP_JIF: {
      CHECK_STACK_UNDERFLOW(t, 1);
      int target = VM_fetchInt(t); // 2바이트 읽기
      int condition = t->stack[t->sp--];
      if (condition != 0) {
        Kernel_jump(t, target);
      }
      break;
    }

    // --- 입출력 ---
    case OP_PRINT: { 
      CHECK_STACK_UNDERFLOW(t, 1);
      int val = t->stack[t->sp--];
      Kernel_stdWrite(FD_STDOUT, val);
      Kernel_stdWriteChar(FD_STDOUT, '\n');
      break;
    }
    case OP_PRTC: { 
      CHECK_STACK_UNDERFLOW(t, 1);
      char c = (char)t->stack[t->sp--];
      Kernel_stdWriteChar(FD_STDOUT, c);
      break;
    }
    case OP_PRTE: {
      CHECK_STACK_UNDERFLOW(t, 1);
      int val = t->stack[t->sp--];
      Kernel_stdWrite(FD_STDERR, val);
      Kernel_stdWriteChar(FD_STDERR, '\n');
      break;
    }
    case OP_PRTS: { // [신규] 문자열 출력
      CHECK_STACK_UNDERFLOW(t, 1);
      int addr = t->stack[t->sp--];
      int phys_addr = Kernel_getPhysAddr(t, addr);
      
      char temp_string_buffer[128]; // 임시 문자열 버퍼 (최대 127자 + 널)
      int i = 0;

      // 힙 범위 체크 및 버퍼에 문자 복사
      while (phys_addr >= 0 && phys_addr < GLOBAL_HEAP_SIZE && i < sizeof(temp_string_buffer) - 1) {
        int val = global_heap[phys_addr];
        if (val == 0) break; // NULL 종료
        temp_string_buffer[i++] = (char)val;
        phys_addr++;
      }
      temp_string_buffer[i] = 0; // 널 종료

      Kernel_stdWrite(FD_STDOUT, temp_string_buffer);
      break;
    }
    case OP_READ: { 
      CHECK_STACK_OVERFLOW(t);
      int val = Kernel_stdRead(FD_STDIN);
      t->stack[++t->sp] = (val != -1) ? val : 0;
      break;
    }
    case OP_SYS: { 
      CHECK_STACK_UNDERFLOW(t, 1);
      int sys_id = t->stack[t->sp--];
      Kernel_systemCall(t, sys_id);
      break;
    }
    case OP_SLEEP: {
      CHECK_STACK_UNDERFLOW(t, 1);
      int ms = t->stack[t->sp--];
      t->wake_up_time = ms; 
      Kernel_yield(t);
      break;
    }
    case OP_EXIT: {
      Kernel_terminateTask(t->id);
      break;
    }

    // --- 메모리 ---
    case OP_MALLOC: {
      CHECK_STACK_UNDERFLOW(t, 1);
      int size = t->stack[t->sp--];
      int addr = Kernel_malloc(t, size);
      CHECK_STACK_OVERFLOW(t); 
      t->stack[++t->sp] = (addr == -1) ? 0 : addr;
      break;
    }
    case OP_LOAD: {
      CHECK_STACK_UNDERFLOW(t, 1);
      int addr = t->stack[t->sp--];
      
      // [주소 변환] 공통 함수 사용
      int phys_addr = Kernel_getPhysAddr(t, addr);
      
      if (phys_addr >= 0 && phys_addr < GLOBAL_HEAP_SIZE) {
        CHECK_STACK_OVERFLOW(t);
        t->stack[++t->sp] = global_heap[phys_addr];
      } else {
        Kernel_stdWrite(FD_STDERR, "SegFault: Read ");
        Kernel_stdWrite(FD_STDERR, phys_addr);
        Kernel_stdWrite(FD_STDERR, "\n");
        Kernel_terminateTask(t->id);
      }
      break;
    }
    case OP_STORE: {
      CHECK_STACK_UNDERFLOW(t, 2);
      int addr = t->stack[t->sp--];
      int val  = t->stack[t->sp--];
      
      // [주소 변환] 공통 함수 사용
      int phys_addr = Kernel_getPhysAddr(t, addr);

      if (phys_addr >= 0 && phys_addr < GLOBAL_HEAP_SIZE) {
        global_heap[phys_addr] = val;
      } else {
        Kernel_stdWrite(FD_STDERR, "SegFault: Write Addr ");
        Kernel_stdWrite(FD_STDERR, phys_addr);
        Kernel_stdWriteChar(FD_STDERR, '\n');
        Kernel_terminateTask(t->id);
      }
      break;
    }
  }
}