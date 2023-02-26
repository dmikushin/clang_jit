# Compile C++ code in runtime with Clang

Compile C++ code in runtime with Clang, using the Clang frontend API.

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
./test_clang_jit_1

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

dclang -cc1 version 12.0.1 based upon LLVM 12.0.1 default target x86_64-pc-linux-gnu

; ModuleID = '-'
source_filename = "-"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind optnone
define dso_local i32* @integerDistances(i32* %x, i32* %y, i32 %items) #0 {
...
```

The second test links the JIT-compiled against dependencies provided by the main executable, and runs it:

```
./test_clang_jit_2

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

...

Integer Distances: 3, 0, 3
```

The third test does the same, plus an IR optimization pass.
