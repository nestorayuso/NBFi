#ifndef RF_H
#define RF_H

#include "nbfi.h"



typedef enum
{
    STATE_OFF,
    STATE_RX,
    STATE_TX
}nbfi_rf_state_s;




extern nbfi_rf_state_s rf_state;
#endif // NBFI_H
