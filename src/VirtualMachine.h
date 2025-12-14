#ifndef VIRTUAL_MACHINE_H
#define VIRTUAL_MACHINE_H

#include "Task.h"

// VM이 커널 함수를 호출해야 하므로 Forward Declaration 필요
void Kernel_stdWrite(int fd, int val);
void Kernel_stdWriteChar(int fd, char c);
int  Kernel_stdRead(int fd);
void Kernel_systemCall(Task* t, int sys_id);
void Kernel_yield(Task* t); // OP_SLEEP에서 사용

// VM 메인 함수
void VM_runStep(Task* t);

#endif

