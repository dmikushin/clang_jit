# Compile C++ code in runtime with Clang

Compile C++ code in runtime with Clang, using the Clang cc1 entry point. Clang is linked into the executable statically and invoked via `cc1_main`. Please find a detailed description in the blog post [The simplest way to compile C++ with Clang at runtime](http://weliveindetail.github.io/blog/post/2017/07/25/compile-with-clang-at-runtime-simple.html).

This project is a fork of [author's original repository](https://github.com/weliveindetail/JitFromScratch), updated and tested to work with LLVM 12.

## Building

```
mkdir build
cd build
cmake ..
make
```

## Testing

The first test simply compiles the provided source code into LLVM IR, and prints it:

```
./test_clang_jit_cc1_1

Compiling the following source code in runtime:

extern "C" int abs(int);
extern int *customIntAllocator(unsigned items);

extern "C" int *integerDistances(int* x, int *y, unsigned items) {
  int *results = customIntAllocator(items);

  for (int i = 0; i < items; i++) {
    results[i] = abs(x[i] - y[i]);
  }

  return results;
}

; ModuleID = '/tmp/JitFromScratch-744ca5.bc'
source_filename = "/tmp/JitFromScratch-744ca5.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind optnone uwtable mustprogress
define dso_local i32* @integerDistances(i32* %x, i32* %y, i32 %items) #0 !dbg !7 {
entry:
...
```

The second test links the JIT-compiled against dependencies provided by the main executable, and runs it:

```
./test_clang_jit_cc1_2

Compiling the following source code in runtime:
extern "C" int abs(int);
extern int *customIntAllocator(unsigned items);

extern "C" int *integerDistances(int* x, int *y, unsigned items) {
  int *results = customIntAllocator(items);

  for (int i = 0; i < items; i++) {
    results[i] = abs(x[i] - y[i]);
  }

  return results;
}

Integer Distances: 3, 0, 3
```

The third test does the same, plus an IR optimization pass.
