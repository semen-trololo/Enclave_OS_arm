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
static unsigned int checks_skip;

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

/* Проверка, которую в этой среде проверить нельзя. В отличие от FAIL она не
   портит вердикт: отсутствие тегов прошивки в QEMU — свойство эмулятора, а не
   поломка ядра. На плате те же строки станут PASS или FAIL. */
static void skip(const char *what)
{
    uart_puts("[SKIP] ");
    uart_puts(what);
    uart_putc('\n');
    checks_skip++;
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
        : "+&r"(v)                    /* early-clobber: v не может пересекаться с метками */
        :
        : "memory", "cc", "lr");
    exc_probe = 0u;
    return exc_caught == EXC_SVC && v == 0xA5A5F00Du;
}

/* ========================== ATAG-список (M2) ==========================
   Прошивка GPU оставляет ядру «записку» в оперативной памяти: сколько всего
   памяти, какая ревизия платы, что за командная строка. Адрес записки прошивка
   передаёт в r2, на Pi это 0x100. Каждый тег:

        +0   size — размер тега в СЛОВАХ, включая эти два поля
        +4   tag  — номер тега
        +8   …    — данные

   Список заканчивается тегом с size == 0 (ATAG_NONE). По самому адресу лежит
   именно size, а номер — следующим словом.

   Парсер обязан выдержать мусор: порванный список не должен увести ядро в
   бесконечный цикл или в чтение по дикому адресу — на плате это висящая плата
   без единого символа в UART, то есть несъёмная SD-карта ради диагностики. */

#define ATAG_NONE     0x00000000u
#define ATAG_CORE     0x54410001u
#define ATAG_MEM      0x54410002u
#define ATAG_SERIAL   0x54410006u
#define ATAG_REVISION 0x54410007u
#define ATAG_CMDLINE  0x54410009u

/* Границы с запасом на порядок: реальный список прошивки — десятки слов. */
#define ATAG_MAX_WORDS 1024u          /* 4 КБ на весь список */
#define ATAG_MAX_TAGS  64u
#define ATAG_MIN_WORDS 2u             /* меньше заголовка тег не занимает */

/* Коды отказа — печатаются числом, чтобы на плате не пришлось гадать. */
#define ATAG_OK        0u
#define ATAG_BAD_ALIGN 1u
#define ATAG_BAD_SIZE  2u
#define ATAG_TOO_BIG   3u
#define ATAG_NO_END    4u

struct atag_walk {
    unsigned int tags;    /* сколько тегов обошли */
    unsigned int ram;     /* сумма ATAG_MEM, байт */
    unsigned int bad;     /* ATAG_OK = список честный и дошёл до ATAG_NONE */
};

static struct atag_walk atag_walk_list(unsigned int addr, int verbose)
{
    struct atag_walk res;
    unsigned int base = addr;
    unsigned int used = 0;
    int done = 0;

    res.tags = 0;
    res.ram = 0;
    res.bad = ((base & 3u) != 0u) ? ATAG_BAD_ALIGN : ATAG_OK;

    while (res.bad == ATAG_OK && res.tags < ATAG_MAX_TAGS &&
           used + ATAG_MIN_WORDS <= ATAG_MAX_WORDS) {
        unsigned int size = peek32(base);
        unsigned int tag = peek32(base + 4u);

        if (size == 0u) {           /* ATAG_NONE — конец списка */
            done = 1;
            break;
        }
        if (size < ATAG_MIN_WORDS) {
            res.bad = ATAG_BAD_SIZE;
            break;
        }
        if (size > ATAG_MAX_WORDS - used) {
            res.bad = ATAG_TOO_BIG;    /* тег вылезает за границу списка */
            break;
        }

        if (tag == ATAG_MEM)
            res.ram += peek32(base + 8u);   /* у ATAG_MEM размер идёт первым */

        if (verbose) {
            uart_puts("  tag 0x");
            uart_puthex(tag);
            uart_puts(", ");
            putdec(size);
            uart_puts(" words");
            if (tag == ATAG_MEM) {
                uart_puts(": ");
                putdec(peek32(base + 8u) / (1024u * 1024u));
                uart_puts(" MB @ 0x");
                uart_puthex(peek32(base + 12u));
            } else if (tag == ATAG_CMDLINE) {
                uart_puts(": ");
                for (unsigned int i = 0; i < (size - ATAG_MIN_WORDS) * 4u; i++) {
                    char c = (char)((peek32(base + 8u + (i & ~3u)) >>
                                     (8u * (i & 3u))) & 0xFFu);
                    if (c == '\0')
                        break;
                    uart_putc(c);
                }
            }
            uart_putc('\n');
        }

        base += size * 4u;
        used += size;
        res.tags++;
    }

    if (res.bad == ATAG_OK && !done)
        res.bad = ATAG_NO_END;      /* упёрлись в лимит, ATAG_NONE не встретили */

    return res;
}

/* Похоже ли на начало списка: выровнено, ненулевое, первым идёт ATAG_CORE,
   размер заголовка правдоподобен. Это единственное, чему можно доверять в
   адресе, который нам принёс r2. */
static int atag_head_ok(unsigned int addr)
{
    unsigned int size;
    unsigned int tag;

    if (addr == 0u || (addr & 3u) != 0u)
        return 0;

    size = peek32(addr);
    tag = peek32(addr + 4u);
    return size >= ATAG_MIN_WORDS && size <= ATAG_MAX_WORDS && tag == ATAG_CORE;
}

/* r2 верить можно, только если по нему реально читается список; иначе
   fallback на зафиксированный 0x100. Путь обязательно печатается. */
static unsigned int atags_pick(unsigned int r2)
{
    if (atag_head_ok(r2))
        return r2;
    if (atag_head_ok(0x100u))
        return 0x100u;
    return 0u;
}

/* synthetic-списки: проверки парсера не должны зависеть от того, оставила ли
   прошивка теги. Здесь ровно то, что обязан правильно посчитать парсер:
   3 тега и 512 МБ памяти суммарно. */
static const unsigned int tags_good[] = {
    5, ATAG_CORE, 1u, 0u, 0u,
    4, ATAG_MEM, 256u * 1024u * 1024u, 0x00000000u,
    4, ATAG_MEM, 256u * 1024u * 1024u, 0x10000000u,
    0, 0
};

/* Тег «размером» больше всего списка — так выглядит мусор в памяти. */
static const unsigned int tags_huge[] = { 2000u, ATAG_CORE, 0u, 0u, 0u, 0u, 0u };
/* Тег меньше заголовка — парсер обязан встать, а не читать вечно. */
static const unsigned int tags_stub[] = { 1u, ATAG_MEM, 0u, 0u };

static void check_atags(unsigned int r2)
{
    struct atag_walk w = atag_walk_list((unsigned int)tags_good, 0);
    unsigned int mb = (tags_good[7] + tags_good[11]) / (1024u * 1024u);

    check(w.bad == ATAG_OK && w.tags == 3u && w.ram == mb * 1024u * 1024u,
          "atag parser reads a synthetic list");
    if (w.bad != ATAG_OK || w.tags != 3u) {
        uart_puts("       walk: tags ");
        putdec(w.tags);
        uart_puts(", bad ");
        putdec(w.bad);
        uart_putc('\n');
    }

    /* Мусор должен быть отвергнут, а не приведёт к зависанию. */
    check(atag_walk_list((unsigned int)tags_huge, 0).bad == ATAG_TOO_BIG,
          "atag parser rejects an oversized tag");
    check(atag_walk_list((unsigned int)tags_stub, 0).bad == ATAG_BAD_SIZE,
          "atag parser rejects a sub-header tag");

    check(atag_head_ok((unsigned int)tags_good) == 1 &&
          atag_head_ok(0u) == 0 &&
          atag_head_ok((unsigned int)tags_good + 1u) == 0 &&
          atag_head_ok((unsigned int)tags_huge) == 0,
          "atag pointer sanity accepts good, rejects junk");

    /* Список от прошивки: в QEMU его нет, поэтому тут SKIP, а не FAIL. */
    if (atags_pick(r2) != 0u) {
        struct atag_walk fw = atag_walk_list(atags_pick(r2), 1);
        check(fw.bad == ATAG_OK, "firmware tag list walks clean");
        uart_puts("       firmware ram: ");
        putdec(fw.ram / (1024u * 1024u));
        uart_puts(" MB in ");
        putdec(fw.tags);
        uart_puts(" tags\n");
    } else {
        skip("firmware tag list walks clean");
    }
}

/* Отчёт по прошивочным тегам: что нашли, каким путём, сколько памяти. */
static void atags_report(unsigned int r2)
{
    unsigned int base = atags_pick(r2);

    info("atag @0x100  ", peek32(0x100u));
    info("atag id@0x104", peek32(0x104u));

    /* Строки отчёта — только ASCII: на плате их читает последовательный
       терминал, у которого кодирование может быть любым. */
    if (base == 0u) {
        uart_puts("  atags      : none (QEMU writes none; board must show 0x54410001)\n");
        uart_puts("  atags path : no valid list\n");
    } else {
        uart_puts("  atags      : list at 0x");
        uart_puthex(base);
        uart_putc('\n');
        uart_puts("  atags path : ");
        uart_puts(base == r2 ? "via r2\n" : "fallback 0x100\n");
    }
}

/* ===== Светодиод ACT как второй канал диагностики =====
   Пока UART под подозрением, канал проверки не должен от него зависеть:
   если отчёт не пришёл, по числу вспышек видно, как далеко успело зайти
   ядро. На Pi 1 B+ ACT висит на GPIO16 и горит при нуле на ноге
   (активный низкий), поэтому «зажечь» — это GPCLR2, а «погасить» — GPSET2. */
#define GPIO_BASE   0x20200000u
#define GPIO_FSEL4  (*(volatile unsigned int *)(GPIO_BASE + 0x10u))
#define GPIO_SET2   (*(volatile unsigned int *)(GPIO_BASE + 0x1Cu))
#define GPIO_CLR2   (*(volatile unsigned int *)(GPIO_BASE + 0x20u))
#define ACT_BIT     (1u << 16)

static void led_delay(void)
{
    /* volatile запрещает выбросить цикл: без него -O2 оставил бы пустоту */
    for (volatile unsigned int i = 0; i < 10000000u; i++)
        ;
}

/* n вспышек — это этап загрузки. 3 = _start (задаёт boot.S),
   5 = uart_init вернулся, то есть C-часть жива и PL011 инициализирован. */
static void led_code(int n)
{
    unsigned int r = GPIO_FSEL4;

    r &= ~(7u << 18);         /* GPFSEL4, поле FSEL16: ноги 15..19 по 3 бита */
    r |= 1u << 18;            /* 001 = выход */
    GPIO_FSEL4 = r;

    for (int i = 0; i < n; i++) {
        GPIO_CLR2 = ACT_BIT;
        led_delay();
        GPIO_SET2 = ACT_BIT;
        led_delay();
    }
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

    /* Пять вспышек = uart_init и два uart_puts прошли, то есть драйвер не
       завис в ожидании освобождения TX-FIFO. Дальше молчание UART может быть
       только из-за линии (земля, ножка, скорость), но не из-за кода. */
    led_code(5);

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

    check_atags((unsigned int)dtb);

    uart_puts("--- RESULT ---\n");
    uart_puts("selftest: ");
    putdec(checks_ok);
    uart_puts(" passed, ");
    putdec(checks_fail);
    uart_puts(" failed, ");
    putdec(checks_skip);
    uart_puts(" skipped\n");
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
