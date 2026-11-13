#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "interrupt.h"

extern void panic(const char* msg, void* addr);

/* Для отладки исключений - see boot.S */
volatile int exc_probe = 0;         /* 0 = no exception, 1 = in handler */
volatile int exc_caught = 0;        /* 0 = no exception, 1 = exception handled */
volatile void* exc_pc = 0;          /* PC when exception taken */

void kernel_main(unsigned int machine_type, unsigned int atags, void* dtb) {
    (void)machine_type;
    (void)atags;
    (void)dtb;

    /* --- Настройка GPIO14/15 в Alt0 для UART --- */
    volatile uint32_t* gpfsel1 = (volatile uint32_t*)0x20200004; // GPFSEL1
    uint32_t r = *gpfsel1;
    r &= ~(7<<12);  // GPIO14
    r |= 4<<12;     // Alt0
    r &= ~(7<<15);  // GPIO15
    r |= 4<<15;     // Alt0
    *gpfsel1 = r;

    /* --- UART --- */
    uart_init();
    uart_puts("\nHello from Enclave OS\n");

    /* --- LED --- */
    led_init_gpio(); // Пина 47
    
    /* Инициализация VIC */
    vic_init();
    
    /* Разрешаем прерывания */
    __asm__ volatile("cpsie i");
    
    while(1) {
        uart_putc('A');
        led_toggle();
    }
}