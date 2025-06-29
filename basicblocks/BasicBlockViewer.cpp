#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace {

// BasicBlockViewerPass is a simple LLVM pass that prints the names of
// all basic blocks in each function of the module and their corresponding
// predecessors and successors.
struct BasicBlockViewerPass : public llvm::PassInfoMixin<BasicBlockViewerPass> {
  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &AM) {
    for (auto &F : M) {
      for (auto &BB : F) {
        // Filter out C++ standard library functions.
        auto DemangledName = llvm::demangle(F.getName());

        llvm::outs() << "Function: " << DemangledName << "\n";
        if (BB.hasName()) {
          llvm::outs() << "  Name: " << BB.getName() << "\n";
        } else {
          llvm::outs() << "  Name: <unnamed>\n";
        }

        if (BB.hasNPredecessorsOrMore(1)) {
          llvm::outs() << "  Predecessors: ";
          for (auto *Pred : llvm::predecessors(&BB)) {
            llvm::outs() << ((Pred->getName() != "") ? Pred->getName()
                                                     : "<unnamed>")
                         << " ";
          }
        }

        llvm::outs() << "\n";
      }
    }
    return llvm::PreservedAnalyses::all();
  }
};
} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {
      .APIVersion = LLVM_PLUGIN_API_VERSION,
      .PluginName = "BasicBlockViewerPass",
      .PluginVersion = "v0.1",
      .RegisterPassBuilderCallbacks = [](llvm::PassBuilder &PB) {
        PB.registerPipelineStartEPCallback(
            [](llvm::ModulePassManager &MPM, llvm::OptimizationLevel Level) {
              MPM.addPass(BasicBlockViewerPass());
            });
      }};
}