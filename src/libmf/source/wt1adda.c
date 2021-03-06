#include "ax8052.h"
#include "wtimer.h"

__reentrantb void wtimer1_addabsolute(struct wtimer_desc __xdata *desc) __reentrant
{
	uint8_t __autodata iesave = IE_1;
	IE_1 = 0;
	wtimer1_update();
	wtimer1_addcore(desc);
	wtimer1_schedq();
	IE_1 = iesave;
}
