/* led.c — GPIO47 (ACT LED) control for Pi 1 B+ */

#include "led.h"

#define GPIO_BASE   0x20200000u

/* GPIO47: GPFSEL4 bits [23:21] = 001 (output)
   GPSET1 bit 15: write 1 to set (turn ON)
   GPCLR1 bit 15: write 1 to clear (turn OFF) */
#define GPIO_FSEL4      (*(volatile unsigned int *)(GPIO_BASE + 0x10u))
#define GPIO_GPSET1     (*(volatile unsigned int *)(GPIO_BASE + 0x20u))
#define GPIO_GPCLR1     (*(volatile unsigned int *)(GPIO_BASE + 0x2Cu))
#define LED_BIT         (1u << 15)

void led_init(void)
{
    unsigned int r = GPIO_FSEL4;
    r &= ~(7u << 21);      /* clear bits [23:21] */
    r |= (1u << 21);       /* set bits [23:21] = 001 (output) */
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