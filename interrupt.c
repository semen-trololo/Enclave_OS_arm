/* interrupt.c — VIC interrupt controller implementation */

#include "interrupt.h"

void vic_init(void) {
    /* Все линии — IRQ (не FIQ) */
    VICINTSELECT = 0;
    
    /* Разрешаем IRQ прерывания в VIC */
    VICINTENABLE = 0;
    
    /* Убираем векторный адрес */
    VICVECTADDR0 = 0;
}

void interrupt_handler(void) {
    uint32_t addr = VICADDRESS;
    VICADDRESS = 0; /* очистка VIC после обработки */
    (void)addr; /* пока неиспользуется */
}