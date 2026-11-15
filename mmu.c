/* mmu.c — MMU initialization and guard page for ARM1176JZF-S (BCM2835).
 *  Memory map:
 *    0x00000000-0x1FFFFFFF : RAM (512 MB)      - Normal memory, RW/RW
 *    0x20000000-0xFFFFFFFF : Peripherals        - Device memory, RW/RW
 *    0x0E000000 (224 MB)   : Guard page         - No Access (domain 15)
 *
 *  L1 table: 16 KB, 4096 entries (1 MB sections), 16 KB aligned.
 *  Guard page uses domain 15 with No Access (DACR[31:30] = 00). */

#include <stdint.h>
#include "mmu.h"

/* L1 translation table: 4096 entries × 4 bytes = 16 KB.
 * Must be 16 KB aligned. Placed in .bss (already zeroed by boot.S). */
static uint32_t __attribute__((aligned(16384))) l1_table[4096];

/* === MMU initialization === */
void mmu_init(void)
{
    uint32_t i;

    /* 1. Clear L1 table (boot.S already zeroed .bss, but be explicit) */
    /* Already zeroed by boot.S clearing __bss_start to __bss_end */

    /* 2. Map RAM (0x00000000-0x1FFFFFFF): 512 MB, Normal memory, RW/RW */
    /* Domain 0 = Client (check AP bits), AP = 0b11 (RW/RW), Bufferable, Shared */
    for (i = 0; i < 512; i++) {  /* 512 sections × 1 MB = 512 MB */
        l1_table[i] = L1_SECTION_ENTRY(
            i * 0x100000u,        /* section address */
            0,                    /* domain 0 */
            L1_AP_RW_RW,          /* read/write for all */
            L1_MEM_NORMAL | L1_BUF
        );
    }

    /* 3. Map Peripherals (0x20000000+): Device memory, RW/RW */
    /* Domain 0 = Client, AP = 0b11 (RW/RW), Non-bufferable */
    for (i = 0x20; i < 4096; i++) {  /* 0x20000000 to 0xFFFFFFFF */
        l1_table[i] = L1_SECTION_ENTRY(
            i * 0x100000u,        /* section address */
            0,                    /* domain 0 */
            L1_AP_RW_RW,          /* read/write for all */
            L1_MEM_DEVICE
        );
    }

    /* 4. Guard page at 0x0E000000 (224 MB): domain 15 = No Access */
    /* Domain 15 = bits 31:30 of DACR = 00 (No Access) */
    l1_table[0x0E] = L1_SECTION_ENTRY(
        0x0E000000u,            /* section address */
        15,                     /* domain 15 */
        L1_AP_NO_NO,            /* no access (ignored for domain 15) */
        L1_MEM_DEVICE           /* doesn't matter, domain 15 = no access */
    );

    /* 5. Set Domain Access Control Register */
    /* Domain 0 = Client (check AP bits), Domain 15 = No Access */
    {
        uint32_t dacr = 0;
        dacr |= DACR_SET_DOMAIN(0,  DACR_DOMAIN_CLIENT);   /* domain 0: check AP */
        dacr |= DACR_SET_DOMAIN(15, DACR_DOMAIN_NOCACHE);  /* domain 15: no access */
        __asm__ volatile("mcr p15, 0, %0, c3, c0, 0" : : "r"(dacr) : "memory");
    }

    /* 6. Set Translation Table Base 0 */
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 0" : : "r"(l1_table) : "memory");

    /* 7. Set Translation Table Base Control Register */
    /* TTBCR: N=0 (use TTBR0 only), EP=0, P=0 (domain check enabled) */
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 2" : : "r"(0) : "memory");

    /* 8. Invalidate TLB and branch predictor */
    __asm__ volatile("mcr p15, 0, %0, c8, c7, 0" : : "r"(0) : "memory");  /* TLBIALL */
    __asm__ volatile("mcr p15, 0, %0, c7, c5, 6" : : "r"(0) : "memory");  /* BPIALL */

    /* 9. Enable MMU in SCTLR */
    {
        uint32_t sctlr;
        __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr) : : "memory");
        sctlr |= SCTLR_M;  /* Enable MMU */
        /* Also enable I-cache (IW bit) */
        sctlr |= SCTLR_IW;
        __asm__ volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(sctlr) : "memory");
    }

    /* 10. Memory barriers after enabling MMU (ARMv6 compatible) */
    __asm__ volatile("mcr p15, 0, %0, c7, c10, 4" : : "r"(0) : "memory");  /* DSB */
    __asm__ volatile("mcr p15, 0, %0, c7, c5, 1" : : "r"(0) : "memory");   /* ISB */

    /* 11. Return from mmu_init (LR contains return address) */
    return;
}
