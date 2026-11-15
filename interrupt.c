/* interrupt.c — диспетчер прерываний BCM2835.
 *  Все источники маршрутизируем в IRQ (FIQ не используем).
 *  Неизвестный источник не паникует, а маскируется и считается —
 *  ядро должно переживать чужой шум и оставаться живым для UART-лога. */

#include "interrupt.h"
#include "timer.h"

volatile uint32_t irq_count = 0;
volatile uint32_t irq_spurious = 0;

void irq_init(void)
{
    /* Запретить всё в обоих банках и в basic */
    IC_DISABLE_IRQS1 = 0xFFFFFFFFu;
    IC_DISABLE_IRQS2 = 0xFFFFFFFFu;
    IC_DISABLE_BASIC = 0xFFu;
    /* FIQ выключен: бит 7 = enable, биты [6:0] = номер источника */
    IC_FIQ_CONTROL   = 0;
}

void irq_enable(uint32_t line)
{
    if (line < 32u)
        IC_ENABLE_IRQS1 = 1u << line;
    else if (line < 64u)
        IC_ENABLE_IRQS2 = 1u << (line - 32u);
}

void irq_disable(uint32_t line)
{
    if (line < 32u)
        IC_DISABLE_IRQS1 = 1u << line;
    else if (line < 64u)
        IC_DISABLE_IRQS2 = 1u << (line - 32u);
}

void irq_dispatch(void)
{
    uint32_t p1, p2, pb;

    irq_count++;

    p1 = IC_IRQ_PENDING1;
    if (p1 & (1u << IRQ_SYSTIMER_C1)) {
        timer_irq_handler();            /* гасит источник (CS W1C) и переставляет compare */
        p1 &= ~(1u << IRQ_SYSTIMER_C1);
    }
    if (p1) {                           /* остаток — неожиданные источники */
        IC_DISABLE_IRQS1 = p1;
        irq_spurious++;
    }

    p2 = IC_IRQ_PENDING2;
    if (p2) {
        IC_DISABLE_IRQS2 = p2;
        irq_spurious++;
    }

    /* basic: биты 0..7 — свои источники ARM; 8..20 — зеркало GPU pending (RO) */
    pb = IC_IRQ_BASIC_PENDING & 0xFFu;
    if (pb) {
        IC_DISABLE_BASIC = pb;
        irq_spurious++;
    }
}
