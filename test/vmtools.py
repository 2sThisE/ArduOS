#!/usr/bin/env python3
import sys

# ------------------------------------------------------------
# 1. 명령어 정의 (OSConfig.h와 100% 일치해야 함)
# ------------------------------------------------------------
OPCODES = {
    "EXIT":   0x00,
    "PRINT":  0x01, "READ":   0x02, "PRTC":   0x03, "PRTE":   0x04, "PRTS":   0x05,
    "PUSH":   0x10, "ADD":    0x11, "SUB":    0x12, "EQ":     0x13, "DUP":    0x14, "POP": 0x15,
    "JMP":    0x20, "JIF":    0x21,
    "SYS":    0x30,
    "PIN_MODE": 0x40, "D_WRITE":  0x41, "SLEEP":    0x42,
    "MALLOC": 0x50, "LOAD":   0x51, "STORE":  0x52,
}

OPS_WITH_IMM = {"PUSH", "JMP", "JIF"}

def parse_number(tok):
    tok = tok.strip()
    if tok.startswith("0x") or tok.startswith("0X"): return int(tok, 16)
    if tok.startswith("'") and len(tok) == 3: return ord(tok[1])
    return int(tok, 10)

def assemble(lines):
    parsed_ops = []
    labels = {}
    pc = 0
    heap_size = 128 # Default Heap Size (if not specified)

    # -------------------------------------------------
    # Pass 1: 파싱, 라벨 계산, 헤더 정보 추출
    # -------------------------------------------------
    for raw_line in lines:
        raw_line = raw_line.strip()
        
        # [Header Directive Check] # @heap 256
        if raw_line.startswith("# @heap"):
            try:
                parts = raw_line.split()
                if len(parts) >= 3:
                    heap_size = int(parts[2])
                    print(f"[Info] Custom Heap Size: {heap_size}")
            except:
                print("[Warn] Invalid heap directive")
            continue

        # 1. 주석 제거 (# 문자 뒤는 무시)
        code_part = raw_line.split("#", 1)[0].strip()
        if not code_part: continue

        # 2. 명령어 분리 (; 세미콜론을 구분자로 사용)
        instructions = code_part.split(';')

        for inst in instructions:
            inst = inst.strip()
            if not inst: continue

            # 라벨 처리 (LABEL:)
            if ":" in inst:
                lbl, remainder = inst.split(":", 1)
                labels[lbl.strip()] = pc
                inst = remainder.strip()
                if not inst: continue

            parts = inst.split()
            mnem = parts[0].upper()
            operand = parts[1] if len(parts) > 1 else None

            if mnem not in OPCODES:
                raise ValueError(f"Unknown opcode: {mnem} in line: {raw_line}")
            
            # PC 증가
            size = 3 if mnem in OPS_WITH_IMM else 1
            parsed_ops.append({
                "mnem": mnem,
                "operand": operand,
                "size": size
            })
            pc += size

    # -------------------------------------------------
    # Pass 2: 바이트코드 생성
    # -------------------------------------------------
    # [Header Generation]
    # Magic(1) + Ver(1) + HeapSize(2, LE)
    header = bytearray([0xAD, 0x01, heap_size & 0xFF, (heap_size >> 8) & 0xFF])
    
    body = []
    for op in parsed_ops:
        mnem = op["mnem"]
        operand = op["operand"]
        
        # Opcode 추가
        body.append(OPCODES[mnem])

        # Operand 추가 (있는 경우)
        if op["size"] == 3:
            if operand is None:
                raise ValueError(f"Opcode {mnem} requires an operand")
            
            val = 0
            if operand in labels:
                val = labels[operand]
            else:
                val = parse_number(operand)
            
            # Little Endian (Low byte, High byte)
            body.append(val & 0xFF)
            body.append((val >> 8) & 0xFF)

    return header + bytes(body)

# ------------------------------------------------------------
# Main
# ------------------------------------------------------------
if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "asm":
        try:
            # encoding='utf-8' 추가하여 인코딩 에러 방지
            with open(sys.argv[2], "r", encoding="utf-8") as f: 
                lines = f.readlines()
            
            code = assemble(lines)
            
            with open(sys.argv[3], "wb") as f: 
                f.write(code)
            
            print(f"[Success] Generated {sys.argv[3]} ({len(code)} bytes)")
            
        except Exception as e: 
            print(f"[Error] {e}")
    else:
        print("Usage: python vmtools.py asm <source.asm> <out.bin>")