#include <ax8052f143.h>
#include <libmftypes.h>
#include <libmfradio.h>
#include <libmfflash.h>
#include <libmfwtimer.h>
#include <libmfcrc.h>
#include "misc.h"
#include "hal.h"
#include "easyax5043.h"
#include "nbfi.h"
#include "nbfi_config.h"
#include "application.h"
#include "slip.h"
#include <stdlib.h>

typedef struct
{
    uint8_t cmd;
    uint8_t payload[270];
    uint16_t len;
}slip_buf_t;

__no_init uint8_t __idata message @ (0xFF);

struct wtimer_desc __xdata slip_process_desc;
struct wtimer_desc __xdata uart_sleep_timer;

static __xdata char uart_can_sleep = 1;

__xdata slip_buf_t slip_rxbuf;

static uint8_t SLIP_Receive();

void SLIP_test_unsleep()
{
    DIRA &= ~(1<<2);
    PORTA_2 = HIGH;
    INTCHGA = 1 << 2;
    INTCHGB = 0;
    INTCHGC = 0;
    //do { char tmp_pinA = PINA; } while(0);
    IE_3 = 1;
    EA = 1;
    if (!PINA_2) {
        uart_can_sleep = 0;
        IE_3 = 0;
    }
    do { char tmp_pinA = PINA; } while(0);
}

char SLIP_isCanSleep()
{
    return uart_can_sleep;
}

void SLIP_setCanSleep(char c)
{
    uart_can_sleep = c;
}

void SLIP_toCanSleep(struct wtimer_desc __xdata *desc)
{
    uart_can_sleep = 1;
}

static void SLIP_restartTimerSleep()
{
    wtimer0_remove(&uart_sleep_timer);
    SLIP_Init();
}

void SLIP_Init()
{
    uart_can_sleep = 0;
    ScheduleTask(&uart_sleep_timer, SLIP_toCanSleep, RELATIVE, SECONDS(5));
    ScheduleTask(&slip_process_desc, SLIP_Process, RELATIVE, MILLISECONDS(10));
}

void SLIP_Send_debug(uint8_t __generic * str, uint8_t len)
{
    SLIP_restartTimerSleep();
    for(int i=0; i<len; i++)
    {
        if(str[i]) uart1_tx(str[i]);
        else break;

    }
    uart1_tx(0x0d);
    SLIP_restartTimerSleep();
}

void SLIP_Send(uint8_t cmd, uint8_t __generic * payload, uint8_t len)
{
    uart_can_sleep = 0;
    SLIP_restartTimerSleep();
    //UART_Init();
    // Encode to SLIP
    uart1_tx(SLIP_START);
    uart1_tx(cmd);
    for(int i=0; i<=len; i++)
    {
        uint8_t c;
        if(i == len) c = CRC8(payload, len);
        else c = payload[i];
        if((c==SLIP_START) || (c==SLIP_END) || (c==SLIP_ESC))
        {
            uart1_tx(SLIP_ESC);
            uart1_tx(c ^ 0xFF);
        }
        else uart1_tx(c);
    }
    uart1_tx(SLIP_END);
    SLIP_restartTimerSleep();
}

//static __xdata nbfi_packet_t pkt;
nbfi_transport_packet_t __xdata* packet;
//uint32_t aaa;

void SLIP_Process(struct wtimer_desc __xdata *desc)
{
    static uint8_t __xdata st = 0;
    if(SLIP_Receive())
    {
        switch(slip_rxbuf.cmd)
        {
        case SLIP_CMD_ECHO:
            SLIP_Send(slip_rxbuf.cmd, slip_rxbuf.payload, slip_rxbuf.len);
            break;
        case SLIP_CMD_SEND_DL:
//        case SLIP_CMD_SEND_500_DL:
//        case SLIP_CMD_SEND_FAST_DL:
            packet =  NBFi_AllocateTxPkt(slip_rxbuf.len - 1);
            if(packet)
            {
                memcpy_xdata(&packet->phy_data.header, slip_rxbuf.payload, slip_rxbuf.len);
                packet->phy_data_length = slip_rxbuf.len - 1;
                packet->handshake = HANDSHAKE_NONE;
                packet->state = PACKET_QUEUED;
                nbfi.mode = TRANSPARENT;
                NBFi_Force_process();
                NBFi_Wait_For_Transmit_End();
                slip_rxbuf.payload[0] = 0;      //OK
            }
            else slip_rxbuf.payload[0] = 0x04;  //ERR_BUFFER_FULL
            SLIP_Send(slip_rxbuf.cmd, slip_rxbuf.payload, 1);
            break;

        case SLIP_CMD_SET_FREQUENCY:
            if(slip_rxbuf.len == 4)
            {
                uint32_t freq = 0;
                freq = (freq+slip_rxbuf.payload[0])<<8;
                freq = (freq+slip_rxbuf.payload[1])<<8;
                freq = (freq+slip_rxbuf.payload[2])<<8;
                freq = (freq+slip_rxbuf.payload[3]);
                nbfi.tx_freq = freq;
            }
            SLIP_Send(slip_rxbuf.cmd, slip_rxbuf.payload, 1);
            break;
        case SLIP_CMD_SET_POWER:
            if(slip_rxbuf.len == 1)
            {
                nbfi.tx_pwr = slip_rxbuf.payload[0];
            }
            slip_rxbuf.payload[0] = nbfi.tx_pwr;
            SLIP_Send(slip_rxbuf.cmd, slip_rxbuf.payload, 1);
            break;
        case SLIP_CMD_SEND_CORR_PCKT:
            NBFi_TX_Correction();
            slip_rxbuf.payload[0] = 0;
            SLIP_Send(slip_rxbuf.cmd, slip_rxbuf.payload, 1);
            break;
        case SLIP_CMD_SET_DL_ADDRESS:
            if(slip_rxbuf.len == 3)
            {
                memcpy_xdata(nbfi.dl_ID, slip_rxbuf.payload, sizeof(nbfi.dl_ID));
            }
            SLIP_Send(slip_rxbuf.cmd, nbfi.dl_ID, sizeof(nbfi.dl_ID));
            break;
//        case SLIP_CMD_GET_SERIAL:
//            slip_rxbuf.payload[0] = unb.serial[0];
//            slip_rxbuf.payload[1] = unb.serial[1];
//            slip_rxbuf.payload[2] = unb.serial[2];
//            slip_rxbuf.payload[3] = unb.serial[3];
//            SLIP_Send(slip_rxbuf.cmd, slip_rxbuf.payload, 4);
//            break;
        case SLIP_CMD_SOFTWARE_RESET:
            message = 0xB1;
            PCON |= 0x10; // SOFTWARE RESET
            break;
        case SLIP_CMD_NBFI_TX_UL:
            slip_rxbuf.payload[0] = NBFi_Send((uint8_t __generic*) slip_rxbuf.payload, slip_rxbuf.len);
            SLIP_Send(slip_rxbuf.cmd, slip_rxbuf.payload, 1);
            break;
        case SLIP_NBFI_CONFIG:
            if(NBFi_Config_Parser(slip_rxbuf.payload))
            {
                nbfi_settings_changed = 0;
                SLIP_Send(slip_rxbuf.cmd, slip_rxbuf.payload, 7);
            }
            break;
        default:
            break;
        }
    }
    ScheduleTask(desc, 0, RELATIVE, MILLISECONDS(5));
}

static uint8_t SLIP_Receive()
{
    char c;
    static uint8_t mode = SLIP_MODE_START;
    char restart_sleep = 1;

    while(uart1_rxcount())
    {
        if (restart_sleep) {
            restart_sleep = 0;
            SLIP_restartTimerSleep();
        }
        c = uart1_rx();

        switch (mode)
        {
        case  SLIP_MODE_START:
            if(c == SLIP_START)
            {
                slip_rxbuf.len = 0;
                memset_xdata(slip_rxbuf.payload, 0, 20);
                mode = SLIP_MODE_RECEIVING_CMD;
            }
            break;
        case SLIP_MODE_RECEIVING_PAYLOAD:
        case SLIP_MODE_RECEIVING_CMD:
            if(c == SLIP_ESC)
            {
                mode = SLIP_MODE_ESC;
                break;
            }
            if(c == SLIP_START)
            {
                slip_rxbuf.len = 0;
                memset_xdata(slip_rxbuf.payload, 0, 20);
                mode = SLIP_MODE_RECEIVING_CMD;
                break;
            }
            if(c == SLIP_END)
            {

                mode = SLIP_MODE_START;
                slip_rxbuf.len--;
                if(CRC8(((uint8_t __generic*)&slip_rxbuf.payload[0]), slip_rxbuf.len) != slip_rxbuf.payload[slip_rxbuf.len]) return 0;
                return 1;
            }
            if(mode == SLIP_MODE_RECEIVING_CMD)
            {
                slip_rxbuf.cmd = c;
                mode = SLIP_MODE_RECEIVING_PAYLOAD;
                break;
            }
            if(mode == SLIP_MODE_RECEIVING_PAYLOAD)
            {
                slip_rxbuf.payload[slip_rxbuf.len++] = c;
                break;
            }
            break;
        case SLIP_MODE_ESC:
            slip_rxbuf.payload[slip_rxbuf.len++] = c ^ 0xFF;
            mode = SLIP_MODE_RECEIVING_PAYLOAD;
            break;
        default:
            mode = SLIP_MODE_START;
            break;
        }

        if(slip_rxbuf.len == 270)
        {
            mode = SLIP_MODE_START;
        }
    }
    return 0;
}
