// По мотивам статей
// https://blog.audio-tk.com/2018/09/18/compiling-c-code-in-memory-with-clang/
// https://wiki.nervtech.org/doku.php?id=blog:2020:0410_dynamic_cpp_compilation
// https://stackoverflow.com/questions/34828480/generate-assembly-from-c-code-in-memory-using-libclang

#include <sstream>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>

#include "llvm_precomp.h"


//#define NV_LLVM_VERBOSE 1

bool LLVMinit = false;

#define ERROR_MSG(msg) std::cout << "[ERROR]: "<<msg<< std::endl;
#define DEBUG_MSG(msg) std::cout << "[DEBUG]: "<<msg<< std::endl;

void InitializeLLVM() {
    if(LLVMinit) {
        return;
    }

    // We have not initialized any pass managers for any device yet.
    // Run the global LLVM pass initialization functions.
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    auto& Registry = *llvm::PassRegistry::getPassRegistry();

    llvm::initializeCore(Registry);
    llvm::initializeScalarOpts(Registry);
    llvm::initializeVectorization(Registry);
    llvm::initializeIPO(Registry);
    llvm::initializeAnalysis(Registry);
    llvm::initializeTransformUtils(Registry);
    llvm::initializeInstCombine(Registry);
    llvm::initializeInstrumentation(Registry);
    llvm::initializeTarget(Registry);


    LLVMinit = true;
}

int main(int argc, char *argv[]) {

    InitializeLLVM();

    const char * func_text = \
"int nv_add(int a, int b) {\n\
    printf(\"call nv_add(%d, %d)\\n\", a, b);\n\
    return a + b;\n\
}\n\
\n\
int nv_sub(int a, int b) {\n\
    printf(\"call nv_sub(%d, %d)\\n\", a, b);\n\
    return a - b;\n\
}\n\
";


    DEBUG_MSG("Running clang compilation...");


    clang::CompilerInstance compilerInstance;
    auto& compilerInvocation = compilerInstance.getInvocation();


    // Диагностика работы Clang
    clang::IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts = new clang::DiagnosticOptions;
    clang::TextDiagnosticPrinter *textDiagPrinter =
            new clang::TextDiagnosticPrinter(llvm::outs(), &*DiagOpts);

    clang::IntrusiveRefCntPtr<clang::DiagnosticIDs> pDiagIDs;

    clang::DiagnosticsEngine *pDiagnosticsEngine =
            new clang::DiagnosticsEngine(pDiagIDs, &*DiagOpts, textDiagPrinter);



    // Целевая платформа
    std::stringstream ss;
    ss << "-triple=" << llvm::sys::getDefaultTargetTriple();
    
    std::cout << llvm::sys::getDefaultTargetTriple();
    
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

    // Компиляция из памяти
    // Send code through a pipe to stdin
    int codeInPipe[2];
    pipe2(codeInPipe, O_NONBLOCK);
    write(codeInPipe[1], (void *) func_text, strlen(func_text));
    close(codeInPipe[1]); // We need to close the pipe to send an EOF
    dup2(codeInPipe[0], STDIN_FILENO);

    itemcstrs.push_back("-"); // Read code from stdin

    clang::CompilerInvocation::CreateFromArgs(compilerInvocation, llvm::ArrayRef<const char *>(itemcstrs.data(), itemcstrs.size()), *pDiagnosticsEngine);

    auto* languageOptions = compilerInvocation.getLangOpts();
    auto& preprocessorOptions = compilerInvocation.getPreprocessorOpts();
    auto& targetOptions = compilerInvocation.getTargetOpts();
    auto& frontEndOptions = compilerInvocation.getFrontendOpts();
#ifdef NV_LLVM_VERBOSE
    frontEndOptions.ShowStats = true;
#endif
    auto& headerSearchOptions = compilerInvocation.getHeaderSearchOpts();
#ifdef NV_LLVM_VERBOSE
    headerSearchOptions.Verbose = true;
#endif
    auto& codeGenOptions = compilerInvocation.getCodeGenOpts();


    targetOptions.Triple = llvm::sys::getDefaultTargetTriple();
    compilerInstance.createDiagnostics(textDiagPrinter, false);

    llvm::LLVMContext context;
    std::unique_ptr<clang::CodeGenAction> action = std::make_unique<clang::EmitLLVMOnlyAction>(&context);

    if(!compilerInstance.ExecuteAction(*action)) {
        ERROR_MSG("Cannot execute action with compiler instance.");
    }

    // Runtime LLVM Module
    std::unique_ptr<llvm::Module> module = action->takeModule();
    if(!module) {
        ERROR_MSG("Cannot retrieve IR module.");
    }

    // Оптимизация IR
    llvm::PassBuilder passBuilder;
    llvm::LoopAnalysisManager loopAnalysisManager(codeGenOptions.DebugPassManager);
    llvm::FunctionAnalysisManager functionAnalysisManager(codeGenOptions.DebugPassManager);
    llvm::CGSCCAnalysisManager cGSCCAnalysisManager(codeGenOptions.DebugPassManager);
    llvm::ModuleAnalysisManager moduleAnalysisManager(codeGenOptions.DebugPassManager);

    passBuilder.registerModuleAnalyses(moduleAnalysisManager);
    passBuilder.registerCGSCCAnalyses(cGSCCAnalysisManager);
    passBuilder.registerFunctionAnalyses(functionAnalysisManager);
    passBuilder.registerLoopAnalyses(loopAnalysisManager);
    passBuilder.crossRegisterProxies(loopAnalysisManager, functionAnalysisManager, cGSCCAnalysisManager, moduleAnalysisManager);

    llvm::ModulePassManager modulePassManager = passBuilder.buildPerModuleDefaultPipeline(llvm::PassBuilder::OptimizationLevel::O3);
    modulePassManager.run(*module, moduleAnalysisManager);

    llvm::EngineBuilder builder(std::move(module));
    builder.setMCJITMemoryManager(std::make_unique<llvm::SectionMemoryManager>());
    builder.setOptLevel(llvm::CodeGenOpt::Level::Aggressive);

    std::string createErrorMsg;

    builder.setEngineKind(llvm::EngineKind::JIT);
    builder.setVerifyModules(true);
    builder.setErrorStr(&createErrorMsg);

    std::string triple = llvm::sys::getDefaultTargetTriple();
    DEBUG_MSG("Using target triple: " << triple);
    auto executionEngine = builder.create();

    if(!executionEngine) {
        ERROR_MSG("Cannot create execution engine.'" << createErrorMsg << "'");
    }

    DEBUG_MSG("Retrieving nv_add/nv_sub functions...");
    typedef int(*AddFunc)(int, int);
    typedef int(*SubFunc)(int, int);

    AddFunc add = reinterpret_cast<AddFunc> (executionEngine->getFunctionAddress("nv_add"));
    if(!add) {
        ERROR_MSG("Cannot retrieve Add function.");
    } else {
        int res = add(40, 2);
        DEBUG_MSG("The meaning of life is: " << res << "!");
    }

    SubFunc sub = reinterpret_cast<SubFunc> (executionEngine->getFunctionAddress("nv_sub"));
    if(!sub) {
        ERROR_MSG("Cannot retrieve Sub function.");
    } else {
        int res = sub(50, 8);
        DEBUG_MSG("The meaning of life is really: " << res << "!");
    }

    DEBUG_MSG("Done running clang compilation.");

    return 0;
}
