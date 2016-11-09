#include "nbfi.h"
#include "nbfi_config.h"
#include "rf.h"
#include "hal.h"
#include "misc.h"

bool __xdata rf_busy = 0;
struct axradio_address rf_destination;

nbfi_rf_state_s rf_state = STATE_OFF;

void RF_SetFreq(uint32_t freq)
{
    axradio_phy_chanfreq[0] = axradio_conv_freq_fromhz(freq);//TODO fix freq calc
    // Dirty fix for insufficient arithmetics precision
    if(freq > 800000000)    axradio_phy_chanfreq[0]+=34; //868MHz
    else                    axradio_phy_chanfreq[0]+=18; //446MHz
}


#define FEM_MAX_DBM 26
#define FEM_GAIN 11
#define AX5043_MAX_DBM 15
//#define TX_RX_SEL PORTC_0

#define PA_DIFFERENTIAL (0x1)
#define PA_SINGLE_ENDED (0x2)
#define PA_SHAPING      (0x4)



#ifdef SDR_RASP
const uint16_t AX5043_power[26] = {0x00aa, 0x00bf, 0x00d1, 0x00ec, 0x010f, 0x0132, 0x0156, 0x017f, 0x01af, 0x1e0, 0x207, 0x244, 0x290, 0x2eb, 0x35e, 0x3d6, 0x406, 0x4a9, 0x57c, 0x600, 0x700, 0x800, 0x9d4, 0xc00, 0xf00, 0xfff};
#endif // DEVICE_SDR

#ifdef SDR_BS3
const uint16_t power_table[]={5, 6, 6, 7, 8, 8, 8, 9, 9, 10, 10, 11, 11, 12, 13, 14, 14, 15, 16, 18, 19, 21, 22, 24, 27, 30, 33, 37, 42, 47, 54, 61, 68, 75, 81, 87, 97, 110, 126, 142, 157, 172, 197, 239};//-15..28dbm
#endif // SDR_BS3


#ifdef DEVICE_REMOTE
const uint16_t RFM6599_PowerTable[26] = {0x00aa, 0x00bf, 0x00d1, 0x00ec, 0x010f, 0x0132, 0x0156, 0x017f, 0x01af, 0x1e0, 0x207, 0x244, 0x290, 0x2eb, 0x35e, 0x3d6, 0x406, 0x4a9, 0x57c, 0x600, 0x700, 0x800, 0x9d4, 0xc00, 0xf00, 0xfff};
#endif // DEVICE

#ifdef AMPER_2_3
const uint16_t AX5043_power[26] = {0x00aa, 0x00bf, 0x00d1, 0x00ec, 0x010f, 0x0132, 0x0156, 0x017f, 0x01af, 0x1e0, 0x207, 0x244, 0x290, 0x2eb, 0x35e, 0x3d6, 0x406, 0x4a9, 0x57c, 0x600, 0x700, 0x800, 0x9d4, 0xc00, 0xf00, 0xfff};
#endif // AMPER_2_3



struct axradio_address  fastdladdress = {
	{ 0x00, 0x30, 0x39, 0x00}
};


void RF_SetModeAndPower(int8_t dBm, rf_direction_t mode, rf_antenna_t ant)
{

    if(dBm > RF_MAX_POWER) dBm = RF_MAX_POWER;
    if(dBm < RF_MIN_POWER) dBm = RF_MIN_POWER;


    if(dBm > FEM_MAX_DBM) dBm = FEM_MAX_DBM;



    if(mode == TX)
    {
        #ifdef AMPER_2_3
        if(dBm > AX5043_MAX_DBM) dBm = AX5043_MAX_DBM;
        AX5043_TXPWRCOEFFB1 = AX5043_power[dBm + 10] >> 8;
        AX5043_TXPWRCOEFFB0 = AX5043_power[dBm + 10] &0xFF;
        // select differential PA
        AX5043_MODCFGA = PA_SINGLE_ENDED;
        #endif

        #ifdef SDR_RASP
        //PA_EN = 1;
        // set power
        AX5043_MODCFGA = PA_DIFFERENTIAL;
        AX5043_TXPWRCOEFFB1 = AX5043_power[dBm+10] >> 8;
        AX5043_TXPWRCOEFFB0 = AX5043_power[dBm+10] &0xFF;
        #endif

        #ifdef SDR_BS3
        PIN_SET_OUTPUT(PA_450_EN_DIR,  PA_450_EN_PIN);
        PA_450_EN = 1;
        // set power
        AX5043_MODCFGA = PA_SINGLE_ENDED;
        AX5043_TXPWRCOEFFB1 = power_table[dBm + 15] >> 8;
        AX5043_TXPWRCOEFFB0 = power_table[dBm + 15] &0xFF;
        #endif

        #ifdef DEVICE_REMOTE

        PIN_SET_OUTPUT(PA_EN_DIR,  PA_EN_PIN);

        PIN_SET_OUTPUT(TX_ANT_DIR, TX_ANT_PIN);

        if(dBm <= AX5043_MAX_DBM)
        {
            // set power
            AX5043_TXPWRCOEFFB1 = RFM6599_PowerTable[dBm + 10] >> 8;
            AX5043_TXPWRCOEFFB0 = RFM6599_PowerTable[dBm + 10] &0xFF;

            // select PA output
            AX5043_MODCFGA = PA_DIFFERENTIAL;
            // FEM PA bypass
            //TX_RX_SEL = RX;
            PA_EN = HIGH;//LOW;
        }
        else
        {
            // set power, considering FEM gain
            AX5043_TXPWRCOEFFB1 = RFM6599_PowerTable[dBm - FEM_GAIN + 10] >> 8;
            AX5043_TXPWRCOEFFB0 = RFM6599_PowerTable[dBm - FEM_GAIN + 10] &0xFF;

            // select single-ended PA
            AX5043_MODCFGA = PA_SINGLE_ENDED;
            //AX5043_MODCFGA = PA_DIFFERENTIAL;
            // enable FEM PA
            //TX_RX_SEL = TX;
            PA_EN = HIGH;
        }
        #endif // DEVICE
    }
    else // mode == RX or IDLE
    {
        #ifdef AMPER_2_3
        AX5043_MODCFGA = PA_DIFFERENTIAL | PA_SHAPING;
        #endif

        #ifdef SDR_RASP
        AX5043_MODCFGA = PA_DIFFERENTIAL | PA_SHAPING;
        //PA_EN = 0;
        #endif

        #ifdef SDR_BS3
        PIN_SET_OUTPUT(PA_450_EN_DIR,  PA_450_EN_PIN);
        PA_450_EN = 0;
        #endif
        #ifdef DEVICE_REMOTE
        PIN_SET_OUTPUT(RX_ANT_DIR, RX_ANT_PIN);
        RX_ANT = ant;

        // select PA output
        AX5043_MODCFGA = PA_DIFFERENTIAL | PA_SHAPING;

        // enable FEM PA bypass
        //TX_RX_SEL = RX;
        PA_EN = LOW;
        #endif // DEVICE
    }

}

nbfi_status_t RF_Init(  nbfi_phy_channel_t  phy_channel,
                        rf_antenna_t        antenna,
                        int8_t              power,
                        uint32_t            freq)
{
    uint8_t er;


    if(rf_busy) return ERR_RF_BUSY;

    rf_busy = true;

    nbfi_phy_channel = phy_channel;


    PIN_SET_OUTPUT(TCXO_DIR, TCXO_PIN); // Enable TCXO...
    TCXO = HIGH;
    //delay_ms(10);   // ... and let it settle

    PORTR = 0x0B;
    DIRR = 0x15;

    switch(phy_channel)
    {
    case UL_DBPSK_100:
    case UL_DBPSK_50:
    case UL_DBPSK_500:
    case UL_PSK_200:
    case UL_PSK_FASTDL:
    case UL_PSK_500:
    case UL_DBPSK_50_PROT_C:
    case UL_DBPSK_50_PROT_D:
    case UL_DBPSK_400_PROT_C:
        ax5043_set_constants();
        er = axradio_init();    // Init radio registers
        if (er != AXRADIO_ERR_NOERROR)
        {
            rf_busy = false; return ERR;
        }
        er = axradio_set_mode(AXRADIO_MODE_ASYNC_TRANSMIT);
        if (er != AXRADIO_ERR_NOERROR)
        {
            rf_busy = false;return ERR;
        }
        RF_SetFreq(freq);
        RF_SetModeAndPower(power, TX, antenna);
        rf_busy = false;
        rf_state = STATE_TX;
        return OK;


    case DL_PSK_200:
    case DL_PSK_FASTDL:
    case DL_PSK_500:
        //delay_ms(10);   // Let it settle
        ax5043_set_constants();
        RF_SetModeAndPower(power, RX, antenna);
        if((phy_channel == DL_PSK_FASTDL))
        {
            //RF_SetFreq(NBFI_FASTDL_FREQ_BASE);
            RF_SetLocalAddress((uint8_t __generic *)&fastdladdress);
        }

        RF_SetLocalAddress((uint8_t __generic *)nbfi.temp_ID);

        RF_SetFreq(freq);

        er = axradio_init();    // Init radio registers
        if (er != AXRADIO_ERR_NOERROR)
        {
             rf_busy = false; return ERR;
        }
        er = axradio_set_mode(AXRADIO_MODE_ASYNC_RECEIVE);
        rf_busy = false;
        if (er != AXRADIO_ERR_NOERROR)
        {
            return ERR;
        }
        rf_state = STATE_RX;
        return OK;
    }
    TCXO = LOW;
    rf_busy = false;
    return ERR;
}

nbfi_status_t RF_Deinit()
{
    uint8_t er;

    if(rf_busy) return ERR_RF_BUSY;
    rf_busy = true;
    er = axradio_set_mode(AXRADIO_MODE_OFF);
    RF_SetModeAndPower(0, RX, 0);
    TCXO = LOW;
    rf_busy = false;
    if (er != AXRADIO_ERR_NOERROR) return ERR;
    rf_state = STATE_OFF;
    return OK;
}

void RF_SetDstAddress(uint8_t __generic * addr)
{
    for(uint8_t i = 0; i !=3; i++) rf_destination.addr[i] = addr[i];
}

void RF_SetLocalAddress(uint8_t __generic * addr)
{
    struct axradio_address_mask     localaddress = {{0,0,0,0},{0xff, 0xff, 0xff, 0x00}};
    localaddress.addr[0] = addr[0];
    localaddress.addr[1] = addr[1];
    localaddress.addr[2] = addr[2];
    axradio_set_local_address(&localaddress);
}

//uint32_t ccc;
nbfi_status_t RF_Transmit(uint8_t __generic* pkt, uint8_t len,  rf_padding_t padding, rf_blocking_t blocking)
{
    if(rf_busy) return ERR_RF_BUSY;

    //ccc = (uint32_t) pkt;

    rf_busy = true;

    axradio_transmit(&rf_destination, pkt, len, padding);

    if(blocking == BLOCKING)
    {
        //uint8_t  iesave = IE & 0x80;
        while(1) // Wait for TX complete
        {
            //EA = 0;
            if(!rf_busy) break;
            //wtimer_idle(WTFLAG_CANSTANDBY);
            //IE |= iesave;
            wtimer_runcallbacks();
        }
        //IE |= iesave;
    }
    //rf_busy = false;
    return OK;
}
