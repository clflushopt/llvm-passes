#include "llvm/ADT/DenseMap.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#define DEBUG_TYPE "mba-obfuscator"

using namespace llvm;
using namespace PatternMatch;

using NonFolderBuilder = IRBuilder<NoFolder>;

static constexpr auto ObfuscatedFunctionNamePrefix = "__mba_";

// Rules dictionary.
static constexpr auto RewriteRulesDict = {
    std::make_pair("A + B - 1", "(A ^ ~B) + 2*(A | B)"),
    std::make_pair("B", "(A | B) - (A & ~B)"),
    std::make_pair("-A - B + 1", "- (A ^ ~B) - 2*(A | B)"),
    std::make_pair("(A | B)", "A + B + 1 + (~A | ~B)"),
    std::make_pair("A ^ B", "A - B + (~(2*A) & 2*B)"),
    std::make_pair("- (A ^ B) - B", "- A -(~(2*A) & 2*B)"),
    std::make_pair("(A ^ B) - A", "-B + (~(2*A) & 2*B)"),
    std::make_pair("(A ^ B) - A", "-B + 2*(~A & B)"),
    std::make_pair("(A ^ B)", "A - B + 2*(~A & B)"),
    std::make_pair("A + B", "(A & B) + (A | B)"),
    std::make_pair("A + B", "(A ^ B) + 2*(A & B)"),
    std::make_pair("(A ^ B)", "A + B - 2*(A & B)"),
    std::make_pair("(A ^ B)", "- A - B + 2*(A | B)"),
    std::make_pair("A & B", "A + B - (A | B)"),
    std::make_pair("A + 1", "(A & B) - (~A | B)"),
    std::make_pair("A ^ B", "(A | B) - (A & B)"),
    std::make_pair("(A ^ B) - A", "-B + (2*(~A) & 2*B)"),
    std::make_pair("- (A ^ B) + A", "-2*(~A & B) + B"),
    std::make_pair("(A & B) - 1", "A + B + (~A & ~B)"),
    std::make_pair("(A ^ B) - 2", "A + B + 2*(~A | ~B)"),
    std::make_pair("(2*A & 2*B)", "((2*A + 1) & 2*B)"),
    std::make_pair("A", "(0 | A)")};

namespace {

struct RewriteRule {
  // Matcher: returns true if the instruction matches, and sets Lhs/Rhs
  std::function<bool(Instruction *, Value *&, Value *&)> Matcher;
  // Rewriter: builds the replacement instruction
  std::function<Instruction *(IRBuilder<NoFolder> &, Value *, Value *)>
      Rewriter;
  const char *Name;
};

// Helper for constants
auto GetConst = [](Value *V, int64_t C) {
  return ConstantInt::get(V->getType(), C);
};

static const RewriteRule RewriteRules[] = {
    // A + B -> (A & B) + (A | B)
    {[](Instruction *I, Value *&L, Value *&R) {
       return match(I, m_Add(m_Value(L), m_Value(R)));
     },
     [](IRBuilder<NoFolder> &B, Value *L, Value *R) {
       return BinaryOperator::CreateAdd(B.CreateAnd(L, R), B.CreateOr(L, R),
                                        "__mba_add");
     },
     "A + B -> (A & B) + (A | B)"},
    // A + B -> (A ^ B) + 2*(A & B)
    {[](Instruction *I, Value *&L, Value *&R) {
       return match(I, m_Add(m_Value(L), m_Value(R)));
     },
     [](IRBuilder<NoFolder> &B, Value *L, Value *R) {
       return BinaryOperator::CreateAdd(
           B.CreateXor(L, R), B.CreateMul(B.CreateAnd(L, R), GetConst(L, 2)),
           "__mba_add2");
     },
     "A + B -> (A ^ B) + 2*(A & B)"},
    // A - B -> (A ^ -B) + 2*(A & -B)
    {[](Instruction *I, Value *&L, Value *&R) {
       return match(I, m_Sub(m_Value(L), m_Value(R)));
     },
     [](IRBuilder<NoFolder> &B, Value *L, Value *R) {
       auto *NegR = B.CreateNeg(R);
       return BinaryOperator::CreateAdd(
           B.CreateXor(L, NegR),
           B.CreateMul(B.CreateAnd(L, NegR), GetConst(L, 2)), "__mba_sub");
     },
     "A - B -> (A ^ -B) + 2*(A & -B)"},
    // A ^ B -> (A | B) - (A & B)
    {[](Instruction *I, Value *&L, Value *&R) {
       return match(I, m_Xor(m_Value(L), m_Value(R)));
     },
     [](IRBuilder<NoFolder> &B, Value *L, Value *R) {
       return BinaryOperator::CreateSub(B.CreateOr(L, R), B.CreateAnd(L, R),
                                        "__mba_xor");
     },
     "A ^ B -> (A | B) - (A & B)"},
    // A & B -> A + B - (A | B)
    {[](Instruction *I, Value *&L, Value *&R) {
       return match(I, m_And(m_Value(L), m_Value(R)));
     },
     [](IRBuilder<NoFolder> &B, Value *L, Value *R) {
       return BinaryOperator::CreateSub(B.CreateAdd(L, R), B.CreateOr(L, R),
                                        "__mba_and");
     },
     "A & B -> A + B - (A | B)"},
};

struct ArithmeticVisitor
    : public InstVisitor<ArithmeticVisitor, Instruction *> {
  DenseMap<Value *, Value *> Replacements;

  // Create a builder with no folding behavior since we don't want to fold
  // constants during the obfuscation process.
  using BuilderTy = IRBuilder<NoFolder>;

  BuilderTy &Builder;

  ArithmeticVisitor(BuilderTy &B) : Builder(B) {}

  Instruction *visitInstruction(Instruction &I) {
    for (const auto &Rule : RewriteRules) {
      Value *L = nullptr, *R = nullptr;
      if (Rule.Matcher(&I, L, R)) {
        return Rule.Rewriter(Builder, L, R);
      }
    }
    return nullptr;
  }
};

struct MBAObfuscator : public PassInfoMixin<MBAObfuscator> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

    bool Changed = false;
    for (auto &BB : F) {
      // Check if the basic block has changed.
      Changed |= runOnBasicBlock(BB);
    }

    return (Changed ? PreservedAnalyses::none() : PreservedAnalyses::all());
  };

  bool runOnBasicBlock(BasicBlock &BB) {
    size_t ReplacedCount = 0;
    size_t FixedPointBound = 10;
    SmallVector<Instruction *, 8> ToRemove;

    for (size_t I = 0; I < FixedPointBound; ++I) {
      for (auto Inst = BB.begin(), IE = BB.end(); Inst != IE; ++Inst) {
        // Skip non-binary (e.g. unary or compare) instructions
        auto *BinOp = dyn_cast<BinaryOperator>(Inst);
        if (!BinOp)
          continue;

        IRBuilder<NoFolder> Builder(BinOp);
        ArithmeticVisitor Visitor(Builder);

        if (auto *NewInst = Visitor.visit(*Inst)) {
          ReplaceInstWithInst(&BB, Inst, NewInst);
          ReplacedCount++;
        }
      }
    }
    return ReplacedCount > 0;
  };

  static bool isRequired() { return true; }

private:
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {.APIVersion = LLVM_PLUGIN_API_VERSION,
          .PluginName = "MBA Obfuscator pass",
          .PluginVersion = "v0.1",
          .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback([](ModulePassManager &MPM,
                                                  OptimizationLevel Level) {
              FunctionPassManager FPM;
              FPM.addPass(MBAObfuscator());
              MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
            });
          }};
}
