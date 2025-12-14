# ls.asm - List Directory Files
    
    # 1. Prepare Buffer Args
    # Syscall_ls expects 3 args on stack (popped in this order): 
    #   1. BufferAddr (Top)
    #   2. BufferSize
    #   3. TargetDirPathSelector
    
    # So we push them in reverse order: Selector, Size, Addr
# @heap 32
    PUSH 1      # TargetDirPathSelector (1: Use t->args)
    PUSH 100    # Buffer Size (100 ints)
    PUSH 0      # Buffer Addr (Virtual Heap 0) -> Top of Stack

    # 2. Call Syscall 1 (LS)
    PUSH 1      # SysID 1
    SYS

    # 3. Print Result String (Optimized)
    # Use new OP_PRTS (Opcode 5) to print the string starting at Heap[0]
    PUSH 1      # Start Address
    PRTS        # Print String from Heap[0] until NULL
    
    EXIT