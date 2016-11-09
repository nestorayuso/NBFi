#include "ax8052.h"
#include "wtimer.h"

__reentrantb void wtimer_add_callback(struct wtimer_callback __xdata *desc) __reentrant
{
        uint8_t __autodata iesave = IE_1;
        IE_1 = 0;
        wtimer_addcb_core(desc);
        IE_1 = iesave;
}
