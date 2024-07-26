#include "ClangDriver.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace orc;

// Show the error message and exit.
[[noreturn]] static void fatalError(Error E)
{
	handleAllErrors(std::move(E), [&](const ErrorInfoBase &EI)
	{
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

typedef int* (*integerDistances_t)(int* x, int *y, unsigned items);

// Determine the size of a C array at compile-time.
template <typename T, size_t sizeOfArray>
constexpr unsigned arrayElements(T (&)[sizeOfArray])
{
	return sizeOfArray;
}
 
// This function will be called from JITed code.
int *customIntAllocator(unsigned items)
{
	static int memory[100];
	static unsigned allocIdx = 0;

	if (allocIdx + items > arrayElements(memory))
		exit(-1);

	int *block = memory + allocIdx;
	allocIdx += items;

	return block;
}

// ExitOnErr handles any errors that occur during this process.
ExitOnError ExitOnErr;

int main(int argc, char *argv[])
{
	// Initialize LLVM.
	InitLLVM X(argc, argv);

	// Initialize the native target, which is necessary for JIT compilation.
	InitializeNativeTarget();

	// Initialize the native target's assembly printer,
	// which is required for generating machine code.
	InitializeNativeTargetAsmPrinter();

	outs() << "Compiling the following source code in runtime:" << "\n" << sourceCode << "\n";

	// Creates a new LLVM context, which holds the state
	// for the LLVM intermediate representation (IR).
	auto C = std::make_unique<LLVMContext>();

	// Compile C++ source code to LLVM IR module.
	ClangDriver driver;
	auto M = driver.compileTranslationUnit(sourceCode, *C);
	if (!M)
		fatalError(M.takeError());

	// Create an instance of LLJIT using the LLJITBuilder.
	auto J = ExitOnErr(LLJITBuilder().create());

	// Get the main JIT dynamic library, which will hold the compiled code.
	JITDylib &JD = J->getMainJITDylib();

	// Add a generator to the JIT dynamic library to resolve symbols from the current process.
	// Note: in order for the JIT dynamic library to use the symbols from the current process,
	// those symbols must be present in the dynamic table. If the symbols are defined in the
	// executable image, they won't be accessible to JIT, because the dynamic table is not
	// generated for an executable by default. The solution is to link the executable with
	// --export-dynamic, or set_target_properties(target PROPERTIES ENABLE_EXPORTS ON) for CMake
	JD.addGenerator(cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(J->getDataLayout().getGlobalPrefix())));
	
	// Add the compiled LLVM IR module to the JIT dynamic library.
	ExitOnErr(J->addIRModule(JD, ThreadSafeModule(std::move(M.get()), std::move(C))));

	// Look up the JIT'd function, cast it to a function pointer, then call it.
	auto sym = ExitOnErr(J->lookup("integerDistances"));
	auto integerDistances = sym.toPtr<integerDistances_t>();

	int x[]{0, 1, 2};
	int y[]{3, 1, -1};

	int *z = integerDistances(x, y, 3);
 
	outs() << format("Integer Distances: %d, %d, %d\n\n", z[0], z[1], z[2]);
	outs().flush();

	return 0;
}

