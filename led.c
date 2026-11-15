/* led.c — ACT LED на Pi 1 B+.
 *  На B+ светодиод переехал с GPIO16 (как на B) на GPIO47,
 *  полярность прямая: 1 = горит, 0 = погашен (подтверждено на плате).
 *  GPIO47 не выведен на гребёнку — только светодиод.
 *  В QEMU запись в GPIO безвредна, но светодиода там нет. */

#include "led.h"

#define GPIO_BASE   0x20200000u

/* GPIO47: GPFSEL4 биты [23:21] = 001 (output)
 *  GPSET1 бит 15 (47-32): записать 1 — поднять пин
 *  GPCLR1 бит 15: записать 1 — опустить пин */
#define GPIO_FSEL4      (*(volatile unsigned int *)(GPIO_BASE + 0x10u))
#define GPIO_GPSET1     (*(volatile unsigned int *)(GPIO_BASE + 0x20u))
#define GPIO_GPCLR1     (*(volatile unsigned int *)(GPIO_BASE + 0x2Cu))
#define LED_BIT         (1u << 15)

void led_init(void)
{
    unsigned int r = GPIO_FSEL4;
    r &= ~(7u << 21);
    r |=  (1u << 21);      /* output */
    GPIO_FSEL4 = r;
}

void led_on(void)
{
    GPIO_GPSET1 = LED_BIT;
}

void led_off(void)
{
    GPIO_GPCLR1 = LED_BIT;
}

void led_toggle(void)
{
    static int state = 0;
    if (state == 0) {
        led_on();
        state = 1;
    } else {
        led_off();
        state = 0;
    }
}
