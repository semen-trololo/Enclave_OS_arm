/* uart.c — минимальный драйвер PL011 для Pi1 B+ (без GPPUDCLK) */

#include "uart.h"

#define MMIO_BASE   0x20000000u

/* --- GPIO --- */
#define GPFSEL1     (*(volatile unsigned int*)(MMIO_BASE + 0x200004))

/* --- PL011 --- */
#define UART_DR     (*(volatile unsigned int*)(MMIO_BASE + 0x201000))
#define UART_FR     (*(volatile unsigned int*)(MMIO_BASE + 0x201018))
#define UART_IBRD   (*(volatile unsigned int*)(MMIO_BASE + 0x201024))
#define UART_FBRD   (*(volatile unsigned int*)(MMIO_BASE + 0x201028))
#define UART_LCRH   (*(volatile unsigned int*)(MMIO_BASE + 0x20102C))
#define UART_CR     (*(volatile unsigned int*)(MMIO_BASE + 0x201030))
#define UART_IMSC   (*(volatile unsigned int*)(MMIO_BASE + 0x201038))

void uart_init(void)
{
    unsigned int r;

    /* 1. GPIO14/15 -> Alt0 */
    r  = GPFSEL1;
    r &= ~((7u << 12) | (7u << 15));
    r |=  ((4u << 12) | (4u << 15));
    GPFSEL1 = r;

    /* 2. Убить всё, потом включить */
    UART_CR   = 0;

    /* 3. Делители — по дефолту 115200 при 3MHz */
    UART_IBRD = 1;
    UART_FBRD = 40;

    /* 4. Кадр — 8N1, FIFO выкл (проще) */
    UART_LCRH = 0x60;  /* 8 bit, no FIFO */

    /* 5. Прерывания — все замаскированы */
    UART_IMSC = 0;

    /* 6. Включить TX и UART */
    UART_CR   = (1u << 0) | (1u << 8);  /* UARTEN | TXE */
}

void uart_putc(char c)
{
    /* Большой таймаут ~100мс если UART не тактируется */
    unsigned int timeout = 0xFFFFFF;
    while ((UART_FR & (1u << 5)) && timeout)
        timeout--;
    if (timeout)
        UART_DR = c;
}

void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');      /* терминалы ждут CR+LF */
        uart_putc(*s++);
    }
}

static const char hex_digits[] = "0123456789abcdef";

void uart_puthex(unsigned int v)
{
    for (int shift = 28; shift >= 0; shift -= 4)
        uart_putc(hex_digits[(v >> shift) & 0xF]);
}