#include "nbfi.h"
#include "nbfi_config.h"
#include <stdlib.h>
#include "hal.h"
#include "slip.h"


#define POLY 0xa001

//nbfi_packet_t __xdata nbfi_pktBuf[NBFI_PKTBUF_SIZE] = {0};

nbfi_transport_packet_t  __xdata* nbfi_TX_pktBuf[NBFI_TX_PKTBUF_SIZE];
nbfi_transport_packet_t __xdata* nbfi_RX_pktBuf[NBFI_RX_PKTBUF_SIZE];

//uint32_t    nbfi_TX_iterator = 0;
//uint32_t    nbfi_RX_iterator = 0;
uint8_t     nbfi_TXbuf_head = 0;


extern uint8_t  __xdata string[30];

nbfi_transport_packet_t __xdata* NBFi_AllocateTxPkt(uint8_t payload_length)
{
    uint8_t ptr = nbfi_TXbuf_head%NBFI_TX_PKTBUF_SIZE;

    if(nbfi_TX_pktBuf[ptr])
    {
        switch(nbfi_TX_pktBuf[ptr]->state)
        {
        case PACKET_QUEUED:
        case PACKET_QUEUED_AGAIN:
        case PACKET_DL_DELAY:
        case PACKET_WAIT_ACK:
        case PACKET_NEED_TO_SEND_RIGHT_NOW:
            return 0;   // tx buffer is full
        }
        free(nbfi_TX_pktBuf[ptr]);
    }

    nbfi_TX_pktBuf[ptr] = (nbfi_transport_packet_t  __xdata*) malloc(sizeof(nbfi_transport_packet_t) + payload_length);

    if(!nbfi_TX_pktBuf[ptr])
    {
        return 0;
    }

    nbfi_TX_pktBuf[ptr]->state = PACKET_ALLOCATED;

    nbfi_TX_pktBuf[ptr]->phy_data_length = payload_length;

    nbfi_TX_pktBuf[ptr]->handshake = HANDSHAKE_NONE;

    nbfi_TX_pktBuf[ptr]->retry_num = 0;

    nbfi_TX_pktBuf[ptr]->mack_num = 0;

    nbfi_TX_pktBuf[ptr]->phy_data.header = 0;

    nbfi_TXbuf_head++;

    return nbfi_TX_pktBuf[ptr];

}



nbfi_transport_packet_t __xdata* NBFi_AllocateRxPkt(uint8_t header, uint8_t payload_length)
{
    uint8_t ptr = header&0x1f;

    static uint8_t old_ptr = 0;
    static uint32_t old_iter = 0;

    //state.DL_iter = (state.DL_iter & 0xffffffe0) + ptr;
    //if(state.DL_iter < old_iter) state.DL_iter += 32;

    /*for(uint8_t i = old_ptr + 1; (i&0x1f) != ptr; i++)
    {
        nbfi_RX_pktBuf[i&0x1f]->state =  PACKET_LOST;
    }*/
    //old_iter = state.DL_iter;
    //old_ptr = ptr;

    switch(nbfi_active_pkt->state/*nbfi_RX_pktBuf[ptr]->state*/)
    {
        case PACKET_QUEUED:
        case PACKET_QUEUED_AGAIN:
        case PACKET_DL_DELAY:
        case PACKET_WAIT_ACK:
            nbfi_active_pkt = &idle_pkt;
            break;

    }

    if(nbfi_RX_pktBuf[ptr])
    {
        free(nbfi_RX_pktBuf[ptr]);
    }

    //nbfi_RX_pktBuf[(ptr+1)&0x1f]->state = PACKET_FREE;

    nbfi_RX_pktBuf[ptr] = (nbfi_transport_packet_t  __xdata*) malloc(sizeof(nbfi_transport_packet_t) + payload_length);

    if(!nbfi_RX_pktBuf[ptr])
    {
        return 0;
    }

    state.DL_iter = ptr;

    nbfi_RX_pktBuf[ptr]->state = PACKET_ALLOCATED;

    nbfi_RX_pktBuf[ptr]->phy_data_length = payload_length;

    nbfi_RX_pktBuf[ptr]->phy_data.header = header;

    return nbfi_RX_pktBuf[ptr];

}


nbfi_transport_packet_t __xdata* NBFi_GetQueuedTXPkt()
{
    //uint8_t i = nbfi_TXbuf_head - NBFI_TX_PKTBUF_SIZE;

    for(uint8_t i = nbfi_TXbuf_head - NBFI_TX_PKTBUF_SIZE; i != nbfi_TXbuf_head; i++)
    {
        uint8_t ptr = i%NBFI_TX_PKTBUF_SIZE;
        if(nbfi_TX_pktBuf[ptr] == 0) continue;
        switch(nbfi_TX_pktBuf[ptr]->state )
        {
        case PACKET_QUEUED_AGAIN:
        case PACKET_NEED_TO_SEND_RIGHT_NOW:
            return nbfi_TX_pktBuf[ptr];
        }
    }

    for(uint8_t i = nbfi_TXbuf_head - NBFI_TX_PKTBUF_SIZE; i != nbfi_TXbuf_head; i++)
    {
        uint8_t ptr = i%NBFI_TX_PKTBUF_SIZE;
        if(nbfi_TX_pktBuf[ptr] == 0) continue;
        switch(nbfi_TX_pktBuf[ptr]->state )
        {
        case PACKET_DL_DELAY:
        case PACKET_WAIT_ACK:
            if(nbfi_TX_pktBuf[ptr] == nbfi_active_pkt) continue;
             nbfi_TX_pktBuf[ptr]->state = PACKET_QUEUED_AGAIN;
        case PACKET_QUEUED:
        case PACKET_QUEUED_AGAIN:
        //case PACKET_NEED_TO_SEND_RIGHT_NOW:
            return nbfi_TX_pktBuf[ptr];
        }
    }
    return 0;
}

nbfi_transport_packet_t __xdata* NBFi_GetSentTXPkt_By_Iter(uint8_t iter)
{

    for(uint8_t i = (nbfi_TXbuf_head); i != (uint8_t)(nbfi_TXbuf_head  - NBFI_TX_PKTBUF_SIZE); i--)
    {
        uint8_t ptr = i%NBFI_TX_PKTBUF_SIZE;
        if(nbfi_TX_pktBuf[ptr] == 0) continue;
        if(nbfi_TX_pktBuf[ptr]->phy_data.SYS && (nbfi_TX_pktBuf[ptr]->phy_data.payload[0] != 0x05))
        {
            continue;
        }
        switch(nbfi_TX_pktBuf[ptr]->state)
        {
        case PACKET_SENT:
        case PACKET_WAIT_ACK:
        case PACKET_LOST:
        case PACKET_DELIVERED:
//        case PACKET_DELIVERED:
            if(nbfi_TX_pktBuf[ptr]->phy_data.ITER == iter)
            {
                return nbfi_TX_pktBuf[ptr];
            }
            break;
        }

    }
    return 0;
}


uint32_t NBFi_Get_RX_ACK_Mask()
{
    //uint32_t mask = 0;
    uint32_t mask = 0;
    uint32_t one = 1;
    //mask = 0;
    //one = 1;
    for(uint8_t i = ((state.DL_iter - 1)&0x1f); (i&0x1f) != (state.DL_iter&0x1f); i-- , one <<= 1 )
    {
        if(!nbfi_RX_pktBuf[i&0x1f]) continue;
        switch(nbfi_RX_pktBuf[i&0x1f]->state)
        {
            case PACKET_RECEIVED:
//            case PACKET_REC_AND_SENT:
//            case PACKET_PROCESSING:
            case PACKET_PROCESSED:
                mask |= one;
                break;
            default:
                break;
        }
    }
    return mask;
}

bool NBFi_Check_RX_Packet_Duplicate(nbfi_pfy_packet_t __xdata * pkt, uint8_t len)
{
    nbfi_pfy_packet_t __xdata *rec_pkt = &nbfi_RX_pktBuf[state.DL_iter&0x1f]->phy_data;

    for(uint8_t i = 0; i != len; i++)
    {
        if(((uint8_t __xdata*)rec_pkt)[i] != ((uint8_t __xdata*)pkt)[i]) return false;
    }
    return true;
}


nbfi_transport_packet_t __xdata* NBFi_Get_QueuedRXPkt(uint8_t *groupe, uint16_t *total_length)
{
    nbfi_transport_packet_t __xdata* pkt;

    uint32_t i;
    for(i = state.DL_iter + 1; i <= (state.DL_iter + NBFI_RX_PKTBUF_SIZE*2); i++ )
    {
        *groupe = 0;
        *total_length = 0;
        uint8_t total_groupe_len = 0;
        //if(nbfi_RX_pktBuf[i&0x1f] == 0) continue;

        while((pkt = nbfi_RX_pktBuf[(i + *groupe)&0x1f]) && pkt->state == PACKET_RECEIVED)
        {
            if((*groupe) == 0)
            {
                *groupe = 1;

                if((pkt->phy_data.MULTI)&&(pkt->phy_data.SYS)&&(pkt->phy_data.payload[0] == 0x05)) //the start packet of the groupe
                {
                    total_groupe_len = pkt->phy_data.payload[1];
                    *total_length = pkt->phy_data_length - 2;
                    continue;
                }
                else
                {
                    if(pkt->phy_data.MULTI == 1) break;
                    //single packet
                    *total_length = pkt->phy_data_length;
                    pkt->state = PACKET_PROCESSING;
                    break;
                }
            }
            //total_pack_num--;
            (*total_length) += pkt->phy_data_length;
            (*groupe)++;

            if((pkt->phy_data.MULTI) && ((*total_length) < total_groupe_len) && ((*groupe) < NBFI_RX_PKTBUF_SIZE - 1))
            {
                    continue;
            }
            else
            {       if(pkt->phy_data.MULTI && ((*total_length) >= total_groupe_len))
                    {
                        (*total_length) = total_groupe_len;
                        pkt->state = PACKET_PROCESSING;
                        break;
                    }

            }
            break;

        }

        if((*groupe) && (nbfi_RX_pktBuf[(i + (*groupe) - 1)&0x1f]->state == PACKET_PROCESSING))
        {
            return nbfi_RX_pktBuf[i&0x1f];
        }

    }
    return 0;
}

void NBFi_Clear_RX_Buffer()
{
    for(uint8_t i = 0; i != NBFI_RX_PKTBUF_SIZE; i++ )
    {
        nbfi_RX_pktBuf[i]->state = PACKET_CLEARED;
    }
}

void NBFi_Send_Clear_Cmd(uint8_t iter)
{
    nbfi_transport_packet_t __xdata* pkt =  NBFi_AllocateTxPkt(8);
    if(!pkt) return;
    pkt->phy_data.payload[0] = 0x04; //clear RX buffer
    for(uint8_t i = 1; i!=8; i++) pkt->phy_data.payload[i] = 0;
    pkt->phy_data.ITER = iter;
    pkt->phy_data.header |= SYS_FLAG;
    pkt->handshake = HANDSHAKE_NONE;
    pkt->state = PACKET_NEED_TO_SEND_RIGHT_NOW;
    //nbfi_active_pkt = pkt;
    #ifdef NBFi_DEBUG
    my_sprintf((char __generic *)string, "send clear message%d", pkt->phy_data.ITER);
    SLIP_Send_debug((uint8_t __generic *)string, 50);
    #endif
}

uint32_t abc;
uint32_t selection;
nbfi_transport_packet_t __xdata* aab;
void NBFi_Resend_Pkt(nbfi_transport_packet_t __xdata* act_pkt, uint32_t mask)
{
    uint8_t iter = act_pkt->phy_data.ITER;
    mask = (~mask) << 1;
    abc = mask;
    selection = 0;
    for(uint8_t i = (act_pkt->mack_num&0x3f) - 1; i != 0; i-- )
    {
      selection |= (((uint32_t)1) << i);
    }
    mask &= selection;//nbfi.mack_mode;
    uint32_t one = 1;
    //abc = iter;
    //uint32_t msk  = mask;
    if(mask > 0xfff)
    {

        mask = 0;
    }
    #ifdef NBFi_DEBUG
    my_sprintf((char __generic *)string, "ready to retry%d:%04X%04X", iter, (uint16_t)(mask>>16), (uint16_t)(mask&0xffff));
    SLIP_Send_debug((uint8_t __generic *)string, 30);
    #endif
    nbfi_transport_packet_t __xdata* pkt = 0, *last_resending_pkt = 0;
    do
    {
        pkt = NBFi_GetSentTXPkt_By_Iter(iter&0x1f);
        aab = pkt;
        abc = mask;
        if(!pkt)
        {
            aab = pkt;
            abc = mask;
            break;
        }
        if(one&mask)
        {
           mask &= ~one;
           if(++pkt->retry_num > nbfi.num_of_retries) pkt->state = PACKET_LOST;
           else
           {
               pkt->state = PACKET_QUEUED_AGAIN;
               if(last_resending_pkt == 0) last_resending_pkt = pkt;
               state.fault_total++;
           }
        }
        else
        {
            pkt->state = PACKET_DELIVERED;
            state.success_total++;
        }

        iter--;
        one <<= 1;
    }while(mask&&one);

    if(last_resending_pkt)
    {
        last_resending_pkt->phy_data.ACK = 1;
        last_resending_pkt->mack_num = act_pkt->mack_num - (act_pkt->phy_data.ITER - last_resending_pkt->phy_data.ITER);
        if(act_pkt->phy_data.ITER < last_resending_pkt->phy_data.ITER) last_resending_pkt->mack_num += 32;
        last_resending_pkt->mack_num |= 0x80;
    }
    else if((act_pkt->mack_num > 1) && (mask == 0))  //all packets delivered, send message to clear receiver's input buffer
    {
         //for(uint8_t i = 1; i!=8; i++) pkt->phy_data.payload[i] = 0;
         NBFi_Send_Clear_Cmd(nbfi_active_pkt->phy_data.ITER);
    }

}



uint16_t CRC16(uint8_t __generic *buf, uint16_t len, uint16_t crc)
{
    while (len--)
    {
        crc ^= *buf++;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    }
    return crc;
}

static unsigned char CRC8byte(unsigned char __xdata data)
{
   uint8_t crc = 0;
   if(data & 1)     crc ^= 0x5e;
   if(data & 2)     crc ^= 0xbc;
   if(data & 4)     crc ^= 0x61;
   if(data & 8)     crc ^= 0xc2;
   if(data & 0x10)  crc ^= 0x9d;
   if(data & 0x20)  crc ^= 0x23;
   if(data & 0x40)  crc ^= 0x46;
   if(data & 0x80)  crc ^= 0x8c;
   return crc;
}

uint8_t CRC8(uint8_t __generic* data, uint8_t len)
{
    uint8_t crc = 0;
    for(uint8_t i = 0; i < len; i++)
    {
        crc = CRC8byte(data[i] ^ crc);
    }
    return crc;
}

uint32_t NBFi_ConvertFreq(uint32_t f_ul)
{
    return NBFI_DL_FREQ_BASE + (f_ul - NBFI_UL_FREQ_BASE ) * 363 / 100 ;
}

/*nbfi_packet_t __xdata* NBFi_AllocatePkt()
{
    for(int i=0; i<NBFI_PKTBUF_SIZE; i++)
    {
        if(nbfi_pktBuf[i].packet_state == PACKET_FREE)
        {
            memset_xdata(&nbfi_pktBuf[i], 0x00, sizeof(nbfi_pktBuf[0]));// TODO may be omitted for size optimization
            nbfi_pktBuf[i].packet_state = PACKET_ALLOCATED;
            return &nbfi_pktBuf[i];
        }
    }
    return 0;
}*/

/*nbfi_packet_t __xdata* NBFi_GetQueuedPkt()
{
    for(int i=0; i<NBFI_PKTBUF_SIZE; i++)
    {
        if(nbfi_pktBuf[i].packet_state == PACKET_QUEUED)
        {
            nbfi_pktBuf[i].packet_state = PACKET_PROCESSING;
            return &nbfi_pktBuf[i];
        }
    }
    return 0;
}*/
