/* timer.c — тик ядра на System Timer (M4).
 *  Счётчик CLO свободно бежит на 1 МГц независимо от IRQ.
 *  Compare C1: когда CLO == C1, поднимается линия IRQ1;
 *  гасится записью 1 в бит 1 регистра CS (write-1-to-clear). */

#include "timer.h"
#include "interrupt.h"

#define ST_BASE     0x20003000u
#define ST_CS       (*(volatile uint32_t *)(ST_BASE + 0x00u))
#define ST_CLO      (*(volatile uint32_t *)(ST_BASE + 0x04u))
#define ST_CHI      (*(volatile uint32_t *)(ST_BASE + 0x08u))
#define ST_C1       (*(volatile uint32_t *)(ST_BASE + 0x10u))
#define ST_MATCH1   (1u << 1)

static volatile uint32_t ticks;
static uint32_t period;

void timer_init(uint32_t period_us)
{
    period = period_us;
    ticks  = 0;
    ST_CS  = ST_MATCH1;             /* снять возможный висящий match */
    ST_C1  = ST_CLO + period;
    irq_enable(IRQ_SYSTIMER_C1);
}

void timer_irq_handler(void)
{
    ST_CS  = ST_MATCH1;             /* W1C: линия IRQ падает только так */
    ST_C1 += period;                /* следующий тик (стабильная частота) */
    ticks++;
}

uint32_t timer_ticks(void)
{
    return ticks;
}

uint32_t timer_now_us(void)
{
    return ST_CLO;
}

void timer_delay_us(uint32_t us)
{
    uint32_t start = ST_CLO;
    while ((uint32_t)(ST_CLO - start) < us) {
        /* крутимся: CLO 32-битный, разность корректна и через wrap */
    }
}
