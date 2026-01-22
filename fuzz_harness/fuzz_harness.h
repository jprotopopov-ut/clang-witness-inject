#ifndef __FUZZ_HARNESS_H__
#define __FUZZ_HARNESS_H__

#ifdef FUZZ_HARNESS_RAND_STDIN
extern int __fuzz_harness_rand(void);
#define rand __fuzz_harness_rand
#endif

// Per https://sv-comp.sosy-lab.org/2026/rules.php

#if defined(__GNUC__) || defined(__clang__)
#define int128 __int128
#endif

#define DECLARE_NONDET2(_id, _type) \
    _type __VERIFIER_nondet_##_id(void);

#define DECLARE_NONDET(_type) DECLARE_NONDET2(_type, _type)

DECLARE_NONDET2(bool, _Bool)
DECLARE_NONDET(char)
DECLARE_NONDET(int)
#if defined(__GNUC__) || defined(__clang__)
DECLARE_NONDET2(int128, __int128)
DECLARE_NONDET2(uint128, unsigned __int128)
#endif
DECLARE_NONDET(float)
DECLARE_NONDET(double)
// DECLARE_NONDET(loff_t)
DECLARE_NONDET(long)
DECLARE_NONDET2(longlong, long long)
// DECLARE_NONDET(pchar) // What is pchar?
DECLARE_NONDET2(pthread_t, unsigned long)
// DECLARE_NONDET(sector_t)
DECLARE_NONDET(short)
DECLARE_NONDET2(size_t, unsigned long long)
DECLARE_NONDET2(u32, unsigned int)
DECLARE_NONDET2(uchar, unsigned char)
DECLARE_NONDET2(ulong, unsigned long)
DECLARE_NONDET2(ulonglong, unsigned long long)
DECLARE_NONDET(unsigned)
DECLARE_NONDET2(ushort, unsigned short)

#undef DECLARE_NONDET
#undef DECLARE_NONDET2

void __VERIFIER_error(void);
void __VERIFIER_assume(int);
void __VERIFIER_nondet_memory(void *, unsigned long long);
void __VERIFIER_atomic_begin(void);
void __VERIFIER_atomic_end(void);

#endif
