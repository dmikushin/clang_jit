#include "ClangCC1Driver.h"

//#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
//#include <llvm/IR/Verifier.h>
//#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
//#include <llvm/Support/ManagedStatic.h>
//#include <llvm/Support/PrettyStackTrace.h>
//#include <llvm/Support/Signals.h>
//#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

// Show the error message and exit.
LLVM_ATTRIBUTE_NORETURN static void fatalError(llvm::Error E) {
  llvm::handleAllErrors(std::move(E), [&](const llvm::ErrorInfoBase &EI) {
    llvm::errs() << "Fatal Error: ";
    EI.log(llvm::errs());
    llvm::errs() << "\n";
    llvm::errs().flush();
  });

  exit(1);
}

int main(int argc, char **argv) {
  using namespace llvm;

  //sys::PrintStackTraceOnErrorSignal(argv[0]);
  //PrettyStackTraceProgram X(argc, argv);

  //atexit(llvm_shutdown);

  // Parse -debug and -debug-only options.
  //cl::ParseCommandLineOptions(argc, argv, "JitFromScratch example project\n");

  // Implementation of the integerDistances function.
  std::string sourceCode =
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

  outs() << "Compiling the following source code in runtime:" << "\n" << sourceCode << "\n";

  // Compile C++ to bitcode.
  LLVMContext context;
  ClangCC1Driver driver;
  auto m = driver.compileTranslationUnit(sourceCode, context);
  if (!m)
    fatalError(m.takeError());

  // Pretty-print the module.
  outs() << *(m.get());

  return 0;
}

