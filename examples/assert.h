#ifndef __FUZZ_HARNESS_WITNESS_ASSERT__
#include <assert.h>
#define __WITNESS_ASSERT(_x) assert(_x)

#else
extern _Noreturn void __fuzz_harness_assert_fail(const char *, const char *, unsigned int, const char *);

#define __WITNESS_ASSERT(_x) \
    ((_Bool) (_x) \
        ? (void) 0 \
        : __fuzz_harness_assert_fail(#_x, __FILE__, __LINE__, __func__))
#endif