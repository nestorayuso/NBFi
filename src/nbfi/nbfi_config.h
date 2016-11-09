#ifndef NBFI_CONFIG_H
#define NBFI_CONFIG_H
#include "defines.h"

#define NBFI_UL_FREQ_BASE       (868800000 - 29250)
#define NBFI_DL_FREQ_BASE       446000000

//#define NBFI_FASTDL_FREQ_BASE   445000000

#define READ_PARAM_CMD          0x00
#define WRITE_PARAM_CMD         0x01
//#define RATE_CHANGE_PARAM_CMD   0x02


#define NBFI_PARAM_MODE         0x00
#define NBFI_PARAM_HANDSHAKE    0x01
#define NBFI_PARAM_MAXLEN       0x02
#define NBFI_PARAM_TXFREQ       0x03
#define NBFI_PARAM_RXFREQ       0x04
#define NBFI_PARAM_ANT          0x05
#define NBFI_PARAM_DL_ADD       0x06



typedef struct
{
    nbfi_mode_t mode;
    nbfi_phy_channel_t tx_phy_channel;
    nbfi_phy_channel_t rx_phy_channel;
    nbfi_handshake_t   handshake_mode;
    nbfi_mack_mode_t   mack_mode;
    uint8_t     num_of_retries;
    uint8_t     max_payload_len;
    uint8_t     dl_ID[3];
    uint8_t     temp_ID[3];
    uint8_t     full_ID[6];
    uint32_t    tx_freq;
    uint32_t    rx_freq;
    uint8_t     tx_antenna;
    uint8_t     rx_antenna;
    int8_t      tx_pwr;
    nbfi_attach_state_t  attach_state;
}nbfi_settings_t;

#define RX_CONF 0x01
#define TX_CONF 0x02


typedef enum
{
    DOWN = 0,     // data rate change down direction
    UP = 1        // data rate change up direction
}nbfi_rate_direct_t;

extern nbfi_settings_t __xdata nbfi;
extern bool nbfi_settings_changed;

extern uint8_t __xdata you_should_dl_power_step_up;
extern uint8_t __xdata you_should_dl_power_step_down;
extern uint8_t __xdata current_tx_rate;
extern uint8_t __xdata current_rx_rate;

void NBFI_Config_Check_State();

bool NBFi_Config_Parser(uint8_t __xdata* buf);
bool NBFi_Config_Rate_Change(uint8_t rx_tx, nbfi_rate_direct_t dir );
void NBFi_Config_Return();
bool NBFi_Config_Send_Mode(bool);
void NBFi_Config_Set_Default();
bool NBFi_Config_Tx_Power_Change(nbfi_rate_direct_t dir);

#endif // NBFI_CONFIG_H
