/* mmu.h — MMU control and page table setup for ARM1176JZF-S (BCM2835).
 *  L1 table: 16 KB, 4096 entries (4 GB / 1 MB per entry).
 *  Domain 15 = No Access (guard page). */

#ifndef MMU_H
#define MMU_H

#include <stdint.h>

/* === Control Registers (ARM1176JZF-S) === */
#define SCTLR   0xEE015000  /* System Control Register (p15, 0, c1, c0, 0) */
#define TTBCR   0xEE015001  /* Translation Table Base Control (p15, 0, c2, c0, 2) */
#define TTBR0   0xEE015002  /* Translation Table Base 0 (p15, 0, c2, c0, 0) */
#define DACR    0xEE015003  /* Domain Access Control (p15, 0, c3, c0, 0) */
#define DFAR    0xEE015004  /* Fault Address Register (p15, 0, c6, c0, 0) */
#define DFSR    0xEE015005  /* Fault Status Register (p15, 0, c5, c0, 0) */
#define IFSR    0xEE015006  /* Instruction Fault Status (p15, 0, c5, c0, 1) */
#define CRR0    0xEE015008  /* Cache lockdown (p15, 0, c9, c0, 0) */

/* === L1 Entry Types (1 MB section) === */
#define L1_SECT       0x00000002u  /* Section entry (bits 1:0 = 10) */
#define L1_PAGE       0x00000001u  /* Page entry (not used in this stage) */

/* === Section Entry Attributes === */
/* Domain: bits 13..8 (shift 8) */
#define L1_DOMAIN_SHIFT   8u
#define L1_DOMAIN_MASK    0x3Fu
#define L1_DOMAIN(d)      (((d) & L1_DOMAIN_MASK) << L1_DOMAIN_SHIFT)

/* Permission bits for domain 0 (client) */
#define L1_AP_SHIFT       10u
#define L1_AP_NO_NO       (0u << L1_AP_SHIFT)  /* no access / no access */
#define L1_AP_RW_NO       (1u << L1_AP_SHIFT)  /* rw / no access */
#define L1_AP_RW_RW       (2u << L1_AP_SHIFT)  /* rw / rw */
#define L1_AP_RO_NO       (5u << L1_AP_SHIFT)  /* ro / no access */
#define L1_AP_RO_RO       (6u << L1_AP_SHIFT)  /* ro / ro */

/* Memory type and cacheability (bits 3..2, 12..11) */
#define L1_BUF            (1u << 2)    /* Bufferable */
#define L1_TEX_SHIFT      12u
#define L1_TEX(c, b, a)   (((c) & 1u) << (12+2) | ((b) & 1u) << 12 | ((a) & 1u) << 11)

/* Memory type field (bits 3..2) + TEX */
#define L1_MEM_NORMAL     (0u << 2)    /* Normal memory */
#define L1_MEM_DEVICE     (1u << 2)    /* Device memory */
#define L1_MEM_SHARED     (2u << 2)    /* Shared device */

/* Other flags */
#define L1_XN             (1u << 4)    /* Execute never (ARMv6) */
#define L1_SECTION        L1_SECT      /* Section entry */

/* === Domain Access Control === */
#define DACR_DOMAIN_SHIFT 0u
#define DACR_DOMAIN_MASK  0x3u
#define DACR_DOMAIN_NOCACHE 0u      /* No access */
#define DACR_DOMAIN_CLIENT  1u      /* Check permission bits */
#define DACR_DOMAIN_MANAGER 3u      /* Don't check permissions */

#define DACR_SET_DOMAIN(d, val) (((val) & DACR_DOMAIN_MASK) << ((d) * 2))

/* === SCTLR bits === */
#define SCTLR_M       (1u << 0)   /* MMU enable */
#define SCTLR_A       (1u << 1)   /* Alignment check (not used) */
#define SCTLR_C       (1u << 2)   /* Cache enable */
#define SCTLR_IW      (1u << 3)   /* Instruction cache enable */
#define SCTLR_V       (1u << 13)  /* Vectors at 0xFFFF0000 (not used) */

/* === Helper macros === */
/* L1 entry: section, domain, permissions, memory type */
#define L1_SECTION_ENTRY(addr, domain, ap, memtype) \
    (((addr) & 0xFFF00000u) | L1_SECT | L1_DOMAIN(domain) | (ap) | (memtype))

/* === Exported functions === */
void mmu_init(void);

#endif /* MMU_H */
