#include "spi.h"

void SPI_Init()
{
    DIRC |= 0x06;
    DIRC |= 0x10; // ADE CS
    DIRB |= 0x04; // FLASH CS

    SPCLKSRC = 0xe0; // Invert clock & polarity, (FRCOSC / 16) = 1.25 MHz
    SPMODE = 0x01; // Master mode ON
}

inline uint8_t SPI_Transfer(uint8_t c)
{
    SPSHREG = c;
    while(!(SPSTATUS & 0x01));
    return SPSHREG;
}

