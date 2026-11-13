#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

/* VIC контроллер (BCM2835) */
#define VIC_BASE 0x2000B200u
#define VICINTSELECT (*(volatile uint32_t*)(VIC_BASE + 0x00))
#define VICINTENABLE (*(volatile uint32_t*)(VIC_BASE + 0x04))
#define VICINTENCLEAR (*(volatile uint32_t*)(VIC_BASE + 0x08))
#define VICVECTADDR0 (*(volatile uint32_t*)(VIC_BASE + 0x100))
#define VICADDRESS (*(volatile uint32_t*)(VIC_BASE + 0xF00))

void vic_init(void);
void interrupt_handler(void);

#endif