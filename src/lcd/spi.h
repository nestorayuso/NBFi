#ifndef SPI_H
#define SPI_H

#include <ax8052f143.h>
#include <libmftypes.h>

void    SPI_Init();
uint8_t SPI_Transfer(uint8_t c);

#endif
