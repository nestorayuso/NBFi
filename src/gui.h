#ifndef GUI_H
#define GUI_H

#include <ax8052f143.h>
#include <libmftypes.h>

#define TABLE_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define GUI_NUM_LINES 4

typedef struct
{
    const char __generic* label;
    void (*handler)(void);
}gui_entry_t;

typedef struct
{
    const char __generic*   label;
    void __generic*         value;
    const char __generic*   format;
    uint32_t                upperBound;
    int32_t                 lowerBound;
}gui_settings_t;

void GUI_Init();

#endif
