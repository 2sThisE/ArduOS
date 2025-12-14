#ifndef HAL_H
#define HAL_H

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include "OSConfig.h" // FD_STDERR 등을 알기 위해 필요
#include <StreamProtocol.h> // [신규]
#include "Protocol.h"       // [신규]

extern SdFat32 sd;
extern volatile unsigned long system_ticks;

void HAL_init();
void HAL_setupTimer();

// 여기가 핵심! 선언이 반드시 있어야 함
void HAL_write(int fd, const char* text);
void HAL_write(int fd, int num);
void HAL_writeChar(int fd, char c);
int  HAL_read(int fd);

// [신규] 통신 모듈에서 입력을 넣어주는 함수
void HAL_pushInput(const uint8_t* data, uint32_t len);

// [신규] 통신 데몬용 패킷 읽기 함수
// 리턴값: 읽은 Payload 길이 (-1이면 패킷 없음)
// cmd_out: 명령어 ID가 저장됨
// payload_out: Payload 데이터가 복사됨
int HAL_readPacket(uint16_t* cmd_out, uint8_t* payload_out, uint32_t max_len);

#endif
