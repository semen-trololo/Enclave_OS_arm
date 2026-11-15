/* uart.h — интерфейс PL011-драйвера (Pi 1 B+, UART0 @ 0x20201000) */

#ifndef UART_H
#define UART_H

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_puthex(unsigned int v);      /* 8 hex-цифр, без префикса */
void uart_putdec(unsigned int v);      /* десятичное, без ведущих нулей */

#endif /* UART_H */
