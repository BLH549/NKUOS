#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <trap.h>
#include "b_irq.h"

volatile size_t ticks;
static unsigned armed, printed, stopped, dumped;
static char events[4096], message[128];
static size_t event_count;
static size_t ticks_at_arm;

static void event(char ch) {
    assert(event_count + 1 < sizeof(events));
    events[event_count++] = ch;
    events[event_count] = 0;
}
static void clock_set_next_event(void) {
    ++armed;
    ticks_at_arm = ticks;
    event('A');
}
static void print_ticks(void) {
    assert(ticks == ticks_at_arm + 1);
    ++printed;
    event('P');
}
static void sbi_shutdown(void) {
    assert(printed > 0 && events[event_count - 1] == 'P');
    ++stopped;
    event('S');
}
static int cprintf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int result = vsnprintf(message, sizeof(message), format, ap);
    va_end(ap);
    event('M');
    return result;
}
void print_trapframe(struct trapframe *tf) {
    assert(tf != NULL);
    ++dumped;
    event('D');
}

#include "b_handler.inc"

static struct trapframe frame, original;
static void reset(size_t before, uintptr_t cause) {
    memset(&frame, 0xa5, sizeof(frame));
    frame.cause = cause | ((uintptr_t)1 << 63);
    original = frame;
    ticks = before;
    armed = printed = stopped = dumped = event_count = 0;
    events[0] = message[0] = 0;
}
static void unchanged(void) {
    assert(memcmp(&frame, &original, sizeof(frame)) == 0);
}

int main(void) {
    static const char *const messages[] = {
        "User software interrupt\n", "Supervisor software interrupt\n",
        "Hypervisor software interrupt\n", "Machine software interrupt\n",
        "User timer interrupt\n", NULL, "Hypervisor timer interrupt\n",
        "Machine timer interrupt\n", "User external interrupt\n",
        "Supervisor external interrupt\n", "Hypervisor external interrupt\n",
        "Machine external interrupt\n", "Coprocessor interrupt\n",
        "Host interrupt\n"
    };
    for (size_t cause = 0; cause < sizeof(messages) / sizeof(messages[0]); ++cause) {
        if (cause == IRQ_S_TIMER) continue;
        reset(71, cause);
        interrupt_handler(&frame);
        unchanged();
        assert(ticks == 71 && !armed && !printed && !stopped && !dumped);
        assert(strcmp(message, messages[cause]) == 0);
    }
    puts("PASS: 13 named non-timer IRQ branches; ticks and frame unchanged");
    reset(71, 15);
    interrupt_handler(&frame);
    unchanged();
    assert(ticks == 71 && !armed && !printed && !stopped && dumped == 1);
    assert(strcmp(message, "Unknown interrupt: cause 15\n") == 0);
    assert(strcmp(events, "MD") == 0);
    puts("PASS: unknown IRQ 15 prints cause then dumps frame");

    const size_t before[] = {0, 98, 99, 100, 998, 999};
    const char *const expected[] = {"A", "A", "AP", "A", "A", "APS"};
    for (size_t i = 0; i < sizeof(before) / sizeof(before[0]); ++i) {
        reset(before[i], IRQ_S_TIMER);
        interrupt_handler(&frame);
        unchanged();
        assert(ticks == before[i] + 1 && armed == 1 && !dumped);
        assert(strcmp(events, expected[i]) == 0);
        assert(printed == (ticks % 100 == 0) && stopped == (ticks == 1000));
        printf("PASS: ticks %zu -> %zu, order=%s\n", before[i], ticks, events);
    }
    reset(0, IRQ_S_TIMER);
    for (size_t i = 1; i <= 1000; ++i) {
        interrupt_handler(&frame);
        unchanged();
        assert(ticks == i && armed == i && printed == i / 100);
        assert(stopped == (i == 1000));
    }
    puts("PASS: 1000 calls, 1000 re-arms, 10 prints, shutdown after final print");
    return 0;
}
