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

Optionally, `skip_invalid_assertions.py` script can be used to strip
syntactically invalid assertions produced by injection phase:
```bash
clang-22 -w -fsyntax-only -fdiagnostics-format=sarif -Wno-sarif-format-unstable -ferror-limit=0 file.c 2>&1 | head -n2 | jq > sarif.json
./skip_invalid_assertions.py --sarif-json sarif.json --assert-fn assert build/examples/thread-join-binomial/injected.c
```