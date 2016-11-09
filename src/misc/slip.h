#ifndef SLIP_H
#define SLIP_H

#include <libmfuart.h>
#include <libmfuart1.h>

#define SLIP_START  0xDD
#define SLIP_END    0xDE
#define SLIP_ESC    0xDF

enum{SLIP_MODE_START, SLIP_MODE_RECEIVING_PAYLOAD, SLIP_MODE_RECEIVING_CMD, SLIP_MODE_ESC};

typedef enum
{
    SLIP_CMD_ECHO           = 0x00,
    SLIP_CMD_SEND_B         = 0x01,
    SLIP_CMD_SEND_C         = 0x02,
    SLIP_CMD_SEND_DL        = 0x03,
    SLIP_CMD_SET_RX         = 0x04,
    SLIP_CMD_SET_FREQUENCY  = 0x05,
    SLIP_CMD_SET_POWER      = 0x06,
    SLIP_CMD_SET_DL_ADDRESS = 0x07,
    SLIP_CMD_SEND_CORR_PCKT = 0x08,
    SLIP_CMD_GET_SERIAL     = 0x09,
    SLIP_CMD_RECEIVED_DL    = 0x10,
    SLIP_CMD_SOFTWARE_RESET = 0x20,
    SLIP_CMD_NBFI_TX_UL     = 0x32,
    SLIP_CMD_NBFI_TX_DL     = 0x33,
    SLIP_NBFI_CONFIG        = 0x40,
    SLIP_CMD_SEND_FAST_DL   = 0x43,
    SLIP_CMD_SEND_500_DL    = 0x44
    //...
}slip_cmd_t;

void SLIP_test_unsleep();
void SLIP_setCanSleep(char c);
void SLIP_Init();
void SLIP_Send(uint8_t cmd, uint8_t __generic * payload, uint8_t len);
void SLIP_Send_debug(uint8_t __generic * str, uint8_t len);
//void SLIP_Process();
void SLIP_Process(struct wtimer_desc __xdata *desc);
char SLIP_isCanSleep();

#endif
