/* interrupt.h — контроллер прерываний BCM2835 @ 0x2000B200.
 Э то НЕ PL190/VIC: зде*сь нет VECTADDR/VICADDRESS/EOI.
 Регистры Enable/Disable — write-only, pending читается отдельно.
 Источник гасится в самом устройстве (для System Timer — W1C в CS). */

#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

#define IC_BASE             0x2000B200u

#define IC_IRQ_BASIC_PENDING (*(volatile uint32_t *)(IC_BASE + 0x00u)) /* RO */
#define IC_IRQ_PENDING1      (*(volatile uint32_t *)(IC_BASE + 0x04u)) /* RO */
#define IC_IRQ_PENDING2      (*(volatile uint32_t *)(IC_BASE + 0x08u)) /* RO */
#define IC_FIQ_CONTROL       (*(volatile uint32_t *)(IC_BASE + 0x0Cu))
#define IC_ENABLE_IRQS1      (*(volatile uint32_t *)(IC_BASE + 0x10u)) /* WO */
#define IC_ENABLE_IRQS2      (*(volatile uint32_t *)(IC_BASE + 0x14u)) /* WO */
#define IC_ENABLE_BASIC      (*(volatile uint32_t *)(IC_BASE + 0x18u)) /* WO */
#define IC_DISABLE_IRQS1     (*(volatile uint32_t *)(IC_BASE + 0x1Cu)) /* WO */
#define IC_DISABLE_IRQS2     (*(volatile uint32_t *)(IC_BASE + 0x20u)) /* WO */
#define IC_DISABLE_BASIC     (*(volatile uint32_t *)(IC_BASE + 0x24u)) /* WO */

/* Номера линий (банк 1: 0..31, банк 2: 32..63) */
#define IRQ_SYSTIMER_C1     1u      /* System Timer compare 1 — свободен для ARM */
#define IRQ_SYSTIMER_C3     3u      /* System Timer compare 3 — свободен для ARM */
#define IRQ_PL011           57u     /* UART0 (банк 2, бит 25) */

extern volatile uint32_t irq_count;     /* всего IRQ */
extern volatile uint32_t irq_spurious;  /* неожиданных источников (замаскированы) */

void irq_init(void);
void irq_enable(uint32_t line);
void irq_disable(uint32_t line);
void irq_dispatch(void);    /* вызывается из irq_handler в boot.S */

#endif /* INTERRUPT_H */
