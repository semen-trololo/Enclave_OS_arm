/* uart.c — минимальный драйвер PL011 для Pi1 B+ */

#include "uart.h"

#define MMIO_BASE   0x20000000u

/* --- GPIO --- */
#define GPFSEL1     (*(volatile unsigned int*)(MMIO_BASE + 0x200004))
#define GPPUD       (*(volatile unsigned int*)(MMIO_BASE + 0x200094))
#define GPPUDCLK0   (*(volatile unsigned int*)(MMIO_BASE + 0x200098))

/* --- PL011 --- */
#define UART_DR     (*(volatile unsigned int*)(MMIO_BASE + 0x201000))
#define UART_FR     (*(volatile unsigned int*)(MMIO_BASE + 0x201018))
#define UART_IBRD   (*(volatile unsigned int*)(MMIO_BASE + 0x201024))
#define UART_FBRD   (*(volatile unsigned int*)(MMIO_BASE + 0x201028))
#define UART_LCRH   (*(volatile unsigned int*)(MMIO_BASE + 0x20102C))
#define UART_CR     (*(volatile unsigned int*)(MMIO_BASE + 0x201030))
#define UART_IMSC   (*(volatile unsigned int*)(MMIO_BASE + 0x201038))

static void delay(unsigned int n)
{
    while (n--) asm volatile("nop");
}

void uart_init(void)
{
    unsigned int r;

    /* 1. GPIO14/15 -> Alt0: ноги физически подключаются к PL011.
          GPFSEL1 управляет ногами 10..19, по 3 бита на ногу:
          GPIO14 = биты [14:12], GPIO15 = биты [17:15], alt0 = 0b100 */
    r  = GPFSEL1;
    r &= ~((7u << 12) | (7u << 15));     /* очистить поле функции */
    r |=  ((4u << 12) | (4u << 15));     /* выставить alt0 */
    GPFSEL1 = r;

    /* 2. Отключить подтяжку pull-up/pull-down на этих ногах
          (последовательность строго по datasheet BCM2835) */
    GPPUD = 0;
    delay(150);
    GPPUDCLK0 = (1u << 14) | (1u << 15);
    delay(150);
    GPPUDCLK0 = 0;

    /* 3. Настроить PL011 */
    UART_CR   = 0;          /* выключить на время настройки */
    UART_IBRD = 1;          /* 3 МГц / (16 * 115200) = 1.627... */
    UART_FBRD = 40;         /* дробная часть: 0.627 * 64 ≈ 40   */
    UART_LCRH = 0x70;       /* 8 бит данных + FIFO */
    UART_IMSC = 0;          /* все прерывания UART замаскированы */
    UART_CR   = 0x301;      /* UARTEN | TXE | RXE — поехали */
}

void uart_putc(char c)
{
    while (UART_FR & (1u << 5))   /* TXFF: ждать, пока FIFO не освободится */
        ;
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
