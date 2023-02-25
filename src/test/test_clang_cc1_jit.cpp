#include "ClangCC1Driver.h"

#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>

// Show the error message and exit.
LLVM_ATTRIBUTE_NORETURN static void fatalError(llvm::Error E)
{
	llvm::handleAllErrors(std::move(E), [&](const llvm::ErrorInfoBase &EI)
	{
		llvm::errs() << "Fatal Error: ";
		EI.log(llvm::errs());
		llvm::errs() << "\n";
		llvm::errs().flush();
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

int main(int argc, char **argv)
{
	using namespace llvm;

	outs() << "Compiling the following source code in runtime:" << "\n" << sourceCode << "\n";

	// Compile C++ source code to LLVM IR module.
	LLVMContext context;
	ClangCC1Driver driver;
	auto m = driver.compileTranslationUnit(sourceCode, context);
	if (!m)
		fatalError(m.takeError());

	// Pretty-print the module.
	outs() << *(m.get());

	return 0;
}

