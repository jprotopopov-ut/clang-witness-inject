# A draft of Clang-based correctness witness injection

Dependencies: LLVM 22
```bash
make compdb -j$(nproc)
make GOBLINT="$HOME/goblint/analyzer/goblint --conf $HOME/goblint/analyzer/conf/examples/very-precise.json" test -j$(nproc)
```

Usage:
```bash
./witness_inject $CFLAGS --witness-yaml witness.yml --assert-fn assert file.c 
```