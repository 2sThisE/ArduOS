#include "Kernel.h"
#include "syscall/SysLs.h"
#include "syscall/SysExec.h"
#include "syscall/SysChdir.h"
#include "syscall/SysGetCwd.h"

// System call dispatcher
// 1. ls
// 2. exec
// 3. chdir(cd)
// 4. getcwd (Get Current Working Directory)
// 5. lcd clear
// 6. lcd set cursor(row,col)
void Kernel_systemCall(Task* t, int sys_id) {
  switch (sys_id) {
    case 1:
      Syscall_ls(t);
      break;
    case 2: 
      Syscall_exec(t);
      break;
    case 3:
      Syscall_chdir(t);
      break;
    case 4:
      Syscall_getcwd(t); // [신규]
      break;
    default:
      // unknown syscall: ignore for now
      break;
  }
}
