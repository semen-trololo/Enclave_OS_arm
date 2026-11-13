/* kernel_main.c — 1 вспышка LED + UART спам */

/* ===== GPIO47 LED (высокий = горит) ===== */
#define GPIO_BASE   0x20200000u
#define GPIO_FSEL4  (*(volatile unsigned int *)(GPIO_BASE + 0x10u))
#define GPIO_GPSET1 (*(volatile unsigned int *)(GPIO_BASE + 0x20u))
#define GPIO_GPCLR1 (*(volatile unsigned int *)(GPIO_BASE + 0x2Cu))
#define LED_BIT     (1u << 15)

/* ===== UART ===== */
#define MMIO_BASE   0x20000000u
#define GPFSEL1     (*(volatile unsigned int*)(MMIO_BASE + 0x200004))
#define UART_DR     (*(volatile unsigned int*)(MMIO_BASE + 0x201000))
#define UART_FR     (*(volatile unsigned int*)(MMIO_BASE + 0x201018))
#define UART_CR     (*(volatile unsigned int*)(MMIO_BASE + 0x201030))
#define UART_LCRH   (*(volatile unsigned int*)(MMIO_BASE + 0x20102C))
#define UART_IBRD   (*(volatile unsigned int*)(MMIO_BASE + 0x201024))
#define UART_FBRD   (*(volatile unsigned int*)(MMIO_BASE + 0x201028))

/* ===== Заглушки для обработчиков исключений ===== */
volatile unsigned int exc_probe;
volatile unsigned int exc_caught;
volatile unsigned int exc_pc;

void panic(const char *tag, void *frame)
{
    while (1)
        ;
}

static void uart_putc(char c)
{
    unsigned int timeout = 0xFFFFFF;
    while ((UART_FR & (1u << 5)) && timeout)
        timeout--;
    if (timeout)
        UART_DR = c;
}

static void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

void kernel_main(unsigned int r0, unsigned int machine, void *dtb)
{
    /* 1. LED — одна вспышка (GPIO47, высокий = горит) */
    unsigned int r = GPIO_FSEL4;
    r &= ~(7u << 21);
    r |= (1u << 21);
    GPIO_FSEL4 = r;

    GPIO_GPSET1 = LED_BIT;
    for (volatile unsigned int d = 0; d < 5000000u; d++)
        ;
    GPIO_GPCLR1 = LED_BIT;
    for (volatile unsigned int d = 0; d < 5000000u; d++)
        ;

    /* 2. GPIO14 -> Alt0 */
    r = GPFSEL1;
    r &= ~((7u << 12) | (7u << 15));
    r |=  ((4u << 12) | (4u << 15));
    GPFSEL1 = r;

    /* 3. UART регистры */
    UART_CR = 0;
    UART_IBRD = 1;
    UART_FBRD = 40;
    UART_LCRH = 0x70;
    UART_CR = (1u << 0) | (1u << 8);

    /* 4. Спам */
    uart_puts("Enclave OS Booting...\r\n");

    /* 5. Idle loop с WFI */
    while (1) {
        __asm__ volatile("wfi");
    }
}