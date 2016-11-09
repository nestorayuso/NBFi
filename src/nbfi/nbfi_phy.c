#include <ax8052f143.h>
#include <libmftypes.h>
#include <libmfcrc.h>

#include "nbfi.h"
#include "nbfi_config.h"
#include "rf.h"
#include "xtea.h"
#include "zigzag.h"
#include "hal.h"

/* PHYSICAL LAYER */
void DL_Receive_Timeout_cb(struct wtimer_desc __xdata *desc);

uint32_t __xdata last_pkt_crc = 0;

const uint8_t __code nbfi_preambula[] = {0x97, 0x15, 0x7A, 0x6F};
const uint8_t __code protocolC_preambula[] = {0x97, 0x15, 0x7A};

static uint8_t __xdata ul_buf[64];

// Send basic NBFi uplink for remote nodes
uint32_t __xdata tx_freq, rx_freq;
nbfi_status_t NBFi_TX_ProtocolD(nbfi_transport_packet_t __generic* pkt)
{
    uint8_t len = 0;

    // Append iterator
    //pkt->header |= (nbfi.ul_iter & UL_ITER_MASK);

    /* Prepare packet */
    memset_xdata(ul_buf,0,sizeof(ul_buf));

    for(int i=0; i<sizeof(nbfi_preambula); i++)
    {
        ul_buf[len++] = nbfi_preambula[i];
    }

    ul_buf[len++] = nbfi.temp_ID[2];
    ul_buf[len++] = nbfi.temp_ID[1];
    ul_buf[len++] = nbfi.temp_ID[0];
    ul_buf[len++] = pkt->phy_data.header;

    memcpy_xdatageneric(&ul_buf[len], pkt->phy_data.payload, 8);

    if(XTEA_Enabled() && XTEA_Available())
    {
        XTEA_Encode(&ul_buf[len]);
    }
    len += 8;

    ul_buf[len++] = CRC8(pkt->phy_data.payload, 8);

    //last_pkt_crc = pkt->packet_crc = CRC16(ul_buf + 3, 14, 0);  // Store it to calculate next RX freq in ALOHA mode
    last_pkt_crc = CRC16(ul_buf + 3, 14, 0);  // Store it to calculate next RX freq in ALOHA mode

    ul_buf[len++] = (uint8_t)(last_pkt_crc >> 8);
    ul_buf[len++] = (uint8_t)(last_pkt_crc);

    /* Transmit with 1st coding part */
    ZIGZAG_Append(&ul_buf[3], &ul_buf[len], 0);  // len is not updated here, but it is assumed that ul_buf grew by 16, don't forget when sending to transmit!


    if(nbfi.tx_freq)    tx_freq = nbfi.tx_freq ;
    //else                tx_freq = NBFI_UL_FREQ_BASE + ((last_pkt_crc%276)*100);
    else tx_freq = NBFI_UL_FREQ_BASE + ((*((const uint32_t __code*)FULL_ID)%276)*100);

    RF_Init(UL_DBPSK_50, nbfi.tx_antenna, nbfi.tx_pwr, tx_freq);
    RF_Transmit(ul_buf, len + ZIGZAG_LEN, BLOCKING, PADDING_4TO1);

    /* Transmit with 2nd coding part */
    ZIGZAG_Append(&ul_buf[3], &ul_buf[len], 1);

    if(nbfi.tx_freq)    tx_freq = nbfi.tx_freq + 31000;
    else tx_freq = NBFI_UL_FREQ_BASE + ((*((const uint32_t __code*)FULL_ID)%276)*100) + 31000;

    RF_Init(UL_DBPSK_50, nbfi.tx_antenna, nbfi.tx_pwr, tx_freq);
    RF_Transmit(ul_buf, len + ZIGZAG_LEN, PADDING_4TO1, BLOCKING);

    //RF_Deinit();

    return OK;
}

nbfi_status_t NBFi_TX_ProtocolC(nbfi_transport_packet_t __generic* pkt)
{
    uint8_t len = 0;

    // Append iterator
//    pkt->header |= (nbfi.ul_iter & 0xF);

    /* Prepare packet */
    memset_xdata(ul_buf,0,sizeof(ul_buf));

    for(int i=0; i<sizeof(protocolC_preambula); i++)
    {
        ul_buf[len++] = protocolC_preambula[i];
    }

    ul_buf[len++] = 0x6f;//nbfi.full_ID[2];
    ul_buf[len++] = nbfi.full_ID[1];
    ul_buf[len++] = nbfi.full_ID[0];
    ul_buf[len++] = pkt->phy_data.header;

    memcpy_xdatageneric(&ul_buf[len], pkt->phy_data.payload, 8);

    if(XTEA_Enabled() && XTEA_Available())
    {
        XTEA_Encode(&ul_buf[len]);
    }
    len += 8;

    //pkt->packet_crc = crc_crc32_msb(ul_buf + 3, 12, 0xFFFFFFFF) ^ 0xFFFFFFFF;// Store it to calculate next RX freq in ALOHA mode

    /*if(!(pkt->header & SYS_FLAG))*/

    last_pkt_crc = crc_crc32_msb(ul_buf + 3, 12, 0xFFFFFFFF) ^ 0xFFFFFFFF;

    ul_buf[len++] = (uint8_t)(last_pkt_crc >> 24);
    ul_buf[len++] = (uint8_t)(last_pkt_crc >> 16);
    ul_buf[len++] = (uint8_t)(last_pkt_crc >> 8);
    ul_buf[len++] = (uint8_t)(last_pkt_crc);

    /* Transmit with 1st coding part */
    ZIGZAG_Append(&ul_buf[3], &ul_buf[len], 0);  // len is not updated here, but it is assumed that ul_buf grew by 16, don't forget when sending to transmit!

    if(nbfi.tx_freq)    tx_freq = nbfi.tx_freq ;
    else                tx_freq = NBFI_UL_FREQ_BASE + ((*((const uint32_t __code*)FULL_ID)%276)*100);

    RF_Init(nbfi.tx_phy_channel, nbfi.tx_antenna, nbfi.tx_pwr, tx_freq);
    RF_Transmit(ul_buf, len + ZIGZAG_LEN, PADDING_4TO1, BLOCKING);

    /* Transmit with 2nd coding part */
    ZIGZAG_Append(&ul_buf[3], &ul_buf[len], 1);

    if(nbfi.tx_freq)    tx_freq = nbfi.tx_freq + 31000;
    else                tx_freq = (NBFI_UL_FREQ_BASE + ((*((const uint32_t __code*)FULL_ID)%276)*100) + 31000);

    RF_Init(nbfi.tx_phy_channel, nbfi.tx_antenna, nbfi.tx_pwr, tx_freq);

    RF_Transmit(ul_buf, len + ZIGZAG_LEN, PADDING_4TO1, BLOCKING);

    //RF_Deinit();

   // if(nbfi.mode == CRX) NBFi_RX_DL();

    return OK;
}



//TODO make const
const uint8_t __code corr_packet[] = {0x97, 0x15, 0x7A, 0x6f, 0, 0, 0, 0, 0, 2, 3, 4, 5, 6, 7, 0xb5, 0x28, 0xa9, 0x51};

nbfi_status_t NBFi_TX_Correction()
{
    RF_Init(UL_DBPSK_50, nbfi.tx_antenna, nbfi.tx_pwr, nbfi.tx_freq);
    RF_Transmit((uint8_t __generic*)corr_packet, sizeof(corr_packet), PADDING_4TO1, BLOCKING);
    NBFi_RX_Controller();
    return OK;
}

uint8_t __xdata hop;
struct wtimer_desc __xdata hopping_desc;
void Hopping(struct wtimer_desc __xdata *desc)
{
    RF_SetFreq(868800000 + (1000*(uint32_t)hop));
//    RF_Transmit((uint8_t __generic*)(&corr_packet[2*hop]), 2, PADDING_4TO1, NONBLOCKING);

    hop++;
    if(hop<10)
    {
        ScheduleTask(desc, 0, ABSOLUTE, desc->time + MILLISECONDS(440));
    }
}

nbfi_status_t NBFi_TX_Hopping()
{
    hop = 0;
    RF_Init(UL_DBPSK_50, 0, nbfi.tx_pwr, 868800000);

    ScheduleTask(&hopping_desc, &Hopping, RELATIVE, 20);
    while(hop < 10 || rf_busy)
    {
        wtimer_runcallbacks();
    }

    RF_Deinit();

    return OK;
}


/*
// Send basic NBFi downlink for gateway or repeater
nbfi_status_t NBFi_TX_DL(nbfi_packet_t __generic* pkt)
{
    static uint8_t __xdata tx_packet[32];

    if((pkt->payload_length == 0)&&(pkt->payload_length > 20)) return ERR; // length check
    RF_Init(DL_PSK_200, 0, 20, nbfi.tx_freq);
    RF_SetDstAddress(nbfi.temp_ID);
//    RF_Transmit(pkt->payload, pkt->payload_length, NO_PADDING, BLOCKING);
    RF_Deinit();
    return OK;
}
*/
/*
nbfi_status_t NBFi_TX_FASTDL(nbfi_transport_packet_t __generic* pkt)//FastDL_Send(uint8_t __xdata* buf, uint8_t len)
{
    nbfi_status_t result;
    if((pkt->phy_data_length==0)&&(pkt->phy_data_length>240)) return ERR; // len check
    if(rf_busy) return ERR_RF_BUSY;
    if(result = RF_Init(DL_PSK_FASTDL, nbfi.tx_antenna, nbfi.tx_pwr, NBFI_FASTDL_FREQ_BASE) != OK) return result;
    RF_SetDstAddress((uint8_t __generic *)&fastdladdress);
    if(result = RF_Transmit(&pkt->phy_data.payload[0], pkt->phy_data_length, NO_PADDING, BLOCKING) != OK) return result;
    RF_Deinit();
    return OK;
}*/


nbfi_status_t NBFi_TX(nbfi_transport_packet_t __generic* pkt)
{
    nbfi_status_t result;
    //if(rf_busy) return ERR_RF_BUSY;
    //rf_busy = true;
    if((pkt->phy_data_length==0)&&(pkt->phy_data_length>240)) return ERR; // len check
    switch(nbfi.tx_phy_channel)
    {
    case UL_DBPSK_50_PROT_C:
    case UL_DBPSK_400_PROT_C:
        return NBFi_TX_ProtocolC(pkt);
    case UL_DBPSK_50_PROT_D:
        return NBFi_TX_ProtocolD(pkt);
    case UL_PSK_FASTDL:
    case UL_PSK_200:
    case UL_PSK_500:

        /*if(nbfi.tx_phy_channel == DL_PSK_FASTDL)
        {
            if(nbfi.tx_freq)    tx_freq = nbfi.tx_freq;
            else tx_freq = NBFI_FASTDL_FREQ_BASE;
//            RF_SetDstAddress((uint8_t __generic *)&fastdladdress);
        }
        else
        {
            if(nbfi.tx_freq)    tx_freq = nbfi.tx_freq;
            else tx_freq = NBFI_DL_FREQ_BASE;
        }*/

        if(nbfi.tx_freq)    tx_freq = nbfi.tx_freq;
            else tx_freq = NBFI_DL_FREQ_BASE;
        RF_SetDstAddress(nbfi.dl_ID);

        //RF_SetDstAddress((uint8_t __generic *)&fastdladdress);

        if(result = RF_Init(nbfi.tx_phy_channel, nbfi.tx_antenna, nbfi.tx_pwr, tx_freq) != OK) return result;
        if(result = RF_Transmit(&pkt->phy_data.header, pkt->phy_data_length + 1, NO_PADDING, BLOCKING) != OK) return result;
        state.UL_total++;
        break;

    }

    //RF_Deinit();
    return OK;
}


nbfi_status_t NBFi_RX_Controller()
{
    switch(nbfi.mode)
    {
    case  DRX:
        switch(nbfi_active_pkt->state)
        {
        case PACKET_DL_RX:
        case PACKET_WAIT_ACK:
        case PACKET_QUEUED_AGAIN:
            if(rf_state != STATE_RX) return NBFi_RX();
        default:
            if(rf_state != STATE_OFF) return RF_Deinit();
        }
    case CRX:
    case TRANSPARENT:
        if(rf_state != STATE_RX) return NBFi_RX();
    }
}


nbfi_status_t NBFi_RX()
{
    nbfi_status_t result;

    switch(nbfi.rx_phy_channel)
    {
        case DL_PSK_200:
        case DL_PSK_500:
             if(nbfi.rx_freq) rx_freq = nbfi.rx_freq;
             //else  rx_freq = NBFI_DL_FREQ_BASE + ((last_pkt_crc%276)*363);
             else rx_freq = NBFI_DL_FREQ_BASE + ((*((const uint32_t __code*)FULL_ID)%276)*363);
             RF_SetLocalAddress((uint8_t __generic *)&nbfi.temp_ID[0]);
             break;

        case DL_PSK_FASTDL:
             if(nbfi.rx_freq) rx_freq = nbfi.rx_freq;
             else rx_freq = NBFI_DL_FREQ_BASE;
             RF_SetLocalAddress((uint8_t __generic *)&fastdladdress);
             break;
    }
    result = RF_Init(nbfi.rx_phy_channel, nbfi.rx_antenna, 0, rx_freq);
    return result;
}

/*
/// Start downlink RX on remote node
nbfi_status_t NBFi_RX_DL()
{
    if(nbfi.rx_freq)    rx_freq = nbfi.rx_freq;
    else                rx_freq = NBFI_DL_FREQ_BASE + ((last_pkt_crc%276)*363);

    RF_Init(UL_PSK_200, nbfi.rx_antenna, 0, rx_freq);
    return OK;
}
*/


