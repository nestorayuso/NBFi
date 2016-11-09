/*
 * ST7565 LCD library
 * Copyright (C) 2010 Limor Fried, Adafruit Industries
 */

#include <stdlib.h>
#include "stlcd.h"
#include "spi.h"
#include "fonts.h"
#include "misc.h"

#define LCD_CS      PORTC_0
#define LCD_RST     PORTB_5
#define LCD_A0      PORTA_1
#define LCD_POWER   PORTB_4

#define LCD_INIT_INTERFACE() {DIRA |= 0x02; DIRC |= 0x01; DIRB |= 0x30;}

const int __code pagemap[] = { 3, 2, 1, 0, 7, 6, 5, 4 };
uint8_t __xdata lcd_buffer[1024];

#define mmio(x)   (*(volatile unsigned char *)(x))

static void ST7565_InterfaceInit()
{
    // Set pin directions
    DIRA |= 0x02;
    DIRC |= 0x01;
    DIRB |= 0x30;

    LCD_CS = 1;

    SPI_Init();
}

static void ST7565_WriteCommand (uint8_t c)
{
    LCD_CS = 0;
    LCD_A0 = 0;
    SPI_Transfer(c);
    LCD_CS = 1;
}

static void ST7565_WriteData (uint8_t c)
{
    LCD_CS = 0;
    LCD_A0 = 1;
    SPI_Transfer(c);
    LCD_CS = 1;
}

void ST7565_Init (void)
{
    ST7565_InterfaceInit();

    LCD_POWER = 1;

    // toggle RST low to reset
    LCD_RST = 0;
    delay_ms(5);
    LCD_RST = 1;
    delay_ms(5);

    // LCD bias select
    ST7565_WriteCommand (CMD_SET_BIAS_7);
    // ADC select
    ST7565_WriteCommand (CMD_SET_ADC_NORMAL);
    // SHL select
    ST7565_WriteCommand (CMD_SET_COM_NORMAL);
    // Initial display line
    ST7565_WriteCommand (CMD_SET_DISP_START_LINE);
    // turn on voltage converter (VC=1, VR=0, VF=0)
    ST7565_WriteCommand (CMD_SET_POWER_CONTROL | 0x4);
    // wait for 50% rising
    delay_ms(10);
    // turn on voltage regulator (VC=1, VR=1, VF=0)
    ST7565_WriteCommand (CMD_SET_POWER_CONTROL | 0x6);
    // wait >=50ms
    delay_ms(50);
    // turn on voltage follower (VC=1, VR=1, VF=1)
    ST7565_WriteCommand (CMD_SET_POWER_CONTROL | 0x7);
    // wait
    delay_ms(10);
    // set lcd operating voltage (regulator resistor, ref voltage resistor)
    ST7565_WriteCommand (CMD_SET_RESISTOR_RATIO | 0x6);

    ST7565_WriteCommand (CMD_DISPLAY_ON);
	ST7565_WriteCommand (CMD_SET_ALLPTS_NORMAL);
    ST7565_SetBrightness (0x04);
}

void ST7565_SetBrightness (uint8_t val)
{
    ST7565_InterfaceInit();

    ST7565_WriteCommand (CMD_SET_VOLUME_FIRST);
    ST7565_WriteCommand (CMD_SET_VOLUME_SECOND | (val & 0x3f));
}

void ST7565_WriteBuffer (void)
{
    ST7565_InterfaceInit();

    for (uint16_t p = 0; p < 8; p++)
    {
        ST7565_WriteCommand (CMD_SET_PAGE | pagemap[p]);
        ST7565_WriteCommand (CMD_SET_COLUMN_LOWER | (0x0 & 0xf));
        ST7565_WriteCommand (CMD_SET_COLUMN_UPPER | ((0x0 >> 4) & 0xf));
        ST7565_WriteCommand (CMD_RMW);
        ST7565_WriteData (0x00);

        for (uint8_t c=0; c <LCDWIDTH; c++)
        {
            ST7565_WriteData (lcd_buffer[(LCDWIDTH*p)+c]);
        }
    }
}
