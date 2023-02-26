#ifndef CLANG_DRIVER_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>

#include <memory>
#include <string>

class ClangDriver
{
public :

	ClangDriver() = default;

	llvm::Expected<std::unique_ptr<llvm::Module>>
	compileTranslationUnit(std::string cppCode, llvm::LLVMContext &context);
};

#endif // CLANG_DRIVER_H

