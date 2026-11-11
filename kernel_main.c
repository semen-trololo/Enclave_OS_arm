#include "uart.h"

void kernel_main(unsigned int r0, unsigned int machine, void *dtb)
{
    (void)r0;
    (void)machine;
    (void)dtb;

    uart_init();
    uart_puts("Enclave OS boot\n");

    while (1) {
        ;
    }
}
