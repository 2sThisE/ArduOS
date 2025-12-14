# @heap 256
INIT:
    PUSH 0
    PUSH 0
    STORE # Index(Heap[0]) = 0

    # 초기 경로 획득 (GetCwd)
    PUSH 200 # CWD Buffer Address (Heap[200])
    PUSH 4   # SysID 4 (GetCwd)
    SYS

    # 첫 프롬프트 출력
    PUSH 200 # CWD Address
    PRTS     # Print CWD (e.g., "/")
    PUSH '>'
    PRTC
    PUSH 32  # Space
    PRTC

LOOP:
    READ
    
    # Check 0 (No Input)
    DUP
    PUSH 0
    EQ
    JIF CLEANUP
    
    # Check CR (13)
    DUP
    PUSH 13
    EQ
    JIF CLEANUP
    
    # Check LF (10)
    DUP
    PUSH 10
    EQ
    JIF PARSE

    # --- Store Char ---
    # Addr = Heap[0] + 1
    PUSH 0
    LOAD
    PUSH 1
    ADD
    
    # STORE [Input, Addr]
    STORE
    
    # Index++
    PUSH 0
    LOAD
    PUSH 1
    ADD
    PUSH 0
    STORE
    
    JMP LOOP

CLEANUP:
    POP
    JMP LOOP

PARSE:
    POP # Remove Enter
    
    # Null terminate string
    PUSH 0
    PUSH 0
    LOAD
    PUSH 1
    ADD
    STORE

    # --- Scan for Space ---
    # ScanPtr(Heap[64]) = 1
    PUSH 1
    PUSH 64
    STORE

SCAN_LOOP:
    PUSH 64; LOAD; LOAD # Read char
    
    DUP
    PUSH 0
    EQ
    JIF NO_ARGS
    
    DUP
    PUSH 32
    EQ
    JIF FOUND_SPACE
    
    POP
    
    # ScanPtr++
    PUSH 64; LOAD; PUSH 1; ADD; PUSH 64; STORE
    
    JMP SCAN_LOOP

FOUND_SPACE:
    POP
    
    # Null terminate command
    PUSH 0
    PUSH 64
    LOAD
    STORE
    
    # ArgAddr = ScanPtr + 1
    PUSH 64; LOAD; PUSH 1; ADD
    JMP DISPATCH

NO_ARGS:
    POP
    PUSH 0 # ArgAddr = 0

DISPATCH:
    # Stack: [ArgAddr]
    
    # --- ArgAddr 처리 (Heap[67]에 저장) ---
    # ArgAddr을 스택에서 꺼내 힙 임시 공간(Heap[67])에 저장
    PUSH 67
    STORE
    
    # 0번지는 없다는 뜻이므로, Heap[67]을 비움 (0으로 만듦)
    PUSH 67; LOAD # ArgAddr (스택에서 꺼냄)
    PUSH 0
    EQ
    JIF NO_ARGS_TO_COPY

    # ArgAddr이 있으면, 인자 문자열을 쉘의 힙 공간(Heap[110])에 복사하고
    # 그 시작 주소를 Heap[67]에 다시 저장 (ArgAddr 임시 변수)
    # -----------------------------------------------------------------
    # (ArgAddr는 이미 스택에 없으므로, Heap[67]에서 가져와서 처리)

    # 1. 대상 주소 (Heap[110])
    PUSH 110
    PUSH 68 # Temp Dest Ptr (Heap[68])
    STORE

    # 2. 소스 주소 (Heap[67] -> ArgAddr)
    PUSH 67; LOAD
    PUSH 69 # Temp Source Ptr (Heap[69])
    STORE

COPY_ARGS_LOOP:
    PUSH 69; LOAD; LOAD # Read char
    DUP
    PUSH 0
    EQ
    JIF COPY_ARGS_END
    
    PUSH 68; LOAD; STORE
    
    PUSH 68; LOAD; PUSH 1; ADD; PUSH 68; STORE
    PUSH 69; LOAD; PUSH 1; ADD; PUSH 69; STORE
    JMP COPY_ARGS_LOOP

COPY_ARGS_END:
    POP # Remove NULL
    PUSH 0 # Null terminate target buffer
    PUSH 68; LOAD; STORE

    # Heap[67]에 인자 문자열의 새로운 시작 주소(Heap[110]) 저장
    PUSH 110
    PUSH 67
    STORE

    JMP CONTINUE_DISPATCH

NO_ARGS_TO_COPY:
    PUSH 0 # ArgAddr = 0
    PUSH 67 # Store 0 for no args
    STORE

CONTINUE_DISPATCH:
    # --- Check for Built-in 'cd' ---
    # Command is at Heap[1]
    
    # Check 'c'
    PUSH 1
    PUSH 66 # Temp Ptr (Heap[66])
    STORE
    
    PUSH 66; LOAD; LOAD # Heap[1]
    PUSH 'c'
    EQ
    JIF CHECK_CD_2
    JMP CHECK_EXEC # Not 'c', go to external

CHECK_CD_2:
    # Check 'd'
    PUSH 66; LOAD; PUSH 1; ADD; PUSH 66; STORE # Ptr++ (Heap[2])
    PUSH 66; LOAD; LOAD
    PUSH 'd'
    EQ
    JIF CHECK_CD_END
    JMP CHECK_EXEC

CHECK_CD_END:
    # Check End of String (NULL or Space)
    PUSH 66; LOAD; PUSH 1; ADD; PUSH 66; STORE # Ptr++ (Heap[3])
    PUSH 66; LOAD; LOAD
    
    DUP
    PUSH 0
    EQ
    JIF DO_CD # "cd" (NULL)
    
    DUP
    PUSH 32
    EQ
    JIF DO_CD # "cd " (Space)
    
    POP # Neither NULL nor Space (e.g., "cda")
    JMP CHECK_EXEC

DO_CD:
    # Syscall_chdir (SysID 3) expects 2 arguments on stack: [PathAddr (Top), BufferAddr]
    # Kernel pops PathAddr first, then BufferAddr.
    
    PUSH 200      # Push BufferAddr (Heap[200] for CWD update) - Bottom
    PUSH 67; LOAD # Push PathAddr (from Heap[67]) - Top
    
    PUSH 3        # SysID 3 (chdir)
    SYS
    JMP RESET

# ==========================================
# EXEC 명령어 구현 (CHECK_CD_END와 DO_EXEC 사이에 삽입)
# ==========================================
CHECK_EXEC:
    # 1. 'exec' 문자열 확인
    PUSH 1
    PUSH 66; STORE # Heap[66] = Cmd Ptr

    # Check 'e'
    PUSH 66; LOAD; LOAD
    PUSH 'e'
    EQ
    JIF CHECK_EXEC_2   # 'e'가 맞으면 다음 글자 검사로
    JMP TRY_NEXT_CHECK # 아니면 실패

CHECK_EXEC_2:
    # Check 'x'
    PUSH 66; LOAD; PUSH 1; ADD; PUSH 66; STORE
    PUSH 66; LOAD; LOAD
    PUSH 'x'
    EQ
    JIF CHECK_EXEC_3
    JMP TRY_NEXT_CHECK

CHECK_EXEC_3:
    # Check 'e'
    PUSH 66; LOAD; PUSH 1; ADD; PUSH 66; STORE
    PUSH 66; LOAD; LOAD
    PUSH 'e'
    EQ
    JIF CHECK_EXEC_4
    JMP TRY_NEXT_CHECK

CHECK_EXEC_4:
    # Check 'c'
    PUSH 66; LOAD; PUSH 1; ADD; PUSH 66; STORE
    PUSH 66; LOAD; LOAD
    PUSH 'c'
    EQ
    JIF CHECK_EXEC_END 
    JMP TRY_NEXT_CHECK

CHECK_EXEC_END:       
    # 끝 (NULL or Space) 확인
    PUSH 66; LOAD; PUSH 1; ADD; PUSH 66; STORE
    PUSH 66; LOAD; LOAD
    
    DUP
    PUSH 0
    EQ
    JIF PARSE_EXEC_START
    
    DUP
    PUSH 32
    EQ
    JIF PARSE_EXEC_START
    
    POP
    JMP TRY_NEXT_CHECK

TRY_NEXT_CHECK:
    JMP DO_EXEC # exec가 아니면 원래의 외부 파일 실행 로직으로 이동

PARSE_EXEC_START:
    POP # Stack 정리 (Space/Null 제거)

    # 1. 초기화 (기본 동기 모드)
    PUSH 1 
    PUSH 82; STORE # Heap[82] = WaitOption (1=Sync)

    PUSH 67; LOAD # 인자 문자열 시작 주소
    PUSH 80; STORE # Heap[80] = Source Ptr

    # 인자 없음 체크 (그냥 "exec"만 입력했을 때)
    PUSH 80; LOAD; PUSH 0; EQ; JIF RESET

    # 2. 옵션 파싱 (-b 체크)
    PUSH 80; LOAD; LOAD
    PUSH '-'
    EQ
    JIF CHECK_FLAG_B
    JMP PARSE_PATH

CHECK_FLAG_B:
    PUSH 80; LOAD; PUSH 1; ADD; LOAD
    PUSH 'b'
    EQ
    JIF SET_ASYNC
    JMP PARSE_PATH

SET_ASYNC:
    PUSH 0
    PUSH 82; STORE # 비동기 설정 (0)
    PUSH 80; LOAD; PUSH 2; ADD; PUSH 80; STORE # "-b" 건너뜀

SKIP_SPACE_LOOP:
    PUSH 80; LOAD; LOAD
    PUSH 32
    EQ
    JIF SKIP_SPACE_ACTION
    JMP PARSE_PATH

SKIP_SPACE_ACTION:
    PUSH 80; LOAD; PUSH 1; ADD; PUSH 80; STORE
    JMP SKIP_SPACE_LOOP

    # 3. 경로 추출 (Heap[80] -> Heap[150])
PARSE_PATH:
    PUSH 150 # Path Buffer Start (Heap[150])
    PUSH 81; STORE

COPY_PATH_LOOP:
    PUSH 80; LOAD; LOAD
    DUP; PUSH 0; EQ; JIF COPY_PATH_END
    DUP; PUSH 32; EQ; JIF COPY_PATH_END_SPACE

    PUSH 81; LOAD; STORE
    PUSH 80; LOAD; PUSH 1; ADD; PUSH 80; STORE
    PUSH 81; LOAD; PUSH 1; ADD; PUSH 81; STORE
    JMP COPY_PATH_LOOP

COPY_PATH_END_SPACE:
    POP
    PUSH 80; LOAD; PUSH 1; ADD; PUSH 80; STORE
    
    # 경로 뒤 공백 스킵 (인자 시작점 찾기)
SKIP_ARGS_SPACE:
    PUSH 80; LOAD; LOAD
    PUSH 32
    EQ
    JIF SKIP_ARGS_SPACE_ACTION
    JMP TERMINATE_PATH

SKIP_ARGS_SPACE_ACTION:
    PUSH 80; LOAD; PUSH 1; ADD; PUSH 80; STORE
    JMP SKIP_ARGS_SPACE

COPY_PATH_END:
    POP
    PUSH 0
    PUSH 80; STORE 

TERMINATE_PATH:
    PUSH 0
    PUSH 81; LOAD; STORE # 경로 문자열 끝(NULL) 처리

    # 4. EXEC 시스템 콜 호출
    # Stack: [ArgAddr] -> [CmdAddr] -> [WaitOption] (커널 POP 순서 역순)
    
    PUSH 80; LOAD  # ArgAddr (Heap[80] or 0)
    PUSH 150       # CmdAddr (Path Buffer)
    PUSH 82; LOAD  # WaitOption (1 or 0)
    
    PUSH 2         # SysID 2 (Exec)
    SYS
    
    JMP RESET
# ==========================================

DO_EXEC:
    # --- Construct Full Path: "/bin/" + CMD + ".bin" ---
    # Build at Heap[70]
    # 1. Copy "/bin/" to Heap[70]
    PUSH '/'
    PUSH 70
    STORE

    PUSH 'b'
    PUSH 71
    STORE

    PUSH 'i'
    PUSH 72
    STORE

    PUSH 'n'
    PUSH 73
    STORE

    PUSH '/'
    PUSH 74
    STORE

    # Current Output Ptr = 75
    PUSH 75
    PUSH 65 # Temp Ptr (Heap[65])
    STORE

    # 2. Copy Command Name (from Heap[1])
    PUSH 1
    PUSH 66 # Source Ptr (Heap[66])
    STORE

COPY_CMD_LOOP:

    PUSH 66; LOAD; LOAD

    DUP

    PUSH 0

    EQ

    JIF COPY_CMD_END

    

    PUSH 65; LOAD; STORE

    

    PUSH 65; LOAD; PUSH 1; ADD; PUSH 65; STORE

    PUSH 66; LOAD; PUSH 1; ADD; PUSH 66; STORE

    JMP COPY_CMD_LOOP



COPY_CMD_END:
    POP # Remove NULL
    # 3. Copy ".bin"
    PUSH '.'
    PUSH 65; LOAD; STORE

    PUSH 65; LOAD; PUSH 1; ADD; PUSH 65; STORE

    PUSH 'b'

    PUSH 65; LOAD; STORE

    PUSH 65; LOAD; PUSH 1; ADD; PUSH 65; STORE

    PUSH 'i'
    PUSH 65; LOAD; STORE

    PUSH 65; LOAD; PUSH 1; ADD; PUSH 65; STORE

    PUSH 'n'
    PUSH 65; LOAD; STORE
    PUSH 65; LOAD; PUSH 1; ADD; PUSH 65; STORE

    # 4. Null Terminate
    PUSH 0
    PUSH 65; LOAD; STORE

    # --- Call Exec ---
    # Stack has [ArgAddr]
    PUSH 67; LOAD # Push ArgAddr (from Heap[67])
    PUSH 70 # Push CmdAddr (Path to /bin/ls.bin)
    
    # [WaitOption] 1 = Sync (Wait for child), 0 = Async
    PUSH 1 
    
    PUSH 2 # SysID 2
    SYS
    
    JMP RESET

RESET:
    # --- Clear Input Buffer (Heap[1] ~ Heap[63]) ---
    PUSH 1
    PUSH 65
    STORE

CLEAR_LOOP:
    PUSH 0
    PUSH 65; LOAD; STORE
    
    PUSH 65; LOAD; PUSH 1; ADD; PUSH 65; STORE
    
    PUSH 65; LOAD
    PUSH 64 # Clear up to 63
    EQ
    JIF CLEAR_LOOP_END
    JMP CLEAR_LOOP

CLEAR_LOOP_END:
    # --- Reset Index ---
    PUSH 0
    PUSH 0
    STORE
    
    # 프롬프트 출력 (CWD + "> ")
    PUSH 200 # CWD Address
    PRTS     # Print CWD
    PUSH '>'
    PRTC
    PUSH 32 # Space
    PRTC
    
    JMP LOOP