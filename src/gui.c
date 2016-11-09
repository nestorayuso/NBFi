#include <ax8052f143.h>
#include <libmftypes.h>
#include <libmfradio.h>
#include <libmfflash.h>
#include <libmfwtimer.h>
#include <libmfcrc.h>
#include "misc.h"

#include "nbfi.h"
#include "nbfi_config.h"
#include "application.h"
#include "slip.h"
#include "hal.h"
#include "time.h"
#include "waviotdvk.h"
#include "gui.h"
#include "glcd.h"
#include "fonts.h"
#include "nbfi_config.h"

struct wtimer_desc __xdata gui_handler;
void (*current_handler)(void);

char __xdata textbuf[30]; // for formatted strings

void GUI_DrawButtonR(const char __generic* label, uint8_t state);
void GUI_DrawButtonL(const char __generic* label, uint8_t state);

void MainHandler();
    void TestsHandler();
        void NBFiCHandler();
        void NBFiHoppingHandler();
        void RSSiHandler();
        void CarrierHandler();
    void InfoHandler();
    void SettingsHandler();

// Text labels
const char __code label_next[] =    "Next";
const char __code label_enter[] =   "Enter";
const char __code label_start[] =   "Start";
const char __code label_stop[] =    "Stop";
const char __code label_cancel[] =  "Cancel";
const char __code label_edit[] =    "Edit";
const char __code label_dec[] =     "<";
const char __code label_inc[] =     ">";
const char __code label_reflash[] = "Reflash";

const gui_entry_t __code main_table[]=
{
    {"Tests",               &TestsHandler},
    {"Settings",            &SettingsHandler},
    {"Info",                &InfoHandler},
};

/*void MainHandler()
{
    static uint8_t __xdata state = 0;

    // Caption
    LCD_DrawString(0,-6,"../", COLOR_FILL, ALIGN_LEFT);

    // Entry list
    for(int i=0; i<TABLE_SIZE(main_table); i++)
    {
        LCD_DrawString(20,(i*9)+5,main_table[i].label, COLOR_FILL, ALIGN_LEFT);
    }

    // Cursor
    LCD_DrawString(10,(state*9)+5,">", COLOR_FILL, ALIGN_LEFT);

    // Button processing
    if(GetButton1())
    {
        GUI_DrawButtonL(label_next, 1);
        state++;
        state = state % TABLE_SIZE(main_table);
    }
    else
    {
        GUI_DrawButtonL(label_next, 0);
    }

    if(GetButton2())
    {
        GUI_DrawButtonR(label_enter, 1);
        current_handler = main_table[state].handler;
    }
    else
    {
        GUI_DrawButtonR(label_enter, 0);
    }
}*/


void MainHandler()
{
    my_sprintf(textbuf, "RX AVER SNR = %i", state.aver_rx_snr);
    LCD_DrawString(0,-6,textbuf, COLOR_FILL, ALIGN_LEFT);
    my_sprintf(textbuf, "TX AVER SNR = %i", state.aver_tx_snr);
    LCD_DrawString(0,3,textbuf, COLOR_FILL, ALIGN_LEFT);
    my_sprintf(textbuf,  "succ=%d, fault=%d", state.success_total, state.fault_total);
    LCD_DrawString(0,12,textbuf, COLOR_FILL, ALIGN_LEFT);
    my_sprintf(textbuf, "Baud rate:rx=%d, tx=%d", current_rx_rate, current_tx_rate);
    LCD_DrawString(0,21,textbuf, COLOR_FILL, ALIGN_LEFT);

    if(GetButton3())
    {
        uint8_t single[] = "Button3!";
        NBFi_Send(single, 8);
    }
}

const gui_entry_t __code packet_tests_table[]=
{
    //{"NBFi-D",          &TestsHandler},
    {"NBFi-C",          &NBFiCHandler},
    {"Carrier TX",      &CarrierHandler},
    //{"Hopping",         &NBFiHoppingHandler},
    {"RSSi",            &RSSiHandler},
};

void TestsHandler()
{
    static uint8_t __xdata state = 0;

    // Caption
    LCD_DrawString(0,-6,"../Tests", COLOR_FILL, ALIGN_LEFT);

    // Entry list
    for(int i=0; i<TABLE_SIZE(packet_tests_table); i++)
    {
        LCD_DrawString(20,(i*9)+5,packet_tests_table[i].label, COLOR_FILL, ALIGN_LEFT);
    }

    // Cursor
    LCD_DrawString(10,(state*9)+5,">", COLOR_FILL, ALIGN_LEFT);

    // Button processing
    if(GetButton1())
    {
        GUI_DrawButtonL(label_next, 1);
        state++;
        state = state % TABLE_SIZE(packet_tests_table);
    }
    else
    {
        GUI_DrawButtonL(label_next, 0);
    }

    if(GetButton2())
    {
        GUI_DrawButtonR(label_enter, 1);
        current_handler = packet_tests_table[state].handler;
    }
    else
    {
        GUI_DrawButtonR(label_enter, 0);
    }

    if(GetButton3())
    {
        state = 0;
        current_handler = &MainHandler;
    }
}

const char __code* gui_packet_state[]=
{
    "FREE",
    "ALLOCATED",
    "QUEUED",
    "TRANSMIT",
    "PROCESSING",
    "DL DELAY",
    "RECEIVE",
    "WAIT ACK",
    "GOT DL",
    "TIMEOUT"
};

//extern received_packet_t __xdata* rx_pkt;
extern uint8_t __xdata rx_pkt_len;
extern int16_t __xdata rssi;
extern int16_t __xdata offset;

extern struct axradio_callback_receive __xdata axradio_cb_receive;

void NBFiCHandler()
{
   /* static uint8_t __xdata state = 0;
    static nbfi_packet_t __xdata* pkt;
    static uint8_t __xdata dl_result = 0;
    static uint8_t test_pkt[8] = {0,0xDE,0xAD,0xBE,0xEF,0x12,0x34,0x56};

    // Caption
    LCD_DrawString(0,-6,"../Tests/NBFi-C", COLOR_FILL, ALIGN_LEFT);

    switch(state)
    {
    case 0:
        LCD_DrawString(10,5,"READY", COLOR_FILL, ALIGN_LEFT);
        break;
    case 1:
        dl_result = 0;
        pkt = NBFi_AllocatePkt();
        pkt->handshake = PROTOCOL_C;
        pkt->packet_state = PACKET_QUEUED;
        memcpy_xdata(pkt->payload, test_pkt, 8);
        state = 2;
        break;
    case 2:
        my_sprintf(textbuf, "PACKET %s", gui_packet_state[pkt->packet_state]);
        LCD_DrawString(10,5,textbuf, COLOR_FILL, ALIGN_LEFT);

        if((pkt->packet_state == PACKET_GOT_DL) || (pkt->packet_state == PACKET_DL_TIMEOUT))
        {
            dl_result = pkt->packet_state;
            pkt->packet_state = PACKET_FREE;
            test_pkt[0]++;
            state = 3;
        }
        break;
    case 3:
        my_sprintf(textbuf, "PACKET %s", gui_packet_state[dl_result]);
        LCD_DrawString(10,5,textbuf, COLOR_FILL, ALIGN_LEFT);
    }

    // Print payload or DL responce
    if((state == 3) && (dl_result == PACKET_GOT_DL))
    {
        my_sprintf(textbuf, "rssi:%d ofs %d", rssi, offset);
        LCD_DrawString(10,14,textbuf, COLOR_FILL, ALIGN_LEFT);

        LCD_DrawString(10,23,"Response:", COLOR_FILL, ALIGN_LEFT);
        for(int i=0; i<rx_pkt_len; i++)//-1 for 1st CMD byte
        {
            my_sprintf(textbuf+(2*i), "%02x", ((uint8_t*)rx_pkt)[i]); // +1 for 1st CMD byte
        }
    }
    else
    {
        LCD_DrawString(10,23,"Payload:", COLOR_FILL, ALIGN_LEFT);
        for(int i=0; i<8; i++)
        {
            my_sprintf(textbuf+(2*i), "%02x", test_pkt[i]);
        }
    }
    LCD_DrawString(63,34,textbuf, COLOR_FILL, ALIGN_CENTERED);

    // Button processing
    if(GetButton1())
    {
        GUI_DrawButtonL(label_start, 1);
        if(((state == 0)||(state == 3))&&(rf_busy == false)) state = 1;
    }
    else
    {
        GUI_DrawButtonL(label_start, 0);
    }

    if(GetButton2())
    {
        GUI_DrawButtonR(label_cancel, 1);
    }
    else
    {
        GUI_DrawButtonR(label_cancel, 0);
    }

    if(GetButton3())
    {
        state = 0;
        current_handler = &TestsHandler;
    }
    */
}

void NBFiHoppingHandler()
{
    static uint8_t __xdata state = 0;

    // Caption
    LCD_DrawString(0,-6,"../Tests/Hopping", COLOR_FILL, ALIGN_LEFT);

    switch(state)
    {
    case 0:
        LCD_DrawString(10,5,"Ready", COLOR_FILL, ALIGN_LEFT);
        break;
    case 1:
        LCD_DrawString(10,5,"Sending", COLOR_FILL, ALIGN_LEFT);

        NBFi_TX_Hopping();
        break;
    }

    // Button processing
    if(GetButton1())
    {
        GUI_DrawButtonL(label_start, 1);
        state = 1;
    }
    else
    {
        GUI_DrawButtonL(label_start, 0);
    }

    if(GetButton2())
    {
        GUI_DrawButtonR(label_cancel, 1);
        state = 0;
    }
    else
    {
        GUI_DrawButtonR(label_cancel, 0);
    }

    if(GetButton3())
    {
        current_handler = &TestsHandler;
    }
}

void RSSiHandler()
{
    static uint8_t __xdata state = 0;
    static int16_t __xdata current_rssi = 0;

    switch(state)
    {
    case 0:
        LCD_DrawString(10,5,"RX disabled", COLOR_FILL, ALIGN_LEFT);
        break;
    case 1:
        LCD_DrawString(10,5,"RX enabled", COLOR_FILL, ALIGN_LEFT);
        current_rssi = (int8_t)AX5043_RSSI - (int16_t)64;
        my_sprintf(textbuf, "RSSi = %i dBm", current_rssi);
        LCD_DrawString(10,23,textbuf, COLOR_FILL, ALIGN_LEFT);
        break;
    }

    // Caption
    LCD_DrawString(0,-6,"../Tests/RSSi", COLOR_FILL, ALIGN_LEFT);

    // Button processing
    if(GetButton1())
    {
        GUI_DrawButtonL(label_start, 1);
        RF_Init(DL_PSK_200, PCB, 0, 446000000);
        state = 1;
    }
    else
    {
        GUI_DrawButtonL(label_start, 0);
    }

    if(GetButton2())
    {
        GUI_DrawButtonR(label_cancel, 1);
        RF_Deinit();
        state = 0;
    }
    else
    {
        GUI_DrawButtonR(label_cancel, 0);
    }

    if(GetButton3())
    {
        current_handler = &TestsHandler;
    }
}

void CarrierHandler()
{
    static uint8_t __xdata state = 0;
    static uint32_t __xdata freq = 868800000;
    // Caption
    LCD_DrawString(0,-6,"../Tests/Carrier TX", COLOR_FILL, ALIGN_LEFT);

    switch(state)
    {
    case 0:
        freq = 868800000;
        RF_Init(UL_DBPSK_50, nbfi.tx_antenna, nbfi.tx_pwr, freq);
        axradio_set_mode(AXRADIO_MODE_CW_TRANSMIT);
        state = 1;
        break;
    case 1:
        break;
    }

    my_sprintf(textbuf, "Freq: %lu Hz", freq);
    LCD_DrawString(10,5,textbuf, COLOR_FILL, ALIGN_LEFT);

    // Button processing
    if(GetButton1())
    {
        GUI_DrawButtonL(label_dec, 1);
        freq -= 1;

        RF_SetFreq(freq);
        axradio_set_channel(0);
    }
    else
    {
        GUI_DrawButtonL(label_dec, 0);
    }

    if(GetButton2())
    {
        GUI_DrawButtonR(label_inc, 1);
        freq += 1;

        RF_SetFreq(freq);
        axradio_set_channel(0);
    }
    else
    {
        GUI_DrawButtonR(label_inc, 0);
    }

    if(GetButton3())
    {
        state = 0;
        RF_Deinit();
        current_handler = &MainHandler;
    }
}

const char __code fmt_uint8_t[] = "%u";
const char __code fmt_int8_t[] = "%d";
const char __code fmt_uint32_t[] = "%d";
const char __code fmt_str[] = "%s";

void SettingsHandler()
{
    static uint8_t __xdata state = 0;
    static uint8_t __xdata edit = 0;

    // Caption
    LCD_DrawString(0,-6,"../Settings", COLOR_FILL, ALIGN_LEFT);

    // Entry list
    for(int i=0; i<4 && i<GUI_NUM_LINES; i++)
    {
        switch(i)
        {
        case 0:
            LCD_DrawString(10,(i*9)+5, "TX Power", COLOR_FILL, ALIGN_LEFT);
            my_sprintf(textbuf, "%d", nbfi.tx_pwr);
            LCD_DrawString(127,(i*9)+5, textbuf, COLOR_FILL, ALIGN_RIGHT);
            break;
        case 1:
            LCD_DrawString(10,(i*9)+5, "TX Antenna", COLOR_FILL, ALIGN_LEFT);
            my_sprintf(textbuf, "%s", nbfi.tx_antenna?"SMA":"PCB");
            LCD_DrawString(127,(i*9)+5, textbuf, COLOR_FILL, ALIGN_RIGHT);
            break;
        case 2:
            LCD_DrawString(10,(i*9)+5, "RX Antenna", COLOR_FILL, ALIGN_LEFT);
            my_sprintf(textbuf, "%s", nbfi.rx_antenna?"SMA":"PCB");
            LCD_DrawString(127,(i*9)+5, textbuf, COLOR_FILL, ALIGN_RIGHT);
            break;
        case 3:
            LCD_DrawString(10,(i*9)+5, "RX Mode", COLOR_FILL, ALIGN_LEFT);
            my_sprintf(textbuf, "%s", nbfi.mode?"CRX":"DRX");
            LCD_DrawString(127,(i*9)+5, textbuf, COLOR_FILL, ALIGN_RIGHT);
            break;
        }
    }

    if(edit)
    {
        LCD_FillRect(0,(state*9)+10,128,10, COLOR_INVERT);

        if(GetButton1())
        {
            GUI_DrawButtonL(label_dec, 1);
            switch(state)
            {
            case 0:
                if(nbfi.tx_pwr > 0) nbfi.tx_pwr--;
                break;
            case 1:
                nbfi.tx_antenna ^= 1;
                break;
            case 2:
                nbfi.rx_antenna ^= 1;
                break;
            case 3:
                nbfi.mode ^= 1;
                break;
            }
        }
        else
        {
            GUI_DrawButtonL(label_dec, 0);
        }

        if(GetButton2())
        {
            GUI_DrawButtonR(label_inc, 1);
            switch(state)
            {
            case 0:
                if(nbfi.tx_pwr < 26) nbfi.tx_pwr++;
                break;
            case 1:
                nbfi.tx_antenna ^= 1;
                break;
            case 2:
                nbfi.rx_antenna ^= 1;
                break;
            case 3:
                nbfi.mode ^= 1;
                break;
            }
        }
        else
        {
            GUI_DrawButtonR(label_inc, 0);
        }

        if(GetButton3())
        {
            edit = 0;
            return;
        }
    }



    // Button processing
    if(edit == 0)
    {
        // Cursor
        LCD_DrawString(5,(state*9)+5,">", COLOR_FILL, ALIGN_LEFT);

        if(GetButton1())
        {
            GUI_DrawButtonL(label_next, 1);
            state++;
            state = state % 4;
        }
        else
        {
            GUI_DrawButtonL(label_next, 0);
        }

        if(GetButton2())
        {
            GUI_DrawButtonR(label_edit, 1);
            edit = 1;
        }
        else
        {
            GUI_DrawButtonR(label_edit, 0);
        }

        if(GetButton3())
        {
            state = 0;
            current_handler = &MainHandler;
        }
    }
}

extern uint32_t __xdata buttons_v;
extern uint32_t __xdata VDD;

void InfoHandler()
{
    LCD_DrawString(0,-6,"../Device info", COLOR_FILL, ALIGN_LEFT);

    // Device full ID
    LCD_DrawString(10,5,"ID:", COLOR_FILL, ALIGN_LEFT);
//    my_sprintf(textbuf, "ID: %02X%02X%02X%02X%02X%02X", nbfi.full_ID[5],nbfi.full_ID[4],nbfi.full_ID[3],nbfi.full_ID[2],nbfi.full_ID[1],nbfi.full_ID[0]);
    my_sprintf(textbuf, "%02X%02X%02X", nbfi.full_ID[2],nbfi.full_ID[1],nbfi.full_ID[0]);
    LCD_DrawString(127,5,textbuf, COLOR_FILL, ALIGN_RIGHT);

    time_t t = RTC_Time();
    struct tm __generic* TM = localtime(&t);

    LCD_DrawString(10,14,"Time:", COLOR_FILL, ALIGN_LEFT);
    my_sprintf(textbuf, "%02d:%02d:%02d", TM->tm_hour, TM->tm_min, TM->tm_sec);
    LCD_DrawString(127,14,textbuf, COLOR_FILL, ALIGN_RIGHT);

//    LCD_DrawString(10,23,"Buttons:", COLOR_FILL, ALIGN_LEFT);
//    my_sprintf(textbuf, "%u", buttons_v);
//    LCD_DrawString(127,23,textbuf, COLOR_FILL, ALIGN_RIGHT);
//
//    LCD_DrawString(10,32,"VCC:", COLOR_FILL, ALIGN_LEFT);
//    my_sprintf(textbuf, "%u", ((VDD*10000)>>16) - 4500);
//    LCD_DrawString(127,32,textbuf, COLOR_FILL, ALIGN_RIGHT);
//
//    LCD_DrawString(10,32,"T:", COLOR_FILL, ALIGN_LEFT);
//    my_sprintf(textbuf, "%d'C", GetVoltageOrTemp(0));
//    LCD_DrawString(127,32,textbuf, COLOR_FILL, ALIGN_RIGHT);

    if(GetButton2())
    {
        GUI_DrawButtonR(label_reflash, 1);
        HAL_RebootToBootloader();
    }
    else
    {
        GUI_DrawButtonR(label_reflash, 0);
    }

    if(GetButton3())
    {
        current_handler = &MainHandler;
    }
}

//-------------------------------- SERVICE ROUTINES -------------------------------------

static void GUI_Handler(struct wtimer_desc __xdata *desc)
{
//    gui_page_t __xdata* page = (gui_page_t __xdata*) desc;

    LCD_ClearBuffer(0);
    LCD_SetFont(&arial_8ptFontInfo);

    (*current_handler)();

    LCD_WriteBuffer();

    ScheduleTask(desc, 0, RELATIVE, MILLISECONDS(50));
}

void GUI_Init()
{
    ScheduleTask(&gui_handler, GUI_Handler, RELATIVE, 0);
    current_handler = &MainHandler;
}

void GUI_DrawButtonL(const char __generic* label, uint8_t state)
{
    if(state)
    {
        LCD_FillRect(0, 52, 51, 12, 1);
        LCD_DrawString(26,48,label, COLOR_INVERT, ALIGN_CENTERED);
    }
    else
    {
        LCD_DrawRect(0, 52, 51, 12, 1);
        LCD_DrawString(26,48,label, COLOR_FILL, ALIGN_CENTERED);
    }
}

void GUI_DrawButtonR(const char __generic* label, uint8_t state)
{
    if(state)
    {
        LCD_FillRect(76, 52, 51, 12, 1);
        LCD_DrawString(102,48,label, COLOR_INVERT, ALIGN_CENTERED);
    }
    else
    {
        LCD_DrawRect(76, 52, 51, 12, 1);
        LCD_DrawString(102,48,label, COLOR_FILL, ALIGN_CENTERED);
    }
}
