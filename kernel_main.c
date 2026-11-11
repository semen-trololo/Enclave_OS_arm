#include "uart.h"

/* Кадр обязан совпадать с тем, что складывает EXC_HANDLER в boot.S:
   13 слов регистров, затем SPSR, LR_<mode> и вычисленный PC. */
struct exc_frame {
    unsigned int r[13];
    unsigned int spsr;
    unsigned int lr;
    unsigned int pc;
};

/* ===== ТОЧКИ КОНТАКТА С АССЕМБЛЕРОМ =====
   boot.S обращается к ним по абсолютным адресам, поэтому они НЕ static
   и лежат в .bss (её обнуляет _start до вызова kernel_main).

   exc_probe == 0  -> любое исключение фатально: обработчик печатает panic;
   exc_probe != 0  -> обработчик пишет номер в exc_caught, сохраняет PC в
                      exc_pc и возвращает управление на инструкцию сразу за
                      ловушкой. Так самопроверка лезет в исключения, не
                      убивая ядро. */
volatile unsigned int exc_probe;
volatile unsigned int exc_caught;
volatile unsigned int exc_pc;

#define EXC_UNDEF 1u
#define EXC_SVC   2u

/* Метки из boot.S: граница блока, копируемого в low memory, и верх стека SVC. */
extern unsigned int _vectors_start[];
extern unsigned int _vectors_end[];
extern char __svc_stack_top[];

static unsigned int checks_ok;
static unsigned int checks_fail;

static void putreg(int n, unsigned int value)
{
    uart_puts("r");
    if (n >= 10)
        uart_putc('0' + (n / 10));
    uart_putc('0' + (n % 10));
    uart_puts("=0x");
    uart_puthex(value);
    uart_putc(' ');
}

void panic(const char *tag, const struct exc_frame *frame)
{
    uart_puts("\n*** PANIC: ");
    uart_puts(tag);
    uart_puts(" ***\n  pc   = 0x");
    uart_puthex(frame->pc);
    uart_puts("\n  lr   = 0x");
    uart_puthex(frame->lr);
    uart_puts("\n  spsr = 0x");
    uart_puthex(frame->spsr);
    uart_puts("\n");

    for (int i = 0; i < 13; i += 4) {
        uart_puts("  ");
        for (int k = 0; k < 4 && i + k < 13; k++)
            putreg(i + k, frame->r[i + k]);
        uart_puts("\n");
    }

    uart_puts("--- halt ---\n");

    while (1) {
        asm volatile("nop");
    }
}

/* ========================== ОТЧЁТ В UART ==========================
   Всё, что нужно для сверки, печатает основное ядро: ни монитора, ни gdb
   на плате нет, единственный канал связи — эти строки. */

static void info(const char *label, unsigned int value)
{
    uart_puts("  ");
    uart_puts(label);
    uart_puts(" 0x");
    uart_puthex(value);
    uart_putc('\n');
}

/* Счётчики удобнее читать в десятичной системе, не в хексе. */
static void putdec(unsigned int v)
{
    char buf[10];
    int n = 0;

    do {
        buf[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0u && n < (int)sizeof(buf));

    while (n > 0)
        uart_putc(buf[--n]);
}

static void check(int ok, const char *what)
{
    uart_puts(ok ? "[PASS] " : "[FAIL] ");
    uart_puts(what);
    uart_putc('\n');
    if (ok)
        checks_ok++;
    else
        checks_fail++;
}

/* Чтение по фиксированному физическому адресу. Адрес спрятан от свёртки в
   константу: видя буквальное *(unsigned int *)0x100, GCC считает, что мы
   лезем за границу нулевого объекта, и ругается -Warray-bounds. */
static unsigned int peek32(unsigned int addr)
{
    asm volatile("" : "+r"(addr));
    return *(volatile unsigned int *)addr;
}

static unsigned int cpsr_now(void)
{
    unsigned int v;
    asm volatile("mrs %0, cpsr" : "=r"(v));
    return v;
}

static unsigned int sp_now(void)
{
    unsigned int v;
    asm volatile("mov %0, sp" : "=r"(v));
    return v;
}

/* ID в CP15: на ARM1176JZF-S номер части обязан быть 0xB76. */
static unsigned int cpu_id(void)
{
    unsigned int v;
    asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(v));
    return v;
}

static unsigned int cpu_control(void)
{
    unsigned int v;
    asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(v));
    return v;
}

/* Таблица векторов обязана лежать в low memory: на ARMv6 нет VBAR, CPU
   всегда берёт векторы с 0x00. Сравниваем копию с оригиналом пословно. */
static void check_vectors_copied(void)
{
    volatile const unsigned int *low = (volatile const unsigned int *)0x0;
    unsigned int len = (unsigned int)((char *)_vectors_end -
                                      (char *)_vectors_start);
    unsigned int bad = 0;

    for (unsigned int i = 0; i < len / 4u; i++) {
        if (low[i] != _vectors_start[i]) {
            bad = i * 4u;
            break;
        }
    }
    /* 8 векторов = 32 байта минимум, и весь блок обязан не лезть на ATAGS. */
    check(bad == 0 && len >= 32u && len < 0x100u, "vector table lives at 0x00000000");
    if (bad != 0) {
        uart_puts("       first mismatch at +0x");
        uart_puthex(bad);
        uart_putc('\n');
    }
}

/* Обнуление .bss делает boot.S — проверяем, что оно действительно случилось. */
static volatile unsigned int bss_probe[8];

static void check_bss(void)
{
    int ok = 1;

    for (int i = 0; i < 8; i++) {
        if (bss_probe[i] != 0u)
            ok = 0;
    }
    check(ok, "bss zeroed by boot.S");
}

static void check_stack(void)
{
    unsigned int top = (unsigned int)__svc_stack_top;
    unsigned int sp = sp_now();

    check(sp <= top && sp > top - 65536u, "svc stack is the one boot.S set up");
}

/* Пробы исключений. caught — номер, который записал обработчик; pc — PC,
   который он вычислил; want — фактический адрес ловушки, взятый ассемблером.
   Если код дожил до этой строки, значит: векторы в low memory работают,
   обработчик сохранил кадр и корректно вернулся (movs pc, lr). */
struct probe_result {
    unsigned int caught;
    unsigned int pc;
    unsigned int want;
};

static struct probe_result probe_undef(void)
{
    struct probe_result res;
    unsigned int want;

    exc_caught = 0u;
    exc_pc = 0u;
    exc_probe = EXC_UNDEF;
    asm volatile(
        "adr  %[w], .Lund_tr%=\n\t"
        ".Lund_tr%=: udf #0\n\t"
        "nop\n\t"
        : [w] "=&r"(want)
        :
        : "memory", "cc", "lr");
    res.caught = exc_caught;
    res.pc = exc_pc;
    res.want = want;
    exc_probe = 0u;
    return res;
}

static struct probe_result probe_svc(void)
{
    struct probe_result res;
    unsigned int want;

    exc_caught = 0u;
    exc_pc = 0u;
    exc_probe = EXC_SVC;
    asm volatile(
        "adr  %[w], .Lsvc_tr%=\n\t"
        ".Lsvc_tr%=: svc #0\n\t"
        "nop\n\t"
        : [w] "=&r"(want)
        :
        : "memory", "cc", "lr");
    res.caught = exc_caught;
    res.pc = exc_pc;
    res.want = want;
    exc_probe = 0u;
    return res;
}

/* Банкованные регистры трогать не должны: значение проходит сквозь
   исключение и обязано вернуться бит в бит (кадр r0..r12 сохраняют и
   восстанавливают целиком). */
static int probe_regs_preserved(void)
{
    unsigned int v = 0xA5A5F00Du;

    exc_caught = 0u;
    exc_probe = EXC_SVC;
    asm volatile(
        "svc #0\n\t"
        "nop\n\t"
        : "+r"(v)
        :
        : "memory", "cc", "lr");
    exc_probe = 0u;
    return exc_caught == EXC_SVC && v == 0xA5A5F00Du;
}

/* ATAG-список устроен так: по самому адресу лежит СЛОВО РАЗМЕРА тега, а его
   номер — следующим словом (ATAG_CORE = 0x54410001, размер 5). На плате
   firmware обязан оставить список на 0x100; QEMU raspi1ap его не пишет
   вообще — замер 11.11.2026, поэтому отсутствие тегов здесь признак
   эмулятора, а не поломки. Печатается, чтобы на плате сравнить одной
   строкой больше и не искать потом, чем отличается запуск. */
static void atags_report(unsigned int r2)
{
    unsigned int size_word = peek32(0x100u);
    unsigned int tag = peek32(0x104u);

    info("atag @0x100  ", size_word);
    info("atag id@0x104", tag);

    /* Строки отчёта — только ASCII: на плате их читает последовательный
       терминал, у которого кодирование может быть любым. */
    if (tag == 0x54410001u)
        uart_puts("  atags      : ATAG_CORE present\n");
    else
        uart_puts("  atags      : none (QEMU writes none; board must show 0x54410001)\n");

    if (r2 == 0x100u)
        uart_puts("  atags path : via r2\n");
    else
        uart_puts("  atags path : fallback 0x100 (r2 does not point at the list)\n");
}

static void selftest(unsigned int boot_r0, unsigned int machine, void *dtb)
{
    struct probe_result und;
    struct probe_result svc;
    unsigned int id = cpu_id();
    unsigned int len = (unsigned int)((char *)_vectors_end -
                                      (char *)_vectors_start);

    uart_puts("Enclave OS boot\n");
    uart_puts("BCM2835 / ARM1176JZF-S - boot report\n");

    uart_puts("--- INFO ---\n");
    info("cpu id       ", id);
    info("cp15 ctrl    ", cpu_control());
    info("cpsr         ", cpsr_now());
    info("sp           ", sp_now());
    info("svc stack top", (unsigned int)__svc_stack_top);
    info("vectors src  ", (unsigned int)_vectors_start);
    info("vectors size ", len);
    info("boot r0      ", boot_r0);
    info("boot r1 mach ", machine);
    info("boot r2 atags", (unsigned int)dtb);
    atags_report((unsigned int)dtb);

    uart_puts("--- CHECKS ---\n");
    check(((id >> 4) & 0xFFFu) == 0xB76u, "cpu part number is ARM1176 (0xb76)");
    check_bss();
    check_vectors_copied();
    check_stack();

    und = probe_undef();
    check(und.caught == EXC_UNDEF && und.pc == und.want,
          "undef instruction trapped and resumed");
    info("       pc     ", und.pc);
    info("       want   ", und.want);

    svc = probe_svc();
    check(svc.caught == EXC_SVC && svc.pc == svc.want, "svc trapped and resumed");
    info("       pc     ", svc.pc);
    info("       want   ", svc.want);

    check(probe_regs_preserved(), "r0-r12 survive an exception");

    uart_puts("--- RESULT ---\n");
    uart_puts("selftest: ");
    putdec(checks_ok);
    uart_puts(" passed, ");
    putdec(checks_fail);
    uart_puts(" failed\n");
    uart_puts(checks_fail == 0u ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    /* Дамп паники в этот прогон не входит: он останавливает ядро.
       Проверяется отдельной сборкой (-DTEST_UND / -DTEST_SVC). */
}

void kernel_main(unsigned int r0, unsigned int machine, void *dtb)
{
    uart_init();
    selftest(r0, machine, dtb);

    /* Проверка паники: сборка с -DTEST_UND / -DTEST_SVC умышленно валит
       ядро в исключение, чтобы увидеть дамп. exc_probe к этому моменту
       равен нулю, поэтому обработчик пойдёт именно в panic. */
#ifdef TEST_UND
    uart_puts("[test] udf #0 -> undef instruction\n");
    asm volatile("udf #0");
#endif
#ifdef TEST_SVC
    uart_puts("[test] svc #0 -> svc exception\n");
    asm volatile("svc #0");
#endif

    while (1) {
        ;
    }
}
