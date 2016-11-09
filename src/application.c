#include <ax8052f143.h>
#include <libmftypes.h>
#include <libmfradio.h>
#include <libmfflash.h>
#include <libmfwtimer.h>
#include <libmfcrc.h>
#include "misc.h"
#include "defines.h"
#include "nbfi.h"
#include "application.h"
#include "slip.h"
#include "hal.h"
#include "waviotdvk.h"
#include "gui.h"
#include "glcd.h"
#include "fonts.h"
#include "time.h"

uint8_t __xdata stay_awake = 0;

uint8_t __xdata unb_pckt[]= {1,2,3,4,5,6,7,8};//test packet

struct wtimer_desc __xdata everysecond_desc;
void EverySecond(struct wtimer_desc __xdata *desc);



void ColdSetup()
{
    #ifndef SDR
    //LCD_Init();
    //ScheduleTask(&everysecond_desc, &EverySecond, RELATIVE, SECONDS(2));
    //Buttons_Init();
    //Backlight(1);
    //GUI_Init();
    #endif
    RTC_Init();
    SLIP_Init();
    NBFI_Init(&RX_Callback);
}

void WarmSetup()
{
    SLIP_test_unsleep();
    ax5043_commsleepexit();
    IE_4 = 1; // enable radio interrupt
    if (SLIP_isCanSleep()) {
        delay_ms(1);
    }
}

void Loop()
{
    //SLIP_Process();
}

//nbfi_packet_t __xdata test_pkt;
void EverySecond(struct wtimer_desc __xdata *desc)
{
static uint8_t counter = 0;
   /*uint8_t data[] =
"\nA death has occurred and everything is changed\
\nWe are painfully aware that life can never be the same again,\
\nThat yesterday is over,\
\nThat relationships once rich have ended.\
\nBut there is another way to look upon this truth.";*/
   //uint8_t single[] = "Hello!!!";
   //NBFi_Send(single, 8);
   //ScheduleTask(desc, 0, RELATIVE, SECONDS(3));
   //NBFi_Send(single, 8);
   //NBFi_Send(single, 8);
   uint8_t data[] = "Do you like this weather?";// I saw a politician with his hands in his own pockets.";
   NBFi_Send(data, sizeof(data));
   /*NBFi_Send(single, 8);
   NBFi_Send(single, 8);
   NBFi_Send(single, 8);*/
   if(counter < 3 )
   {
        //ScheduleTask(desc, 0, RELATIVE, SECONDS(3));
        counter++;
   }
   ScheduleTask(desc, 0, RELATIVE, SECONDS(90));
   /* if(nbfi.attach_state == NBFI_ATTACHED)
    {
        nbfi_packet_t __xdata* pkt = NBFi_AllocatePkt();
        if(pkt)
        {
            pkt->header = 0;
            memcpy_xdata(pkt->payload, unb_pckt, 8);
            pkt->handshake = HANDSHAKE_SIMPLE;
            pkt->timeout_callback = 0;
            pkt->packet_state = PACKET_QUEUED;

            ScheduleTask(desc, 0, RELATIVE, SECONDS(100));
        }
    }
    else
    {
        ScheduleTask(desc, 0, RELATIVE, SECONDS(1));
    }*/

}

void RX_Callback(uint8_t __generic* data, uint16_t length)
{
    //NBFi_Send(data, length);
}
