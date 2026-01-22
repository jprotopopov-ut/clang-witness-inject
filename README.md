# A draft of Clang-based correctness witness injection

Dependencies: LLVM 22
```bash
make compdb -j$(nproc)
make GOBLINT="$HOME/goblint/analyzer/goblint" TEST_CC=clang-22 test -j$(nproc)
```

Usage:
```bash
./witness_inject $CFLAGS --witness-yaml witness.yml --assert-fn assert file.c # file.c should be exactly the same file mentioned in the witness file_name location entries
```

Injects location and loop invariants from `witness.yml` into the source code in form of assertions.

Optionally, `strip_malformed_asserts.py` script can be used to strip
syntactically invalid assertions produced by injection phase:
```bash
clang-22 -w -fsyntax-only -fdiagnostics-format=sarif -Wno-sarif-format-unstable -ferror-limit=0 file.c 2>&1 | head -n2 | jq > sarif.json
./strip_malformed_asserts.py --sarif-json sarif.json --assert-fn assert build/examples/thread-join-binomial/injected.c
```

## SV-COMP + AFL++ integration
`fuzz_harness` is a basic harness that provides definitions for SV-COMP
`__VERIFIER_*` APIs and thus enables execution of SV-COMP benchmarks. .

Environment variables:
* `FUZZ_HARNESS_RC` -- overrides return code for `main` function. Some SV-COMP
  benchmarks return non-zero exit codes even irrespective of actual result. This
  shall mitigate the issue.
* `FUZZ_HARNESS_TIMEOUT` -- terminates execution after `N` microseconds, exiting
  with `0` exit code. To avoid benchmarks that loop indefinitely.

Macros:
* `FUZZ_HARNESS_RAND_STDIN` -- if defined, all `__VERIFIED_nondet_*` and `rand`
calls are turned into reads from stdin.

The benchmark should be built and linked as follows:
```bash
$CC $CFLAGS -include fuzz_harness/fuzz_harness.h benchmark.c fuzz_harness/fuzz_harness.c fuzz_harness/fuzz_harness.s -o benchmark.exe $LDFLAGS -Wl,-e,__fuzz_harness_start
```

Primary use case is fuzzing via
[AFL++](https://github.com/AFLplusplus/AFLplusplus). For provided examples,
instrumented executables can be built as follows:
```bash
AFLPP_CC=$HOME/AFLpp/bin/afl-clang-lto make build/test/atomic-gcc/atomic-gcc.afl # Update the target, add AFLPP_CFLAGS and AFLPP_LDFLAGS as needed
```