#include "ClangDriver.h"

#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

// Show the error message and exit.
[[noreturn]] static void fatalError(Error E) {
  handleAllErrors(std::move(E), [&](const ErrorInfoBase &EI) {
    errs() << "Fatal Error: ";
    EI.log(errs());
    errs() << "\n";
    errs().flush();
  });

  exit(1);
}

// Test function.
static std::string sourceCode =
    "extern \"C\" int abs(int);                                           \n"
    "extern int *customIntAllocator(unsigned items);                      \n"
    "                                                                     \n"
    "extern \"C\" int *integerDistances(int* x, int *y, unsigned items) { \n"
    "  int *results = customIntAllocator(items);                          \n"
    "                                                                     \n"
    "  for (int i = 0; i < items; i++) {                                  \n"
    "    results[i] = abs(x[i] - y[i]);                                   \n"
    "  }                                                                  \n"
    "                                                                     \n"
    "  return results;                                                    \n"
    "}                                                                    \n";

int main(int argc, char **argv) {
  outs() << "Compiling the following source code in runtime:"
         << "\n"
         << sourceCode << "\n";

  // Compile C++ source code to LLVM IR module.
  LLVMContext context;
  ClangDriver driver;
  auto m = driver.compileTranslationUnit(sourceCode, context);
  if (!m)
    fatalError(m.takeError());

  // Pretty-print the module.
  outs() << *(m.get());

  return 0;
}
