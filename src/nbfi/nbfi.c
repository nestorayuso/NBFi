//
// S. Omelchenko
// 18.02.2016

#include "nbfi.h"
#include "nbfi_config.h"
#include "rf.h"
#include "xtea.h"
#include "slip.h"
#include "hal.h"
#include <stdlib.h>


#ifdef NBFi_DEBUG
uint8_t  __xdata string[30];
#endif

nbfi_phy_channel_t nbfi_phy_channel;

nbfi_state_t __xdata state = {0,0,0,0};

const uint32_t NBFI_DL_DELAY[9] = {SECONDS(30), SECONDS(60), SECONDS(60), SECONDS(30), SECONDS(5), SECONDS(5), SECONDS(5), SECONDS(5), 0};
const uint32_t NBFI_DL_LISTEN_TIME[3] = {SECONDS(60), SECONDS(10), MILLISECONDS(500)};
const uint32_t NBFI_DL_ADD_RND_LISTEN_TIME[3] = {SECONDS(1), SECONDS(1), MILLISECONDS(1000)};



nbfi_transport_packet_t __xdata idle_pkt = {PACKET_FREE, HANDSHAKE_NONE, 0, 0, 0, {0,0} };
nbfi_transport_packet_t __xdata* __xdata nbfi_active_pkt = &idle_pkt;
nbfi_packet_state_t nbfi_active_pkt_old_state;


struct wtimer_desc __xdata nbfi_processTask_desc;
struct wtimer_desc __xdata dl_receive_desc;
struct wtimer_desc __xdata wait_for_extra_desc;


//received_packet_t __xdata* rx_pkt;

//uint8_t __xdata rx_pkt_len = 0;// TODO, to refactor
int16_t __xdata rssi = 0;
int16_t __xdata offset = 0;

rx_handler_t __xdata  rx_handler;

uint8_t __xdata not_acked = 0;

uint8_t __xdata has_smth_to_transmit;

uint8_t __xdata aver_tx_snr = 0;
uint8_t __xdata aver_rx_snr = 0;
int16_t __xdata noise = -150;

int16_t __xdata noise_summ = 0;
uint8_t __xdata noise_cntr = 0;
int16_t __xdata noise_min = -150;
uint8_t __xdata noise_min_cntr = 2;


nbfi_status_t NBFi_Send(uint8_t __generic* payload, uint8_t length)
{
    nbfi_transport_packet_t __xdata* packet;
    //nbfi_transport_packet_t __xdata* first_in_groupe;
    uint8_t groupe = 0;
    uint8_t len = length;

    do
    {
        uint8_t l;
        uint8_t first = 0;
        if(length > nbfi.max_payload_len)
        {
            first = (groupe == 0)*2;
            l = nbfi.max_payload_len - first;
        }
        else l = length;
        //l = (length > nbfi.max_payload_len)?nbfi.max_payload_len:length;
        packet =  NBFi_AllocateTxPkt(l + first);
        if(!packet) return ERR_BUFFER_FULL;
        memcpy_xdata(&packet->phy_data.payload + first, (void const*)&payload[groupe * nbfi.max_payload_len - 2*(groupe != 0)], l);
        packet->state = PACKET_QUEUED;
        packet->handshake = nbfi.handshake_mode;
        packet->phy_data.ITER = state.UL_iter++ & 0x1f;
        if(l < length)
        {
            packet->phy_data.MULTI = 1;
            if(groupe == 0) //the start packet of the groupe must be system
            {
                packet->phy_data.SYS = 1;
                packet->phy_data.payload[0] = 0x05;
                packet->phy_data.payload[1] = len;
                //first_in_groupe = packet;
            }
        }

        length -= l;
        groupe++;
        if(nbfi.handshake_mode != HANDSHAKE_NONE)
        {
            if((length == 0) && (groupe == 1))
            {
                if(((state.UL_iter) % nbfi.mack_mode) == 0)
                {
                    packet->phy_data.ACK = 1;
                    packet->mack_num = not_acked + 1;//nbfi.mack_mode;
                    not_acked = 0;
                }
                else not_acked++;
            }
            else   //the last packet of groupe must be acked
            {
                if(length == 0)
                {
                    packet->phy_data.ACK = 1;
                    packet->phy_data.MULTI = 1;
                    packet->mack_num = groupe + not_acked;//((state.UL_iter - groupe)%nbfi.mack_mode);
                    not_acked = 0;
                    //first_in_groupe->phy_data.payload[1] = len;
                }
            }

        }
        //if(packet->phy_data.ACK)

       /* if(not_acked > 2)
        {
            not_acked = 2;
        }*/

    }while(length);

    NBFi_Force_process();
    return OK;
}

uint8_t numa;
void NBFi_ProcessRxPackets()
{
    nbfi_transport_packet_t __xdata* pkt;
    uint8_t __xdata *data;
    uint8_t groupe;
    uint16_t total_length;
    while(1)
    {

        pkt = NBFi_Get_QueuedRXPkt(&groupe, &total_length);

        if(!pkt)    return;

        data = malloc(total_length);

        if(!data)   return;

        uint8_t iter = pkt->phy_data.ITER;
        uint16_t memcpy_len = total_length;
        numa = total_length;
        for(uint8_t i = 0; i != groupe; i++)
        {
            uint8_t len;
            uint8_t first = 0;
            if((i == 0)&&(groupe > 1)) {len = 6; first = 2;}
            else len = (memcpy_len>=nbfi.max_payload_len)?nbfi.max_payload_len:memcpy_len%nbfi.max_payload_len;
            memcpy_xdata(data + i*nbfi.max_payload_len - 2*(i != 0), (void const*)(&nbfi_RX_pktBuf[(iter + i)&0x1f]->phy_data.payload[first]), len);
            memcpy_len -= len;
            if(nbfi_RX_pktBuf[(iter + i)&0x1f]->phy_data.ACK) nbfi_RX_pktBuf[(iter + i)&0x1f]->state = PACKET_CLEARED;
            else nbfi_RX_pktBuf[(iter + i)&0x1f]->state = PACKET_PROCESSED;
            //nbfi_RX_pktBuf[(iter + i)&0x1f]->state = PACKET_PROCESSED;
        }

        SLIP_Send_debug((uint8_t __generic *)data, total_length);

        //SLIP_Send(SLIP_CMD_RECEIVED_DL, (uint8_t __generic *)data, total_length);

        rx_handler((uint8_t __generic *)data, total_length);

        free(data);
    }

}

uint8_t __xdata* ppp;
struct axradio_status __xdata *aa;
void NBFi_ParseReceivedPacket(struct axradio_status __xdata *st)
{
    // Get packet from within axsem lib

    aa = st;
    state.DL_total++;
    noise_min_cntr = 5;
    uint8_t snr;
    if(st->u.rx.phy.rssi < noise) snr = 0;
    else snr = (st->u.rx.phy.rssi - noise) & 0xff;

    state.aver_rx_snr = ((uint16_t)state.aver_rx_snr)*3 + snr>>2;

    nbfi_transport_packet_t __xdata* pkt = 0;

    if(nbfi.mode == TRANSPARENT)
    {
        SLIP_Send(SLIP_CMD_RECEIVED_DL, (uint8_t __generic *)st->u.rx.pktdata, st->u.rx.pktlen - 4);
        goto end;
    }

    nbfi_pfy_packet_t __xdata *phy_pkt = (nbfi_pfy_packet_t __xdata *)st->u.rx.pktdata;

    ppp = (uint8_t __xdata*)phy_pkt;

    wtimer0_remove(&wait_for_extra_desc);

    if(nbfi_active_pkt->state == PACKET_WAIT_FOR_EXTRA_PACKETS)
    {
        nbfi_active_pkt->state = nbfi_active_pkt_old_state;
    }

    if(phy_pkt->SYS)
    {
            /* System messages */
            switch(phy_pkt->payload[0]) // Message type
            {
            case 0x00:  //ACK received
                uint32_t mask = 0;
                uint8_t i = 1;
                if(((nbfi_active_pkt->state == PACKET_WAIT_ACK) /*|| (nbfi_active_pkt->state == PACKET_WAIT_FOR_EXTRA_PACKETS)*/) && (phy_pkt->ITER == nbfi_active_pkt->phy_data.ITER))
                {
                    wtimer0_remove(&dl_receive_desc);
                    nbfi_active_pkt->state =  PACKET_DELIVERED;
#ifdef NBFi_DEBUG
                    my_sprintf((char __generic *)string, "receive ack%d, snr=%d", phy_pkt->ITER, phy_pkt->payload[5]);
                    SLIP_Send_debug((uint8_t __generic *)string, 50);
#endif

                    state.aver_tx_snr = ((uint16_t)state.aver_tx_snr)*3 + phy_pkt->payload[5]>>2;
                    do
                    {
                        mask = (mask << 8) + phy_pkt->payload[i];
                    }   while (++i < 5);

                    NBFi_Resend_Pkt(nbfi_active_pkt, mask);

                }
                break;

            case 03:    //ACK on system packet received
                if((nbfi_active_pkt->state == PACKET_WAIT_ACK))
                {
                    wtimer0_remove(&dl_receive_desc);
                    nbfi_active_pkt->state =  PACKET_DELIVERED;
                    state.success_total++;
                }
                break;
            case 0x04:  //clear RX buffer message received
                NBFi_Clear_RX_Buffer();
                #ifdef NBFi_DEBUG
                my_sprintf((char __generic *)string, "receive clear%d", phy_pkt->ITER);
                SLIP_Send_debug((uint8_t __generic *)string, 50);
                #endif
                break;
            case 0x05:  //start packet of the groupe
                goto place_to_stack;
            case 0x06:  //nbfi configure

                if(NBFi_Config_Parser(&phy_pkt->payload[1]))
                {
                    nbfi_transport_packet_t __xdata* ack_pkt =  NBFi_AllocateTxPkt(8);
                    if(!ack_pkt) break;
                    ack_pkt->phy_data.payload[0] = 0x06;
                    memcpy_xdata(&ack_pkt->phy_data.payload[1], &phy_pkt->payload[1], 7);
                    ack_pkt->phy_data.ITER = phy_pkt->ITER;
                    ack_pkt->phy_data.header |= SYS_FLAG;
                    if(nbfi_settings_changed)
                    {
                            ack_pkt->handshake = HANDSHAKE_SIMPLE;
                            ack_pkt->phy_data.header |= ACK_FLAG;
                    }else   ack_pkt->handshake = HANDSHAKE_NONE;
                    nbfi_settings_changed = 0;
                    ack_pkt->state = PACKET_NEED_TO_SEND_RIGHT_NOW;
                    //nbfi_active_pkt = ack_pkt;
                }
                break;

            }
            if(phy_pkt->ACK)    //send ACK on system packet
            {
                    nbfi_transport_packet_t __xdata* ack_pkt =  NBFi_AllocateTxPkt(8);
                    if(pkt)
                    {
                        ack_pkt->phy_data.payload[0] = 0x03; //ACK on SYS
                        ack_pkt->phy_data.payload[1] = phy_pkt->payload[0];//type of sys packet
                        ack_pkt->phy_data.payload[2] = 0;
                        ack_pkt->phy_data.payload[3] = 0;
                        ack_pkt->phy_data.payload[4] = 0;
                        ack_pkt->phy_data.payload[5] = snr;
                        ack_pkt->phy_data.payload[6] = (uint8_t)(noise + 150);
                        ack_pkt->phy_data.payload[7] = you_should_dl_power_step_down + you_should_dl_power_step_up + (nbfi.tx_pwr & 0x3f); //you_should_dl_power_step_down = (1 << 7);
                        ack_pkt->phy_data.ITER = phy_pkt->ITER;
                        ack_pkt->phy_data.header |= SYS_FLAG;
                        ack_pkt->handshake = HANDSHAKE_NONE;
                        ack_pkt->state = PACKET_NEED_TO_SEND_RIGHT_NOW;
                        //nbfi_active_pkt = ack_pkt;
                    }
            }

    }
    else
    {
        //Get application packet
place_to_stack:
        if(!NBFi_Check_RX_Packet_Duplicate(phy_pkt, st->u.rx.pktlen - 4))   //if there is no rx duplicate
        {
            pkt = NBFi_AllocateRxPkt(phy_pkt->header, st->u.rx.pktlen - 5);
            if(!pkt) return;
            memcpy_xdata(&pkt->phy_data.header, st->u.rx.pktdata, st->u.rx.pktlen - 4);
            pkt->state = PACKET_RECEIVED;
            #ifdef NBFi_DEBUG
            my_sprintf((char __generic *)string, "receive data%d", pkt->phy_data.ITER);
            SLIP_Send_debug((uint8_t __generic *)string, 50);
            #endif
        }

        if(phy_pkt->ACK)
        {
            // Send ACK
            nbfi_transport_packet_t __xdata* ack_pkt =  NBFi_AllocateTxPkt(8);
            if(ack_pkt)
            {
                uint32_t mask = NBFi_Get_RX_ACK_Mask();
                ack_pkt->phy_data.payload[0] = 0x00;
                ack_pkt->phy_data.payload[1] = (mask >> 24)&0xff;
                ack_pkt->phy_data.payload[2] = (mask >> 16)&0xff;
                ack_pkt->phy_data.payload[3] = (mask >> 8)&0xff;
                ack_pkt->phy_data.payload[4] = (mask >> 0)&0xff;
                ack_pkt->phy_data.payload[5] = snr;
                ack_pkt->phy_data.payload[6] = (uint8_t)(noise + 150);
                ack_pkt->phy_data.payload[7] = you_should_dl_power_step_down + you_should_dl_power_step_up + (nbfi.tx_pwr & 0x3f); //you_should_dl_power_step_down = (1 << 7);
                ack_pkt->phy_data.ITER = state.DL_iter&0x1f;
                ack_pkt->phy_data.header |= SYS_FLAG;
                if(nbfi.handshake_mode == HANDSHAKE_TRIPPLE)
                {
                    ack_pkt->handshake = HANDSHAKE_SIMPLE;
                    ack_pkt->phy_data.header |= ACK_FLAG;
                }
                else ack_pkt->handshake = HANDSHAKE_NONE;
                ack_pkt->state = PACKET_NEED_TO_SEND_RIGHT_NOW;
                //nbfi_active_pkt = ack_pkt;
                #ifdef NBFi_DEBUG
                my_sprintf((char __generic *)string, "send ack%d:%04X%04X", ack_pkt->phy_data.ITER, (uint16_t)(mask>>16), (uint16_t)mask&0xffff);
                SLIP_Send_debug((uint8_t __generic *)string, 50);
                #endif
                NBFi_Force_process();
            }
        }
    }

    NBFi_ProcessRxPackets();

    if(/*phy_pkt->SYS && */phy_pkt->MULTI && !phy_pkt->ACK)
    {
        //wait for extra packets
        nbfi_active_pkt_old_state = nbfi_active_pkt->state;
        nbfi_active_pkt->state = PACKET_WAIT_FOR_EXTRA_PACKETS;
        ScheduleTask(&wait_for_extra_desc, NBFi_Wait_Extra_Handler, RELATIVE, NBFI_DL_LISTEN_TIME[nbfi.rx_phy_channel]);
    }
    else
    {
        if(nbfi_active_pkt->state == PACKET_WAIT_FOR_EXTRA_PACKETS) nbfi_active_pkt->state = nbfi_active_pkt_old_state;

    }

    if(!phy_pkt->ACK) NBFI_Config_Check_State();
    if(NBFi_GetQueuedTXPkt()) NBFi_Force_process();
    else
    {
end:   NBFi_RX_Controller();
    }

}


static void NBFi_ProcessTasks(struct wtimer_desc __xdata *desc)
{

    if(rf_busy == false)
    {
        switch(nbfi_active_pkt->state)
        {
        case PACKET_DL_DELAY:
        case PACKET_WAIT_ACK:
        case PACKET_WAIT_FOR_EXTRA_PACKETS:
            break;
       /* case PACKET_NEED_TO_SEND_RIGHT_NOW:
            nbfi_active_pkt->state = PACKET_SENT;
            if(nbfi_active_pkt->phy_data.SYS && NBFi_GetQueuedTXPkt()) nbfi_active_pkt->phy_data.header |= MULTI_FLAG;
            NBFi_TX(nbfi_active_pkt);
            if(!nbfi_active_pkt->phy_data.ACK && NBFi_GetQueuedTXPkt())
            {
                    NBFi_Force_process();
            }
            else NBFi_RX_Controller();
            break;*/
        default:

            nbfi_transport_packet_t __xdata* pkt = NBFi_GetQueuedTXPkt();
            if(pkt)
            {
                if((pkt->handshake != HANDSHAKE_NONE) && (pkt->phy_data.ACK) && (nbfi.mode != TRANSPARENT))
                {
                    switch(nbfi.mode)
                    {
                    case DRX:
                        pkt->state = PACKET_DL_DELAY;
                        ScheduleTask(&dl_receive_desc, NBFi_RX_DL_StartHandler, RELATIVE, NBFI_DL_DELAY[nbfi.tx_phy_channel - 20]);
                        break;
                    case CRX:
                        pkt->state = PACKET_WAIT_ACK;
                        ScheduleTask(&dl_receive_desc, NBFi_Receive_Timeout_cb, RELATIVE, NBFI_DL_DELAY[nbfi.tx_phy_channel - 20] + NBFI_DL_LISTEN_TIME[nbfi.rx_phy_channel] + random()%(NBFI_DL_ADD_RND_LISTEN_TIME[nbfi.rx_phy_channel]));
                        break;
                    }

                }
                else pkt->state = PACKET_SENT;
                nbfi_active_pkt = pkt;
                if(pkt->phy_data.SYS && NBFi_GetQueuedTXPkt()) pkt->phy_data.header |= MULTI_FLAG;
                #ifdef NBFi_DEBUG
                my_sprintf((char __generic *)string, "send data%d", pkt->phy_data.ITER);
                SLIP_Send_debug((uint8_t __generic *)string, 50);
                #endif
                NBFi_TX(pkt);
                if(!pkt->phy_data.ACK && NBFi_GetQueuedTXPkt())
                {
                    NBFi_Force_process();
                }
                else
                {
                    has_smth_to_transmit = 0;
                    NBFI_Config_Check_State();
                    NBFi_RX_Controller();
                }

            }
            else
            {
                //if(aver_rx_rssi > 30)  ;//NBFi_Config_Rate_Change(UP); ////data rate step up
                //else
                //{
                    has_smth_to_transmit = 0;
                    NBFi_RX_Controller();
                    //ScheduleTask(desc, 0, RELATIVE, SECONDS(1));
                    //return;
                //}
            }
        }
    }

    //if(NBFi_GetQueuedTXPkt())
    //{
    //    ScheduleTask(desc, 0, RELATIVE, MILLISECONDS(10));
   // }
   // else
    {
        if(rf_state == STATE_RX)
        {
            if(noise_cntr >= 10)
            {
                int16_t n = noise_summ/noise_cntr;
                noise_summ = 0;
                noise_cntr = 0;
                if(n < noise_min) noise_min = n;
                if(--noise_min_cntr == 0)
                {
                    if(noise_min == -150) noise = n;
                    else noise = noise_min;
                    noise_min = 0;
                    noise_min_cntr = 5;
                #ifdef NBFi_DEBUG
                    my_sprintf((char __generic *)string, "aver noise=%d", noise);
                    SLIP_Send_debug((uint8_t __generic *)string, 50);
                #endif
                }

            }
            else
            {
                int8_t r = AX5043_RSSI;
                noise_summ += r - (int16_t)axradio_phy_rssioffset;
                noise_cntr++;
                //#ifdef NBFi_DEBUG
                //    my_sprintf((char __generic *)string, "noise=%d", r - (int16_t)axradio_phy_rssioffset);
                //    SLIP_Send_debug((uint8_t __generic *)string, 50);
                //#endif
            }
        }
        ScheduleTask(desc, 0, RELATIVE, SECONDS(1));
    }

}


void NBFi_RX_DL_StartHandler(struct wtimer_desc __xdata *desc)
{
    nbfi_active_pkt->state = PACKET_DL_RX;
    ScheduleTask(desc, NBFi_Receive_Timeout_cb, RELATIVE, NBFI_DL_LISTEN_TIME[nbfi.rx_phy_channel]);
    NBFi_RX_Controller();
}

void NBFi_Receive_Timeout_cb(struct wtimer_desc __xdata *desc)
{
    if(rf_busy)
    {
        ScheduleTask(desc, NBFi_Receive_Timeout_cb, RELATIVE, NBFI_DL_LISTEN_TIME[nbfi.rx_phy_channel]);
        return;
    }
    wtimer0_remove(&dl_receive_desc);
    if(nbfi_active_pkt->state != PACKET_WAIT_ACK) return;
    state.fault_total++;
    NBFi_Config_Tx_Power_Change(UP);
    if(++nbfi_active_pkt->retry_num > nbfi.num_of_retries)
    {
        nbfi_active_pkt->state = PACKET_LOST;
        if(nbfi_active_pkt->phy_data.SYS && (nbfi_active_pkt->phy_data.payload[0] == 0x06)/* && (nbfi_active_pkt->phy_data.payload[1] != RATE_CHANGE_PARAM_CMD)*/)
        {
            NBFi_Config_Return(); //return to previous work configuration
        }
        else
        {
            NBFi_Config_Set_Default(); //set default configuration
            NBFi_Config_Send_Mode(false);
        }
    }
    else
    {
        nbfi_active_pkt->state = PACKET_QUEUED_AGAIN;
        NBFi_Force_process();
        return;
    }
    NBFi_RX_Controller();
}

void NBFi_Wait_Extra_Handler(struct wtimer_desc __xdata *desc)
{
    wtimer0_remove(&wait_for_extra_desc);
    if(nbfi_active_pkt->state == PACKET_WAIT_FOR_EXTRA_PACKETS)     {nbfi_active_pkt->state = nbfi_active_pkt_old_state;}
    if(NBFi_GetQueuedTXPkt()) NBFi_Force_process();
}

void NBFi_Force_process()
{
    ScheduleTask(&nbfi_processTask_desc, NBFi_ProcessTasks, RELATIVE, MILLISECONDS(5));
}

void NBFi_Wait_For_Transmit_End()
{
    if(!NBFi_GetQueuedTXPkt()) return;
    has_smth_to_transmit = 1;
    while(1)
    {
            if(!has_smth_to_transmit) break;
            wtimer_runcallbacks();
    }
}

nbfi_status_t NBFI_Init(rx_handler_t h)
{

    NBFi_Config_Set_Default();
    for(uint8_t i = 0; i < NBFI_TX_PKTBUF_SIZE; i++) nbfi_TX_pktBuf[i] = 0;
    for(uint8_t i = 0; i < NBFI_RX_PKTBUF_SIZE; i++) nbfi_RX_pktBuf[i] = 0;

    rx_handler = h;

    RF_Init(nbfi.tx_phy_channel, nbfi.tx_antenna, nbfi.tx_pwr, 446000000);
    NBFi_RX_Controller();

    RF_SetDstAddress((uint8_t __generic *)&fastdladdress);

    XTEA_Enable(false);
    NBFi_Config_Send_Mode(false);
    NBFi_Send_Clear_Cmd(0);

    NBFi_Force_process();

    return OK;
}


