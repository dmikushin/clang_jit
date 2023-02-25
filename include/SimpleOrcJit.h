#pragma once

#include "ClangCC1Driver.h"

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#include <llvm/ExecutionEngine/Orc/OrcError.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/RuntimeDyld.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Mangler.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/DynamicLibrary.h>

#include <functional>
#include <memory>
#include <vector>

#define DEBUG_TYPE "jitfromscratch"

#if _WIN32
#define DECL_JIT_ACCESS_CPP __declspec(dllexport)
#else
#define DECL_JIT_ACCESS_CPP
#endif

class SimpleOrcJit {
  struct NotifyObjectLoaded_t {
    NotifyObjectLoaded_t(SimpleOrcJit &jit) : Jit(jit) {}

    // Called by the ObjectLayer for each emitted object.
    // Forward notification to GDB JIT interface.
    void
    operator()(llvm::orc::RTDyldObjectLinkingLayerBase::ObjHandleT,
               const llvm::orc::RTDyldObjectLinkingLayerBase::ObjectPtr &obj,
               const llvm::LoadedObjectInfo &info) {

      // Workaround 5.0 API inconsistency:
      // http://lists.llvm.org/pipermail/llvm-dev/2017-August/116806.html
      const auto &fixedInfo =
          static_cast<const llvm::RuntimeDyld::LoadedObjectInfo &>(info);

      Jit.GdbEventListener->NotifyObjectEmitted(*obj->getBinary(), fixedInfo);
    }

  private:
    SimpleOrcJit &Jit;
  };

  using ModulePtr_t = std::unique_ptr<llvm::Module>;
  using IRCompiler_t = llvm::orc::SimpleCompiler;

  using ObjectLayer_t = llvm::orc::RTDyldObjectLinkingLayer;
  using CompileLayer_t = llvm::orc::IRCompileLayer<ObjectLayer_t, IRCompiler_t>;

public:
  SimpleOrcJit(llvm::TargetMachine &targetMachine)
      : DL(targetMachine.createDataLayout()),
        MemoryManagerPtr(std::make_shared<llvm::SectionMemoryManager>()),
        SymbolResolverPtr(llvm::orc::createLambdaResolver(
            [&](std::string name) { return findSymbolInJITedCode(name); },
            [&](std::string name) { return findSymbolInHostProcess(name); })),
        NotifyObjectLoaded(*this),
        ObjectLayer([this]() { return MemoryManagerPtr; }, NotifyObjectLoaded),
        CompileLayer(ObjectLayer, IRCompiler_t(targetMachine)) {
    // Load own executable as dynamic library.
    // Required for RTDyldMemoryManager::getSymbolAddressInProcess().
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

    // Internally points to a llvm::ManagedStatic.
    // No need to free. "create" is a misleading term here.
    GdbEventListener = llvm::JITEventListener::createGDBRegistrationListener();
  }

  void submitModule(ModulePtr_t module) {
    DEBUG({
      llvm::dbgs() << "Submit LLVM module:\n\n";
      llvm::dbgs() << *module.get() << "\n\n";
    });

    // Commit module for compilation to machine code. Actual compilation
    // happens on demand as soon as one of it's symbols is accessed. None of
    // the layers used here issue Errors from this call.
    llvm::cantFail(
        CompileLayer.addModule(std::move(module), SymbolResolverPtr));
  }

  llvm::Expected<std::unique_ptr<llvm::Module>>
  compileModuleFromCpp(std::string cppCode, llvm::LLVMContext &context) {
    return ClangDriver.compileTranslationUnit(cppCode, context);
  }

  template <class Signature_t>
  llvm::Expected<std::function<Signature_t>> getFunction(std::string name) {
    using namespace llvm;

    // Find symbol name in committed modules.
    std::string mangledName = mangle(std::move(name));
    JITSymbol sym = findSymbolInJITedCode(mangledName);
    if (!sym)
      return make_error<orc::JITSymbolNotFound>(mangledName);

    // Access symbol address.
    // Invokes compilation for the respective module if not compiled yet.
    Expected<JITTargetAddress> addr = sym.getAddress();
    if (!addr)
      return addr.takeError();

    auto typedFunctionPtr = reinterpret_cast<Signature_t *>(*addr);
    return std::function<Signature_t>(typedFunctionPtr);
  }

private:
  llvm::DataLayout DL;
  ClangCC1Driver ClangDriver;
  std::shared_ptr<llvm::RTDyldMemoryManager> MemoryManagerPtr;
  std::shared_ptr<llvm::JITSymbolResolver> SymbolResolverPtr;
  NotifyObjectLoaded_t NotifyObjectLoaded;
  llvm::JITEventListener *GdbEventListener;

  ObjectLayer_t ObjectLayer;
  CompileLayer_t CompileLayer;

  llvm::JITSymbol findSymbolInJITedCode(std::string mangledName) {
    constexpr bool exportedSymbolsOnly = false;
    return CompileLayer.findSymbol(mangledName, exportedSymbolsOnly);
  }

  llvm::JITSymbol findSymbolInHostProcess(std::string mangledName) {
    // Lookup function address in the host symbol table.
    if (llvm::JITTargetAddress addr =
            llvm::RTDyldMemoryManager::getSymbolAddressInProcess(mangledName))
      return llvm::JITSymbol(addr, llvm::JITSymbolFlags::Exported);

    return nullptr;
  }

  // System name mangler: may prepend '_' on OSX or '\x1' on Windows
  std::string mangle(std::string name) {
    std::string buffer;
    llvm::raw_string_ostream ostream(buffer);
    llvm::Mangler::getNameWithPrefix(ostream, std::move(name), DL);
    return ostream.str();
  }
};
