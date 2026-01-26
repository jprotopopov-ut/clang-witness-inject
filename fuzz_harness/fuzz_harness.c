#include "fuzz_harness.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

extern int __fuzz_harness_main_fn(int, char *[]);

static _Bool __fuzz_harness_get_exit_code(int *rc) {
    char *rc_env = getenv("FUZZ_HARNESS_RC");
    if (rc_env != NULL && *rc_env != '\0') {
        *rc = strtol(rc_env, NULL, 10);
        return true;
    } else {
        return false;
    }
}

__attribute__((used)) int __fuzz_harness_main(int argc, char *argv[]) {
    int rc = 0;
    _Bool override_rc = __fuzz_harness_get_exit_code(&rc);

    int ret = __fuzz_harness_main_fn(argc, argv);
    return override_rc ? rc : ret;
}

int __fuzz_harness_rand(void) {
    unsigned int res;
    fread(&res, sizeof(unsigned int), 1, stdin);
    return res % (RAND_MAX + 1u);
}

static int __fuzz_harness_timeout_handler_exit_code = 0;
static int __fuzz_harness_abort_handler_exit_code = 0;
static int __fuzz_harness_witness_assert_handler_exit_code = -1;
static int __fuzz_harness_sanitizer_death_handler_exit_code = -2;

_Noreturn void __fuzz_harness_assert_fail(const char *message, const char *file, unsigned int line, const char *function) {
    fprintf(stderr, "%s:%u: %s: Assertion `%s' failed.\n", file, line, function, message);
    exit(__fuzz_harness_witness_assert_handler_exit_code);
}

static void __fuzz_harness_timeout_handler(int sig) {
    (void) sig;
    fprintf(stderr, "Fuzz harness terminated on timeout\n");
    exit(__fuzz_harness_timeout_handler_exit_code);
}

static void __fuzz_harness_abort_handler(int sig) {
    (void) sig;
    fprintf(stderr, "Fuzz harness terminated on abort\n");
    exit(__fuzz_harness_abort_handler_exit_code);
}

static __attribute__((used)) void __fuzz_harness_sanitizer_death(void) {
    fprintf(stderr, "Fuzz harness terminated on sanitizer failure\n");
    exit(__fuzz_harness_sanitizer_death_handler_exit_code);
}

__attribute__((constructor)) static void __fuzz_harness_init(void) {
    const char *override_witness_assert = getenv("FUZZ_HARNESS_WITNESS_ASSERT_RC");
    if (override_witness_assert != NULL && *override_witness_assert != '\0') {
        __fuzz_harness_witness_assert_handler_exit_code = strtoull(override_witness_assert, NULL, 10);
    }

    const char *override_abort = getenv("FUZZ_HARNESS_ABORT_RC");
    if (override_abort != NULL && *override_abort != '\0') {
        __fuzz_harness_abort_handler_exit_code = strtoull(override_abort, NULL, 10);
        signal(SIGABRT, __fuzz_harness_abort_handler);
    }

    const char *override_sanitizer_death = getenv("FUZZ_HARNESS_SANITIZER_RC");
    if (override_sanitizer_death != NULL && *override_sanitizer_death != '\0') {
#if FUZZ_HARNESS_SANITIZER_HOOK != 0
        void __sanitizer_set_death_callback(void (*callback)(void));
        __fuzz_harness_sanitizer_death_handler_exit_code = strtoull(override_sanitizer_death, NULL, 10);
        __sanitizer_set_death_callback(__fuzz_harness_sanitizer_death);
#else
        fprintf(stderr, "FUZZ_HARNESS_SANITIZER_RC has no effect as fuzz harness has been compiler without sanitizer support!\n");
#endif
    }

    const char *timeout = getenv("FUZZ_HARNESS_TIMEOUT");
    if (timeout != NULL && *timeout != '\0') {
        __fuzz_harness_get_exit_code(&__fuzz_harness_timeout_handler_exit_code);
        unsigned long long timeout_value = strtoull(timeout, NULL, 10);

        signal(SIGALRM, __fuzz_harness_timeout_handler);
#define MICRO 1000000
        struct itimerval timer = {
            .it_value.tv_sec = timeout_value / MICRO,
            .it_value.tv_usec = timeout_value % MICRO
        };
#undef MICRO
        setitimer(ITIMER_REAL, &timer, NULL);
    }
}

// Per https://sv-comp.sosy-lab.org/2026/rules.php

#define DEFINE_NONDET2(_id, _type) \
    _type __VERIFIER_nondet_##_id(void) { \
        union { \
            char buffer[sizeof(_type)]; \
            _type value; \
        } value; \
        for (unsigned long i = 0; i < sizeof(_type); i++) { \
            value.buffer[i] = rand(); \
        } \
        return value.value; \
    }

#define DEFINE_NONDET(_type) DEFINE_NONDET2(_type, _type)

DEFINE_NONDET2(bool, _Bool)
DEFINE_NONDET(char)
DEFINE_NONDET(int)
#if defined(__GNUC__) || defined(__clang__)
DEFINE_NONDET2(int128, __int128)
DEFINE_NONDET2(uint128, unsigned __int128)
#endif
DEFINE_NONDET(float)
DEFINE_NONDET(double)
// DEFINE_NONDET(loff_t)
DEFINE_NONDET(long)
DEFINE_NONDET2(longlong, long long)
// DEFINE_NONDET(pchar) // What is pchar?
DEFINE_NONDET(pthread_t)
// DEFINE_NONDET(sector_t)
DEFINE_NONDET(short)
DEFINE_NONDET2(size_t, unsigned long long)
DEFINE_NONDET2(u32, uint32_t)
DEFINE_NONDET2(uchar, unsigned char)
DEFINE_NONDET2(ulong, unsigned long)
DEFINE_NONDET2(ulonglong, unsigned long long)
DEFINE_NONDET(unsigned)
DEFINE_NONDET2(ushort, unsigned short)

#undef DEFINE_NONDET
#undef DEFINE_NONDET2

void __VERIFIER_error(void) {
    abort();
}

void __VERIFIER_assume(int expression) {
    volatile int x;
    for (; !expression;) {
        x = 1;
    }
    return;
}

void __VERIFIER_nondet_memory(void *mem, unsigned long long size) {
    unsigned char *p = mem;
    for (size_t i = 0; i < size; i++) {
        p[i] = __VERIFIER_nondet_uchar();
    }
}

static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

void __VERIFIER_atomic_begin(void) {
    pthread_mutex_lock(&global_lock);
}

void __VERIFIER_atomic_end(void) {
    pthread_mutex_unlock(&global_lock);
}
