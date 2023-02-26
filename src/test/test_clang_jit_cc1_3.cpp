#include "ClangCC1Driver.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

using namespace llvm;
using namespace orc;

// A function object that creates a simple pass pipeline to apply to each
// module as it passes through the IRTransformLayer.
class SimpleOptimizer
{
	llvm::PassManagerBuilder B;
  
public :

	SimpleOptimizer(unsigned OptLevel = 3) { B.OptLevel = OptLevel; }

	Expected<ThreadSafeModule> operator()(ThreadSafeModule TSM, MaterializationResponsibility &R)
	{
		TSM.withModuleDo([this](Module &M)
		{
			dbgs() << "--- BEFORE OPTIMIZATION ---\n" << M << "\n";
    
			legacy::FunctionPassManager FPM(&M);
			B.populateFunctionPassManager(FPM);

			FPM.doInitialization();
			for (Function &F : M)
				FPM.run(F);
			FPM.doFinalization();

			legacy::PassManager MPM;
			B.populateModulePassManager(MPM);
			MPM.run(M);    

			dbgs() << "--- AFTER OPTIMIZATION ---\n" << M << "\n";
		});

		return std::move(TSM);
	}
};

// Show the error message and exit.
LLVM_ATTRIBUTE_NORETURN static void fatalError(Error E)
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

ExitOnError ExitOnErr;

int main(int argc, char *argv[])
{
	// Initialize LLVM.
	InitLLVM X(argc, argv);

	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();

	outs() << "Compiling the following source code in runtime:" << "\n" << sourceCode << "\n";

	// Compile C++ source code to LLVM IR module.
	auto C = std::make_unique<LLVMContext>();
	ClangCC1Driver driver;
	auto M = driver.compileTranslationUnit(sourceCode, *C);
	if (!M)
		fatalError(M.takeError());

	auto J = ExitOnErr(LLJITBuilder().create());

	// Install transform to optimize modules when they're materialized.
	J->getIRTransformLayer().setTransform(SimpleOptimizer());

	JITDylib &JD = J->getMainJITDylib();
	JD.addGenerator(cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(J->getDataLayout().getGlobalPrefix())));
	ExitOnErr(J->addIRModule(JD, ThreadSafeModule(std::move(M.get()), std::move(C))));

	// Look up the JIT'd function, cast it to a function pointer, then call it.
	auto sym = ExitOnErr(J->lookup("integerDistances"));
	auto integerDistances = reinterpret_cast<integerDistances_t>(sym.getAddress());

	int x[]{0, 1, 2};
	int y[]{3, 1, -1};

	int *z = integerDistances(x, y, 3);
 
	outs() << format("Integer Distances: %d, %d, %d\n\n", z[0], z[1], z[2]);
	outs().flush();

	return 0;
}

