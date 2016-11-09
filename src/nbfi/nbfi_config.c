#include "nbfi.h"
#include "nbfi_config.h"
#include "slip.h"
#include "hal.h"
#include "rf.h"



bool NBFi_Config_Tx_Idle();

nbfi_settings_t __xdata nbfi;

#ifdef AMPER_2_3
const nbfi_settings_t __code nbfi_set_default =
{
    CRX,//mode;
    UL_DBPSK_50_PROT_C,//UL_DBPSK_400_PROT_C,//DL_PSK_500,//UL_DBPSK_400_PROT_C,//TX_PROT_C,//DL_PSK_FASTDL,//DL_PSK_500,//UL_DBPSK_50,// tx_phy_channel;
    DL_PSK_200,//UL_PSK_500,//UL_PSK_200,//UL_PSK_FASTDL,//UL_PSK_500,//UL_PSK_200, // rx_phy_channel;
    HANDSHAKE_SIMPLE,// nandshake_mode;
    MACK_1, //mack_mode
    5,//num_of_retries;
    8,//max_payload_len;
    {0},    //dl_ID[3];
    {0},    //temp_ID[3];
    {0},    //full_ID[6];
    0,//NBFI_DL_FREQ_BASE,      //tx_freq;
    0,//NBFI_DL_FREQ_BASE,      //rx_freq;
    PCB,    //tx_antenna;
    PCB,    //rx_antenna;
    RF_MAX_POWER,     //tx_pwr;
};
#elseif SDR
const nbfi_settings_t __code nbfi_set_default =
{
    CRX,//mode;
    UL_DBPSK_50_PROT_C,//UL_DBPSK_400_PROT_C,//DL_PSK_500,//UL_DBPSK_400_PROT_C,//TX_PROT_C,//DL_PSK_FASTDL,//DL_PSK_500,//UL_DBPSK_50,// tx_phy_channel;
    DL_PSK_200,//UL_PSK_500,//UL_PSK_200,//UL_PSK_FASTDL,//UL_PSK_500,//UL_PSK_200, // rx_phy_channel;
    HANDSHAKE_SIMPLE,// nandshake_mode;
    MACK_1, //mack_mode
    5,//num_of_retries;
    8,//max_payload_len;
    {0},    //dl_ID[3];
    {0},    //temp_ID[3];
    {0},    //full_ID[6];
    0,//NBFI_DL_FREQ_BASE,      //tx_freq;
    0,//NBFI_DL_FREQ_BASE,      //rx_freq;
    PCB,    //tx_antenna;
    PCB,    //rx_antenna;
    RF_MAX_POWER,     //tx_pwr;
};
#else
const nbfi_settings_t __code nbfi_set_default =
{
    CRX,//mode;
    UL_DBPSK_50_PROT_C,//UL_DBPSK_400_PROT_C,//DL_PSK_500,//UL_DBPSK_400_PROT_C,//TX_PROT_C,//DL_PSK_FASTDL,//DL_PSK_500,//UL_DBPSK_50,// tx_phy_channel;
    DL_PSK_200,//UL_PSK_500,//UL_PSK_200,//UL_PSK_FASTDL,//UL_PSK_500,//UL_PSK_200, // rx_phy_channel;
    HANDSHAKE_SIMPLE,// nandshake_mode;
    MACK_1, //mack_mode
    5,//num_of_retries;
    8,//max_payload_len;
    {0},    //dl_ID[3];
    {0},    //temp_ID[3];
    {0},    //full_ID[6];
    0,//NBFI_DL_FREQ_BASE,      //tx_freq;
    0,//NBFI_DL_FREQ_BASE,      //rx_freq;
    PCB,    //tx_antenna;
    PCB,    //rx_antenna;
    RF_MAX_POWER,     //tx_pwr;
};
#endif

extern uint8_t  __xdata string[30];

nbfi_settings_t __xdata nbfi_prev;
bool nbfi_settings_changed = 0;


#define NUM_OF_TX_RATES    2
#define NUM_OF_RX_RATES    2
nbfi_phy_channel_t __xdata TxRateTable[NUM_OF_TX_RATES] = {UL_DBPSK_50_PROT_C, UL_DBPSK_400_PROT_C};

nbfi_phy_channel_t __xdata RxRateTable[NUM_OF_RX_RATES] = {DL_PSK_200, DL_PSK_500};
//uint8_t __xdata RSSIRatesTableUp[NUM_OF_RATES] = {30, 40};
//uint8_t __xdata RSSIRatesTableDown[NUM_OF_RATES] = {0, 25};

#define TX_SNRLEVEL_FOR_UP          20
#define TX_SNRLEVEL_FOR_DOWN        15

#define RX_SNRLEVEL_FOR_UP          20
#define RX_SNRLEVEL_FOR_DOWN        15


uint8_t __xdata current_tx_rate = 0;
uint8_t __xdata current_rx_rate = 0;
uint8_t __xdata success_rate = 0;

uint8_t __xdata you_should_dl_power_step_up = 0;
uint8_t __xdata you_should_dl_power_step_down = 0;



void NBFI_Config_Check_State()
{
    #ifdef FIXED_BAUD_RATE
    return;
    #endif // FIXED_BAUD_RATE

    #ifdef NBFi_DEBUG
                    my_sprintf((char __generic *)string, "aver SNR rx=%d, tx=%d, pwr=%d", state.aver_rx_snr, state.aver_tx_snr, nbfi.tx_pwr);
                    SLIP_Send_debug((uint8_t __generic *)string, 50);

                    my_sprintf((char __generic *)string, "success=%d, fault=%d, should=%d", state.success_total, state.fault_total, you_should_dl_power_step_up + you_should_dl_power_step_down);
                    SLIP_Send_debug((uint8_t __generic *)string, 50);

    #endif

    if(you_should_dl_power_step_down && (state.aver_rx_snr < RX_SNRLEVEL_FOR_UP)) you_should_dl_power_step_down = 0;
    if(you_should_dl_power_step_up && (state.aver_rx_snr > RX_SNRLEVEL_FOR_DOWN)) you_should_dl_power_step_up = 0;

    if(state.success_total + state.fault_total >= 5)
    {
        success_rate = state.success_total*5/(state.success_total + state.fault_total);
        state.success_total = state.fault_total = 0;
    }

    if(/*(success_rate >= 4)&&*/(state.aver_tx_snr > TX_SNRLEVEL_FOR_UP) && (state.aver_rx_snr > RX_SNRLEVEL_FOR_UP))
    {
        if(!NBFi_Config_Rate_Change(RX_CONF|TX_CONF, UP))
        {
            NBFi_Config_Tx_Power_Change(DOWN);
            you_should_dl_power_step_down = (1 << 7);
        }
        return;
    }

    if(/*(success_rate >= 4)&&*/(state.aver_tx_snr > TX_SNRLEVEL_FOR_UP))
    {
        if(!NBFi_Config_Rate_Change(TX_CONF, UP)) NBFi_Config_Tx_Power_Change(DOWN);
        return;
    }

    if(/*(success_rate >= 4)&&*/(state.aver_rx_snr > RX_SNRLEVEL_FOR_UP))
    {
        if(!NBFi_Config_Rate_Change(RX_CONF, UP)) you_should_dl_power_step_down = (1 << 7);
        return;
    }

    if((state.aver_tx_snr < TX_SNRLEVEL_FOR_DOWN) /*|| (success_rate < 4)*/)
    {
        if(!NBFi_Config_Tx_Power_Change(UP)) NBFi_Config_Rate_Change(TX_CONF, DOWN);
    }

    if((state.aver_rx_snr < RX_SNRLEVEL_FOR_DOWN) /*|| (success_rate < 4)*/)
    {
        you_should_dl_power_step_up = (1 << 6);
        NBFi_Config_Rate_Change(RX_CONF, DOWN);
        return;
    }

}

bool NBFi_Config_Rate_Change(uint8_t rx_tx, nbfi_rate_direct_t dir )
{
    if((rx_tx & RX_CONF) && NBFi_Config_Tx_Idle())
    {
        if(dir == UP)
        {
            if(++current_rx_rate > NUM_OF_RX_RATES - 1)  current_rx_rate = NUM_OF_RX_RATES - 1;

        }
        if(dir == DOWN)
        {
            if(((int8_t)(--current_rx_rate)) < 0 ) current_rx_rate = 0;
        }
    }

    if(rx_tx & TX_CONF)
    {
        if(dir == UP)
        {
            if(++current_tx_rate > NUM_OF_TX_RATES - 1)  current_tx_rate = NUM_OF_TX_RATES - 1;
            if(RxRateTable[current_rx_rate] == DL_PSK_200) while(TxRateTable[current_tx_rate] != UL_DBPSK_50_PROT_C)  current_tx_rate--;
        }
        if(dir == DOWN)
        {
            if(((int8_t)(--current_tx_rate)) < 0 ) current_tx_rate = 0;
        }
    }
    if((nbfi.tx_phy_channel == TxRateTable[current_tx_rate]) && (nbfi.rx_phy_channel == RxRateTable[current_rx_rate])) return false;

    memcpy_xdata(&nbfi_prev, &nbfi, sizeof(nbfi));
    nbfi.tx_phy_channel = TxRateTable[current_tx_rate];

    if((nbfi.rx_phy_channel == RxRateTable[current_rx_rate]) && dir == DOWN) return true;
    nbfi.rx_phy_channel = RxRateTable[current_rx_rate];

    #ifdef NBFi_DEBUG
        my_sprintf((char __generic *)string, "Baud rate changed to rx=%d, tx=%d", current_rx_rate, current_tx_rate);
        SLIP_Send_debug((uint8_t __generic *)string, 50);
    #endif
    if(!NBFi_Config_Send_Mode(true))
    {
        NBFi_Config_Return();
        return false;
    }
    return true;
}

bool NBFi_Config_Tx_Power_Change(nbfi_rate_direct_t dir)
{
    int8_t old_pwr = nbfi.tx_pwr;
    if(dir == UP)
    {
        nbfi.tx_pwr += 3;
        if(nbfi.tx_pwr > RF_MAX_POWER) nbfi.tx_pwr = RF_MAX_POWER;
    }
    else
    {
        nbfi.tx_pwr -= 3;
        if(nbfi.tx_pwr < RF_MIN_POWER) nbfi.tx_pwr = RF_MIN_POWER;
    }
    return (nbfi.tx_pwr != old_pwr);
}

bool NBFi_Config_Send_Mode(bool ack)
{
    nbfi_transport_packet_t __xdata* ack_pkt =  NBFi_AllocateTxPkt(8);

    if(!ack_pkt)
    {

        return false;
    }
    ack_pkt->phy_data.payload[0] = 0x06;
    ack_pkt->phy_data.payload[1] = (WRITE_PARAM_CMD << 6) + 0x80 + NBFI_PARAM_MODE;
    ack_pkt->phy_data.payload[2] = nbfi.mode;
    ack_pkt->phy_data.payload[3] = nbfi.mack_mode;
    ack_pkt->phy_data.payload[4] = nbfi.tx_phy_channel;
    ack_pkt->phy_data.payload[5] = nbfi.rx_phy_channel;
    ack_pkt->phy_data.payload[6] = nbfi.handshake_mode;
    ack_pkt->phy_data.payload[7] = nbfi.num_of_retries;
    ack_pkt->phy_data.ITER = state.UL_iter & 0x1f;;
    ack_pkt->phy_data.header |= SYS_FLAG;
    if(ack)
    {
        ack_pkt->handshake = HANDSHAKE_SIMPLE;
        ack_pkt->phy_data.header |= ACK_FLAG;
    }
    ack_pkt->state = PACKET_NEED_TO_SEND_RIGHT_NOW;
    return true;
}

void bigendian_cpy(uint8_t __xdata* from, uint8_t __xdata* to, uint8_t len)
{
    for(uint8_t i = 0; i != len; i++)
    {
        to[i] = from[len - i - 1];
    }
}

bool NBFi_Config_Parser(uint8_t __xdata* buf)
{
    switch(buf[0]>>6)
    {
        case READ_PARAM_CMD:
                memset_xdata(&buf[1], 0, 6);
                switch(buf[0]&0x3f)
                {
                    case NBFI_PARAM_MODE:
                        buf[1] = nbfi.mode;
                        buf[2] = nbfi.mack_mode;
                        buf[3] = nbfi.tx_phy_channel;
                        buf[4] = nbfi.rx_phy_channel;
                        buf[5] = nbfi.tx_pwr;
                        buf[6] = nbfi.num_of_retries;
                        break;
                    case NBFI_PARAM_MAXLEN:
                        buf[1] = nbfi.max_payload_len;
                        break;
                    case NBFI_PARAM_TXFREQ:
                        bigendian_cpy((uint8_t __xdata*)&nbfi.tx_freq, &buf[1], 4);
                        break;
                    case NBFI_PARAM_RXFREQ:
                        bigendian_cpy((uint8_t __xdata*)&nbfi.rx_freq, &buf[1], 4);
                        break;
                    case NBFI_PARAM_ANT:
                        buf[1] = nbfi.tx_pwr;
                        buf[2] = nbfi.tx_antenna;
                        buf[3] = nbfi.rx_antenna;
                        break;
                    case NBFI_PARAM_DL_ADD:
                        for(uint8_t i = 0; i != 3; i++)  buf[1 + i] = nbfi.dl_ID[i];
                        break;
                    default:
                        return false;
                        break;
                }
            break;

        case WRITE_PARAM_CMD:
                memcpy_xdata(&nbfi_prev, &nbfi, sizeof(nbfi));
                switch(buf[0]&0x3f)
                {
                    case NBFI_PARAM_MODE:
                        if(buf[1] != 0xff) nbfi.mode = buf[1];
                        if(buf[2] != 0xff) nbfi.mack_mode = buf[2];
                        if(buf[3] != 0xff) nbfi.tx_phy_channel = buf[3];
                        if(buf[4] != 0xff) nbfi.rx_phy_channel = buf[4];
                        if(buf[5] != 0xff) nbfi.tx_pwr = buf[5];
                        if(buf[6] != 0xff) nbfi.num_of_retries = buf[6];
                        break;
                    case NBFI_PARAM_MAXLEN:
                        nbfi.max_payload_len = buf[1];
                        break;
                    case NBFI_PARAM_TXFREQ:
                        bigendian_cpy(&buf[1], (uint8_t __xdata*)&nbfi.tx_freq, 4);
                        break;
                    case NBFI_PARAM_RXFREQ:
                        bigendian_cpy(&buf[1], (uint8_t __xdata*)&nbfi.rx_freq, 4);
                        break;
                    case NBFI_PARAM_ANT:
                        if(buf[1] != 0xff) nbfi.tx_pwr = buf[1];
                        if(buf[2] != 0xff) nbfi.tx_antenna = buf[2];
                        if(buf[3] != 0xff) nbfi.rx_antenna = buf[3];
                        break;
                    case NBFI_PARAM_DL_ADD:
                        for(uint8_t i = 0; i != 3; i++)  nbfi.dl_ID[i] = buf[1 + i];
                        break;
                    default:
                        return false;
                        break;
                }
                nbfi_settings_changed = 1;
            break;
    }
    buf[0] |= 0x80;
    return true;
}

void NBFi_Config_Return()
{
    memcpy_xdata(&nbfi, &nbfi_prev, sizeof(nbfi));

}

void NBFi_Config_Set_Default()
{
    for(uint8_t i = 0; i != sizeof(nbfi_settings_t); i++)
    {
        ((uint8_t __xdata*)&nbfi)[i] = ((uint8_t __code*)&nbfi_set_default)[i];
    }
    nbfi.full_ID[0] = FULL_ID[0];
    nbfi.full_ID[1] = FULL_ID[1];
    nbfi.full_ID[2] = FULL_ID[2];

    nbfi.full_ID[3] = 0;
    nbfi.full_ID[4] = 0;
    nbfi.full_ID[5] = 0;


    //TEMP DIRTY HACK!!!...
    nbfi.temp_ID[0] = nbfi.full_ID[2];
    nbfi.temp_ID[1] = nbfi.full_ID[1];
    nbfi.temp_ID[2] = nbfi.full_ID[0];
    //...TEMP DIRTY HACK!!!


    for(uint8_t i = 0; i != 3; i++) nbfi.dl_ID[i] = fastdladdress.addr[i];   //default DL address

    random_seed = nbfi.full_ID[0] ^ nbfi.full_ID[1] ^ nbfi.full_ID[2] ^ nbfi.full_ID[3] ^ nbfi.full_ID[4] ^ nbfi.full_ID[5];

    state.aver_tx_snr = state.aver_rx_snr = 0;
    current_tx_rate = current_rx_rate = 0;

    #ifdef NBFi_DEBUG
                    my_sprintf((char __generic *)string, "Change mode to defaults");
                    SLIP_Send_debug((uint8_t __generic *)string, 50);
    #endif

}

bool NBFi_Config_Tx_Idle()
{
        switch(nbfi_active_pkt->state)
        {
        case PACKET_DL_DELAY:
        case PACKET_WAIT_ACK:
        case PACKET_WAIT_FOR_EXTRA_PACKETS:
        case PACKET_QUEUED_AGAIN:
            return false;
        default:
            return true;
        }
}
