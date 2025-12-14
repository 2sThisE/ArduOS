# @heap 16

INIT:
    # 카운터 초기화 (Heap[0] = 0)
    PUSH 0
    PUSH 0
    STORE

LOOP:
    # 1. 종료 조건 확인 (Counter == 20000)
    PUSH 0      # Heap[0] 주소
    LOAD        # 값 읽기
    PUSH 20000  # 목표 횟수 (이 숫자를 바꾸면 실행 시간 조절 가능)
    EQ
    JIF FINISH  # 20000과 같으면 루프 종료

    # 2. 카운터 증가 (Counter++)
    PUSH 0      # Heap[0] 주소
    LOAD
    PUSH 1
    ADD         # 1 더하기
    PUSH 0      # Heap[0] 주소
    STORE       # 저장

    # 3. 루프 반복
    JMP LOOP

FINISH:
    # 4. 종료 메시지 출력 ("DONE\n")
    PUSH 'D'
    PRTC
    PUSH 'O'
    PRTC
    PUSH 'N'
    PRTC
    PUSH 'E'
    PRTC
    PUSH 10     # 줄바꿈 (LF)
    PRTC

    # 프로그램 종료 (더 이상 명령어가 없으면 태스크 종료)
    EXIT