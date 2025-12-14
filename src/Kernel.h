#ifndef KERNEL_H
#define KERNEL_H

#include "Task.h"
#include <StreamProtocol.h> // [신규] 통신 라이브러리 추가

extern Task tasks[TASK_COUNT];
extern int global_heap[GLOBAL_HEAP_SIZE];

// 커널에서 VM이 호출하는 함수들
void Kernel_init();
void Kernel_processCommunication(); // [신규] 통신 처리 함수
bool Kernel_loadTask(int id, const char* filename, const char* args = NULL, const char* parent_cwd = NULL, const char* parent_arg_str = NULL);
void Kernel_runScheduler(); // loop()에서 이거 하나만 부르면 됨

// VM에서 호출하는 서비스들 (API)
void Kernel_stdWrite(int fd, int val);
void Kernel_stdWrite(int fd, const char* str);
void Kernel_stdWriteChar(int fd, char c);
int  Kernel_stdRead(int fd);
void Kernel_systemCall(Task* t, int sys_id);
void Kernel_refillBuffer(Task* t);
void Kernel_jump(Task* t, int addr);
void Kernel_yield(Task* t);
void Kernel_terminateTask(int id);
void resolve_path(Task* t, const char* input, char* output);

// [메모리 헬퍼]
int Kernel_getPhysAddr(Task* t, int virt_addr);

#endif