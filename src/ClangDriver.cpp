#include "ClangDriver.h"

// https://habr.com/ru/post/572878/
// https://blog.audio-tk.com/2018/09/18/compiling-c-code-in-memory-with-clang/
// https://wiki.nervtech.org/doku.php?id=blog:2020:0410_dynamic_cpp_compilation
// https://stackoverflow.com/questions/34828480/generate-assembly-from-c-code-in-memory-using-libclang

#include <llvm/InitializePasses.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>

#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"

#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/FileSystemOptions.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Sema/Sema.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTConsumer.h>

#include <sstream>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>

#define VERBOSE

llvm::Expected<std::unique_ptr<llvm::Module>>
ClangDriver::compileTranslationUnit(std::string cppCode, llvm::LLVMContext &context)
{
    clang::CompilerInstance compilerInstance;
    auto& compilerInvocation = compilerInstance.getInvocation();

    clang::IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts = new clang::DiagnosticOptions;
    clang::TextDiagnosticPrinter *textDiagPrinter =
            new clang::TextDiagnosticPrinter(llvm::outs(), &*DiagOpts);

    clang::IntrusiveRefCntPtr<clang::DiagnosticIDs> pDiagIDs;

    clang::DiagnosticsEngine *pDiagnosticsEngine =
            new clang::DiagnosticsEngine(pDiagIDs, &*DiagOpts, textDiagPrinter);

    std::stringstream ss;
    ss << "-triple=" << llvm::sys::getDefaultTargetTriple();
    
    std::istream_iterator<std::string> begin(ss);
    std::istream_iterator<std::string> end;
    std::istream_iterator<std::string> i = begin;
    std::vector<const char*> itemcstrs;
    std::vector<std::string> itemstrs;
    while(i != end) {
        itemstrs.push_back(*i);
        ++i;
    }

    for (unsigned idx = 0; idx < itemstrs.size(); idx++) {
        // note: if itemstrs is modified after this, itemcstrs will be full
        // of invalid pointers! Could make copies, but would have to clean up then...
        itemcstrs.push_back(itemstrs[idx].c_str());
    }

    // Send code through a pipe to stdin
    int codeInPipe[2];
    pipe2(codeInPipe, O_NONBLOCK);
    write(codeInPipe[1], (void *) cppCode.data(), cppCode.length());
    close(codeInPipe[1]); // We need to close the pipe to send an EOF
    dup2(codeInPipe[0], STDIN_FILENO);

    itemcstrs.push_back("-"); // Read code from stdin

    clang::CompilerInvocation::CreateFromArgs(compilerInvocation, llvm::ArrayRef<const char *>(itemcstrs.data(), itemcstrs.size()), *pDiagnosticsEngine);

    auto* langOpts = compilerInvocation.getLangOpts();
    langOpts->CPlusPlus = 1;
    auto& preprocessorOptions = compilerInvocation.getPreprocessorOpts();
    auto& targetOptions = compilerInvocation.getTargetOpts();
    auto& frontEndOptions = compilerInvocation.getFrontendOpts();
#ifdef VERBOSE
    frontEndOptions.ShowStats = true;
#endif
    auto& headerSearchOptions = compilerInvocation.getHeaderSearchOpts();
#ifdef VERBOSE
    headerSearchOptions.Verbose = true;
#endif
    auto& codeGenOptions = compilerInvocation.getCodeGenOpts();

    targetOptions.Triple = llvm::sys::getDefaultTargetTriple();
    compilerInstance.createDiagnostics(textDiagPrinter, false);

    std::unique_ptr<clang::CodeGenAction> action = std::make_unique<clang::EmitLLVMOnlyAction>(&context);

    // TODO Turn false status into llvm::Error
    /*llvm::Error err =*/ compilerInstance.ExecuteAction(*action);
    /*if (err)
        return std::move(err);*/

    // Delete output files to free Compiler Instance
    compilerInstance.clearOutputFiles(/*EraseFiles=*/false);

    return action->takeModule();
}

