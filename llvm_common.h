#ifndef LLVM_COMMON_
#define LLVM_COMMON_

#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BCPLUSPLUS__) || defined(__MWERKS__)
#if defined(NV_LIB_STATIC)
#define NVLLVM_EXPORT
#elif defined(NVLLVM_LIB)
#define NVLLVM_EXPORT __declspec(dllexport)
#else
#define NVLLVM_EXPORT __declspec(dllimport)
#endif
#else
#define NVLLVM_EXPORT
#endif

#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0602
#endif

#endif