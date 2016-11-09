#include "ax8052.h"
#include "wtimer.h"

__reentrantb void wtimer0_addabsolute(struct wtimer_desc __xdata *desc) __reentrant
{
        uint8_t __autodata iesave = IE_1;
        IE_1 = 0;
        wtimer0_update();
        wtimer0_addcore(desc);
        wtimer0_schedq();
        IE_1 = iesave;
}
