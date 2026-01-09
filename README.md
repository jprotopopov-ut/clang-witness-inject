# A draft of Clang-based correctness witness injection

Dependencies: LLVM 22
```bash
make compdb -j$(nproc)
make GOBLINT="$HOME/goblint/analyzer/goblint --conf $HOME/goblint/analyzer/conf/examples/very-precise.json" test -j$(nproc)
```

Usage:
```bash
./witness_inject $CFLAGS --witness-yaml witness.yml --assert-fn assert file.c # file.c should be exactly the same file mentioned in the witness file_name location entries
```

Injects location and loop invariants from `witness.yml` into the source code in form of assertions.