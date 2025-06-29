# LLVM Passes Library

A Library of LLVM passes built with LLVM 20 and based on the Skeleton pass
template[^1].

Build:

```sh
    mkdir build
    cd build
    cmake ..
    make
    cd ..
```

Run:

```sh
    clang -fpass-plugin=`echo build/skeleton/SkeletonPass.*` something.c
```

## Passes

* MBA Obfuscation.
* Basic Block Viewer.
* Skeleton from [^1].

[^1]: <https://github.com/sampsyo/llvm-pass-skeleton>
