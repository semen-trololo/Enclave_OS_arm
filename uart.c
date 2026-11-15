/* uart.c — драйвер PL011 для Pi 1 B+, 115200 8N1.
 *  Тактирование: config.txt -> init_uart_clock=3000000, core_freq=250.
 *  Делители: 3000000 / (16 * 115200) = 1.6276 -> IBRD=1, FBRD=40.
 *  GPPUD/GPPUDCLK не трогаем: пины 14/15 уже настроены прошивкой
 *  (enable_uart=1), а кривая последовательность GPPUDCLK вешала плату. */

#include "uart.h"

#define MMIO_BASE   0x20000000u

/* --- GPIO --- */
#define GPFSEL1     (*(volatile unsigned int *)(MMIO_BASE + 0x200004u))

/* --- PL011 --- */
#define UART_DR     (*(volatile unsigned int *)(MMIO_BASE + 0x201000u))
#define UART_FR     (*(volatile unsigned int *)(MMIO_BASE + 0x201018u))
#define UART_IBRD   (*(volatile unsigned int *)(MMIO_BASE + 0x201024u))
#define UART_FBRD   (*(volatile unsigned int *)(MMIO_BASE + 0x201028u))
#define UART_LCRH   (*(volatile unsigned int *)(MMIO_BASE + 0x20102Cu))
#define UART_CR     (*(volatile unsigned int *)(MMIO_BASE + 0x201030u))
#define UART_IMSC   (*(volatile unsigned int *)(MMIO_BASE + 0x201038u))
#define UART_ICR    (*(volatile unsigned int *)(MMIO_BASE + 0x201044u))

#define FR_TXFF     (1u << 5)     /* transmit FIFO full */
#define FR_BUSY     (1u << 3)     /* UART передаёт символ */

void uart_init(void)
{
    unsigned int r;
    unsigned int timeout;

    /* 1. GPIO14 (TX) / GPIO15 (RX) -> Alt0 (PL011) */
    r  = GPFSEL1;
    r &= ~((7u << 12) | (7u << 15));
    r |=  ((4u << 12) | (4u << 15));
    GPFSEL1 = r;

    /* 2. Выключить UART перед сменой делителей/формата */
    UART_CR = 0;

    /* 3. Дождаться конца текущего символа (с таймаутом: вдруг нет клока) */
    timeout = 0xFFFFFu;
    while ((UART_FR & FR_BUSY) && timeout)
        timeout--;

    /* 4. Снять все висящие прерывания */
    UART_ICR = 0x7FFu;

    /* 5. Делители: 115200 при UART clock 3 MHz */
    UART_IBRD = 1;
    UART_FBRD = 40;

    /* 6. Кадр 8N1 + FIFO (WLEN=11, FEN=1) */
    UART_LCRH = 0x70u;

    /* 7. Прерывания UART замаскированы (работаем опросом) */
    UART_IMSC = 0;

    /* 8. UARTEN | TXE | RXE */
    UART_CR = (1u << 0) | (1u << 8) | (1u << 9);
}

void uart_putc(char c)
{
    /* Таймаут на случай мёртвого железа: лучше потерять символ, чем зависнуть */
    unsigned int timeout = 0xFFFFFu;
    while ((UART_FR & FR_TXFF) && timeout)
        timeout--;
    if (timeout)
        UART_DR = (unsigned int)c;
}

void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');      /* терминалы ждут CR+LF */
            uart_putc(*s++);
    }
}

void uart_puthex(unsigned int v)
{
    static const char hex_digits[] = "0123456789abcdef";
    int shift;
    for (shift = 28; shift >= 0; shift -= 4)
        uart_putc(hex_digits[(v >> shift) & 0xFu]);
}

void uart_putdec(unsigned int v)
{
    char buf[10];
    int i = 0;
    do {
        buf[i++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (i)
        uart_putc(buf[--i]);
}
