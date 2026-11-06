#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <trap.h>
#include "c_causes.h"

static char message[128];
static unsigned dumped;
static int cprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    return result;
}
void print_trapframe(struct trapframe *tf) {
    assert(tf != NULL);
    ++dumped;
}
#include "c_handler.inc"

int main(void) {
    /* Put a 32-bit encoding at an address aligned to two but not four bytes. */
    _Alignas(4) unsigned short code[] = {0, 0x9002, 0, 0x007b, 0};
    const unsigned slots[] = {0, 1, 3};
    const unsigned lengths[] = {2, 2, 4};
    for (unsigned cause = 2; cause <= 3; ++cause) {
        for (unsigned i = 0; i < 3; ++i) {
            struct trapframe tf, expected;
            memset(&tf, 0xa5, sizeof(tf));
            tf.cause = cause;
            tf.epc = (uintptr_t)&code[slots[i]];
            expected = tf;
            expected.epc += lengths[i];
            char output[128];
            snprintf(output, sizeof(output), "%s at 0x%016lx\n",
                     cause == 2 ? "Illegal instruction" : "Breakpoint",
                     (unsigned long)tf.epc);
            dumped = 0;
            message[0] = 0;
            exception_handler(&tf);
            assert(memcmp(&tf, &expected, sizeof(tf)) == 0);
            assert(strcmp(message, output) == 0 && dumped == 0);
            printf("PASS: cause=%u encoding=0x%04x length=%u, exact output and only EPC changed\n",
                   cause, code[slots[i]], lengths[i]);
        }
    }
    /* EPC=1 is intentionally unreadable: fallbacks must not fetch instructions. */
    const uintptr_t causes[] = {0, 1, 4, 5, 6, 7, 8, 9, 12, 13, 15, 20, UINTPTR_MAX};
    for (unsigned i = 0; i < sizeof(causes) / sizeof(causes[0]); ++i) {
        struct trapframe tf, expected;
        memset(&tf, 0x5a, sizeof(tf));
        tf.cause = causes[i];
        tf.epc = 1;
        expected = tf;
        dumped = 0;
        message[0] = 0;
        exception_handler(&tf);
        assert(dumped == 1 && message[0] == 0);
        assert(memcmp(&tf, &expected, sizeof(tf)) == 0);
    }
    puts("PASS: 13 fallback causes dump once, preserve full frame, never fetch EPC=1");
    return 0;
}
