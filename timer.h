/* timer.h — System Timer BCM2835 @ 0x20003000, 1 МГц, compare-канал C1.
 C 0/C2 использует *GPU, C1/C3 свободны для ARM.
 SP804 (0x2000B400) не используем: в QEMU он не реализован. */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void     timer_init(uint32_t period_us);  /* тик заданной длины, IRQ по C1 */
void     timer_irq_handler(void);         /* из irq_dispatch: W1C + re-arm + ticks++ */
uint32_t timer_ticks(void);               /* счётчик тиков с момента timer_init */
uint32_t timer_now_us(void);              /* младшие 32 бита CLO (wrap ~71.6 мин) */
void     timer_delay_us(uint32_t us);     /* busy-wait по CLO, для таймаутов selftest */

#endif /* TIMER_H */
