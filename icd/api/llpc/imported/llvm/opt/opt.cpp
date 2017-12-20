//===- opt.cpp - The LLVM Modular Optimizer -------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Optimizations may be specified an arbitrary number of times on the command
// line, They are run in the order specified.
//
//===----------------------------------------------------------------------===//

#include "NewPMDriver.h"
#include "PassPrinters.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/RegionPass.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Bitcode/BitcodeWriterPass.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LegacyPassNameParser.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/LinkAllIR.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/Coroutines.h"
#include "llvm/Transforms/IPO/AlwaysInliner.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <algorithm>
#include <memory>
#include "llpcDebug.h"
using namespace Llpc;
using namespace llvm;
using namespace opt_tool;

// The OptimizationList is automatically populated with registered Passes by the
// PassNameParser.
//
static cl::list<const PassInfo*, bool, PassNameParser>
PassList(cl::desc("Optimizations available:"));

// This flag specifies a textual description of the optimization pass pipeline
// to run over the module. This flag switches opt to use the new pass manager
// infrastructure, completely disabling all of the flags specific to the old
// pass management.
static cl::opt<std::string> PassPipeline(
    "passes",
    cl::desc("A textual description of the pass pipeline for optimizing"),
    cl::Hidden);

// Other command line options...
//

static cl::opt<bool>
PrintEachXForm("p", cl::desc("Print module after each transformation"));

static cl::opt<bool>
NoOutput("disable-output",
         cl::desc("Do not write result bitcode file"), cl::Hidden, cl::init(true));

static cl::opt<bool>
OutputAssembly("S", cl::desc("Write output as LLVM assembly"));

static cl::opt<bool>
    OutputThinLTOBC("thinlto-bc",
                    cl::desc("Write output as ThinLTO-ready bitcode"));

static cl::opt<bool>
NoVerify("disable-verify", cl::desc("Do not run the verifier"), cl::Hidden);

static cl::opt<bool>
VerifyEach("verify-each", cl::desc("Verify after each transform"));

static cl::opt<bool>
    DisableDITypeMap("disable-debug-info-type-map",
                     cl::desc("Don't use a uniquing type map for debug info"));

static cl::opt<bool>
StripDebug("strip-debug",
           cl::desc("Strip debugger symbol info from translation unit"));

static cl::opt<bool>
DisableInline("disable-inlining", cl::desc("Do not run the inliner pass"));

static cl::opt<bool>
DisableOptimizations("disable-opt",
                     cl::desc("Do not run any optimization passes"));

static cl::opt<bool>
StandardLinkOpts("std-link-opts",
                 cl::desc("Include the standard link time optimizations"));

static cl::opt<bool>
OptLevelO0("O0",
  cl::desc("Optimization level 0. Similar to clang -O0"));

static cl::opt<bool>
OptLevelO1("O1",
           cl::desc("Optimization level 1. Similar to clang -O1"));

static cl::opt<bool>
OptLevelO2("O2",
           cl::desc("Optimization level 2. Similar to clang -O2"));

static cl::opt<bool>
OptLevelOs("Os",
           cl::desc("Like -O2 with extra optimizations for size. Similar to clang -Os"));

static cl::opt<bool>
OptLevelOz("Oz",
           cl::desc("Like -Os but reduces code size further. Similar to clang -Oz"));

static cl::opt<bool>
OptLevelO3("O3",
           cl::desc("Optimization level 3. Similar to clang -O3"), cl::init(true));

static cl::opt<unsigned>
CodeGenOptLevel("codegen-opt-level",
                cl::desc("Override optimization level for codegen hooks"));

static cl::opt<bool>
UnitAtATime("funit-at-a-time",
            cl::desc("Enable IPO. This corresponds to gcc's -funit-at-a-time"),
            cl::init(true));

static cl::opt<bool>
DisableLoopUnrolling("disable-loop-unrolling",
                     cl::desc("Disable loop unrolling in all relevant passes"),
                     cl::init(false));
static cl::opt<bool>
DisableLoopVectorization("disable-loop-vectorization",
                     cl::desc("Disable the loop vectorization pass"),
                     cl::init(false));

static cl::opt<bool>
DisableSLPVectorization("disable-slp-vectorization",
                        cl::desc("Disable the slp vectorization pass"),
                        cl::init(false));

static cl::opt<bool> EmitSummaryIndex("module-summary",
                                      cl::desc("Emit module summary index"),
                                      cl::init(false));

static cl::opt<bool> EmitModuleHash("module-hash", cl::desc("Emit module hash"),
                                    cl::init(false));

static cl::opt<bool>
DisableSimplifyLibCalls("disable-simplify-libcalls",
                        cl::desc("Disable simplify-libcalls"));

static cl::opt<bool>
Quiet("q", cl::desc("Obsolete option"), cl::Hidden);

static cl::alias
QuietA("quiet", cl::desc("Alias for -q"), cl::aliasopt(Quiet));

static cl::opt<bool>
AnalyzeOnly("analyze", cl::desc("Only perform analysis, no optimization"));


static cl::opt<bool> PreserveBitcodeUseListOrder(
    "preserve-bc-uselistorder",
    cl::desc("Preserve use-list order when writing LLVM bitcode."),
    cl::init(true), cl::Hidden);

static cl::opt<bool> PreserveAssemblyUseListOrder(
    "preserve-ll-uselistorder",
    cl::desc("Preserve use-list order when writing LLVM assembly."),
    cl::init(false), cl::Hidden);

static cl::opt<bool> DiscardValueNames(
    "discard-value-names",
    cl::desc("Discard names from Value (other than GlobalValue)."),
    cl::init(false), cl::Hidden);

static cl::opt<bool> Coroutines(
  "enable-coroutines",
  cl::desc("Enable coroutine passes."),
  cl::init(false), cl::Hidden);

static cl::opt<bool> PassRemarksWithHotness(
    "pass-remarks-with-hotness",
    cl::desc("With PGO, include profile count in optimization remarks"),
    cl::Hidden);

static cl::opt<std::string>
    RemarksFilename("pass-remarks-output",
                    cl::desc("YAML output filename for pass remarks"),
                    cl::value_desc("filename"));

static inline void addPass(legacy::PassManagerBase &PM, Pass *P) {
  // Add the pass to the pass manager...
  PM.add(P);

  // If we are verifying all of the intermediate steps, add the verifier...
  if (VerifyEach)
    PM.add(createVerifierPass());
}

/// This routine adds optimization passes based on selected optimization level,
/// OptLevel.
///
/// OptLevel - Optimization Level
static void AddOptimizationPasses(legacy::PassManagerBase &MPM,
                                  legacy::FunctionPassManager &FPM,
                                  TargetMachine *TM, unsigned OptLevel,
                                  unsigned SizeLevel) {
  if (!NoVerify || VerifyEach)
    FPM.add(createVerifierPass()); // Verify that input is correct

  PassManagerBuilder Builder;
  Builder.OptLevel = OptLevel;
  Builder.SizeLevel = SizeLevel;

  if (DisableInline) {
    // No inlining pass
  } else if (OptLevel > 1) {
    Builder.Inliner = createFunctionInliningPass(OptLevel, SizeLevel, false);
  } else {
    Builder.Inliner = createAlwaysInlinerLegacyPass();
  }
  Builder.DisableUnitAtATime = !UnitAtATime;
  Builder.DisableUnrollLoops = (DisableLoopUnrolling.getNumOccurrences() > 0) ?
                               DisableLoopUnrolling : OptLevel == 0;

  // This is final, unless there is a #pragma vectorize enable
  if (DisableLoopVectorization)
    Builder.LoopVectorize = false;
  // If option wasn't forced via cmd line (-vectorize-loops, -loop-vectorize)
  else if (!Builder.LoopVectorize)
    Builder.LoopVectorize = OptLevel > 1 && SizeLevel < 2;

  // When #pragma vectorize is on for SLP, do the same as above
  Builder.SLPVectorize =
      DisableSLPVectorization ? false : OptLevel > 1 && SizeLevel < 2;

  if (TM)
    TM->adjustPassManager(Builder);

  if (Coroutines)
    addCoroutinePassesToExtensionPoints(Builder);

  Builder.populateFunctionPassManager(FPM);
  Builder.populateModulePassManager(MPM);
}

static void AddStandardLinkPasses(legacy::PassManagerBase &PM) {
  PassManagerBuilder Builder;
  Builder.VerifyInput = true;
  if (DisableOptimizations)
    Builder.OptLevel = 0;

  if (!DisableInline)
    Builder.Inliner = createFunctionInliningPass();
  Builder.populateLTOPassManager(PM);
}

//===----------------------------------------------------------------------===//
// CodeGen-related helper functions.
//

static CodeGenOpt::Level GetCodeGenOptLevel() {
  if (CodeGenOptLevel.getNumOccurrences())
    return static_cast<CodeGenOpt::Level>(unsigned(CodeGenOptLevel));
  if (OptLevelO1)
    return CodeGenOpt::Less;
  if (OptLevelO2)
    return CodeGenOpt::Default;
  if (OptLevelO3)
    return CodeGenOpt::Aggressive;
  return CodeGenOpt::None;
}

// Returns the TargetMachine instance or zero if no triple is provided.
static TargetMachine* GetTargetMachine(Triple TheTriple, StringRef CPUStr,
                                       StringRef FeaturesStr,
                                       const TargetOptions &Options) {
  std::string Error;
  const Target *TheTarget = TargetRegistry::lookupTarget(MArch, TheTriple,
                                                         Error);
  // Some modules don't specify a triple, and this is okay.
  if (!TheTarget) {
    return nullptr;
  }

  return TheTarget->createTargetMachine(TheTriple.getTriple(), CPUStr,
                                        FeaturesStr, Options, getRelocModel(),
                                        getCodeModel(), GetCodeGenOptLevel());
}

#ifdef LINK_POLLY_INTO_TOOLS
namespace polly {
void initializePollyPasses(llvm::PassRegistry &Registry);
}
#endif

//===----------------------------------------------------------------------===//
// Initialize optimizer
void InitOptimizer()
{
  // Initialize passes
  PassRegistry &Registry = *PassRegistry::getPassRegistry();
  initializeCore(Registry);
  initializeCoroutines(Registry);
  initializeScalarOpts(Registry);
  initializeObjCARCOpts(Registry);
  initializeVectorization(Registry);
  initializeIPO(Registry);
  initializeAnalysis(Registry);
  initializeTransformUtils(Registry);
  initializeInstCombine(Registry);
  initializeInstrumentation(Registry);
  initializeTarget(Registry);
  // For codegen passes, only passes that do IR to IR transformation are
  // supported.
  initializeCodeGenPreparePass(Registry);
  initializeAtomicExpandPass(Registry);
  initializeRewriteSymbolsLegacyPassPass(Registry);
  initializeWinEHPreparePass(Registry);
  initializeDwarfEHPreparePass(Registry);
  initializeSafeStackLegacyPassPass(Registry);
  initializeSjLjEHPreparePass(Registry);
  initializePreISelIntrinsicLoweringLegacyPassPass(Registry);
  initializeGlobalMergePass(Registry);
  initializeInterleavedAccessPass(Registry);
#ifdef LLVM_SOURCE_PROMOTION
  initializeExpandMemCmpPassPass(Registry);
  initializeEntryExitInstrumenterPass(Registry);
  initializePostInlineEntryExitInstrumenterPass(Registry);
  initializeWriteBitcodePassPass(Registry);
  initializeExpandReductionsPass(Registry);
#else
  initializeCountingFunctionInserterPass(Registry);
#endif
  initializeUnreachableBlockElimLegacyPassPass(Registry);

#ifdef LINK_POLLY_INTO_TOOLS
  polly::initializePollyPasses(Registry);
#endif
}

//===----------------------------------------------------------------------===//
// Do optimization for input module
bool OptimizeModule(Module* M)
{
  // Enable debug stream buffering.
  EnableDebugBuffering = true;

  SMDiagnostic Err;
  LLVMContext& Context = M->getContext();

  Context.setDiscardValueNames(DiscardValueNames);
  if (!DisableDITypeMap)
    Context.enableDebugTypeODRUniquing();

  if (PassRemarksWithHotness)
  {
      Context.setDiagnosticsHotnessRequested(true);
  }

  std::unique_ptr<ToolOutputFile> YamlFile;

  if (RemarksFilename != "") {
    std::error_code EC;
    YamlFile = llvm::make_unique<ToolOutputFile>(RemarksFilename, EC,
        sys::fs::F_None);
    if (EC) {
      errs() << EC.message() << '\n';
      return false;
    }
    Context.setDiagnosticsOutputFile(
        llvm::make_unique<yaml::Output>(YamlFile->os()));
  }

  // Strip debug info before running the verifier.
  if (StripDebug)
    StripDebugInfo(*M);

  // Immediately run the verifier to catch any problems before starting up the
  // pass pipelines.  Otherwise we can crash on broken code during
  // doInitialization().
  std::string errMsg;
  raw_string_ostream errStream(errMsg);
  if (!NoVerify && verifyModule(*M, &errStream)) {
    LLPC_ERRS("Optimization: Input module is broken!\n" << errStream.str() << "\n");
    return false;
  }

  Triple ModuleTriple(M->getTargetTriple());
  std::string CPUStr, FeaturesStr;
  TargetMachine *Machine = nullptr;
  const TargetOptions Options = InitTargetOptionsFromCodeGenFlags();

  if (ModuleTriple.getArch()) {
    CPUStr = getCPUStr();
    FeaturesStr = getFeaturesStr();
    Machine = GetTargetMachine(ModuleTriple, CPUStr, FeaturesStr, Options);
  }

  std::unique_ptr<TargetMachine> TM(Machine);

  // Override function attributes based on CPUStr, FeaturesStr, and command line
  // flags.
  setFunctionAttributes(CPUStr, FeaturesStr, *M);

  if (PassPipeline.getNumOccurrences() > 0) {
    OutputKind OK = OK_NoOutput;
    if (!NoOutput)
      OK = OutputAssembly ? OK_OutputAssembly : OK_OutputBitcode;

    VerifierKind VK = VK_VerifyInAndOut;
    if (NoVerify)
      VK = VK_NoVerifier;
    else if (VerifyEach)
      VK = VK_VerifyEachPass;

    // The user has asked to use the new pass manager and provided a pipeline
    // string. Hand off the rest of the functionality to the new code for that
    // layer.
    return runPassPipeline("LLPC", *M, TM.get(),
                           PassPipeline, OK, VK, PreserveAssemblyUseListOrder,
                           PreserveBitcodeUseListOrder, EmitSummaryIndex,
                           EmitModuleHash);
  }

  // Create a PassManager to hold and optimize the collection of passes we are
  // about to build.
  //
  legacy::PassManager Passes;

  // Add an appropriate TargetLibraryInfo pass for the module's triple.
  TargetLibraryInfoImpl TLII(ModuleTriple);

  // The -disable-simplify-libcalls flag actually disables all builtin optzns.
  if (DisableSimplifyLibCalls)
    TLII.disableAllFunctions();
  Passes.add(new TargetLibraryInfoWrapperPass(TLII));

  // Add internal analysis passes from the target machine.
  Passes.add(createTargetTransformInfoWrapperPass(TM ? TM->getTargetIRAnalysis()
                                                     : TargetIRAnalysis()));

  std::unique_ptr<legacy::FunctionPassManager> FPasses;
  if (OptLevelO0 || OptLevelO1 || OptLevelO2 || OptLevelOs || OptLevelOz ||
      OptLevelO3) {
    FPasses.reset(new legacy::FunctionPassManager(M));
    FPasses->add(createTargetTransformInfoWrapperPass(
        TM ? TM->getTargetIRAnalysis() : TargetIRAnalysis()));
  }

  if (TM) {
    auto &LTM = static_cast<LLVMTargetMachine &>(*TM);
    Pass *TPC = LTM.createPassConfig(Passes);
    Passes.add(TPC);
  }

  // Create a new optimization pass for each one specified on the command line
  for (unsigned i = 0; i < PassList.size(); ++i) {
    if (StandardLinkOpts &&
        StandardLinkOpts.getPosition() < PassList.getPosition(i)) {
      AddStandardLinkPasses(Passes);
      StandardLinkOpts = false;
    }

    if (OptLevelO0 && OptLevelO0.getPosition() < PassList.getPosition(i)) {
      AddOptimizationPasses(Passes, *FPasses, TM.get(), 0, 0);
      OptLevelO0 = false;
    }

    if (OptLevelO1 && OptLevelO1.getPosition() < PassList.getPosition(i)) {
      AddOptimizationPasses(Passes, *FPasses, TM.get(), 1, 0);
      OptLevelO1 = false;
    }

    if (OptLevelO2 && OptLevelO2.getPosition() < PassList.getPosition(i)) {
      AddOptimizationPasses(Passes, *FPasses, TM.get(), 2, 0);
      OptLevelO2 = false;
    }

    if (OptLevelOs && OptLevelOs.getPosition() < PassList.getPosition(i)) {
      AddOptimizationPasses(Passes, *FPasses, TM.get(), 2, 1);
      OptLevelOs = false;
    }

    if (OptLevelOz && OptLevelOz.getPosition() < PassList.getPosition(i)) {
      AddOptimizationPasses(Passes, *FPasses, TM.get(), 2, 2);
      OptLevelOz = false;
    }

    if (OptLevelO3 && OptLevelO3.getPosition() < PassList.getPosition(i)) {
      AddOptimizationPasses(Passes, *FPasses, TM.get(), 3, 0);
      OptLevelO3 = false;
    }

    const PassInfo *PassInf = PassList[i];
    Pass *P = nullptr;
    if (PassInf->getNormalCtor())
      P = PassInf->getNormalCtor()();
    else
      LLPC_ERRS("LLPC" << ": cannot create pass: "
             << PassInf->getPassName() << "\n");
    if (P) {
      PassKind Kind = P->getPassKind();
      addPass(Passes, P);

      if (AnalyzeOnly) {
        switch (Kind) {
        case PT_BasicBlock:
          Passes.add(createBasicBlockPassPrinter(PassInf, outs(), Quiet));
          break;
        case PT_Region:
          Passes.add(createRegionPassPrinter(PassInf, outs(), Quiet));
          break;
        case PT_Loop:
          Passes.add(createLoopPassPrinter(PassInf, outs(), Quiet));
          break;
        case PT_Function:
          Passes.add(createFunctionPassPrinter(PassInf, outs(), Quiet));
          break;
        case PT_CallGraphSCC:
          Passes.add(createCallGraphPassPrinter(PassInf, outs(), Quiet));
          break;
        default:
          Passes.add(createModulePassPrinter(PassInf, outs(), Quiet));
          break;
        }
      }
    }

    if (PrintEachXForm)
      Passes.add(
          createPrintModulePass(errs(), "", PreserveAssemblyUseListOrder));
  }

  if (StandardLinkOpts) {
    AddStandardLinkPasses(Passes);
    StandardLinkOpts = false;
  }

  if (OptLevelO0)
    AddOptimizationPasses(Passes, *FPasses, TM.get(), 0, 0);

  if (OptLevelO1)
    AddOptimizationPasses(Passes, *FPasses, TM.get(), 1, 0);

  if (OptLevelO2)
    AddOptimizationPasses(Passes, *FPasses, TM.get(), 2, 0);

  if (OptLevelOs)
    AddOptimizationPasses(Passes, *FPasses, TM.get(), 2, 1);

  if (OptLevelOz)
    AddOptimizationPasses(Passes, *FPasses, TM.get(), 2, 2);

  if (OptLevelO3)
    AddOptimizationPasses(Passes, *FPasses, TM.get(), 3, 0);

  if (FPasses) {
    FPasses->doInitialization();
    for (Function &F : *M)
      FPasses->run(F);
    FPasses->doFinalization();
  }

  // Check that the module is well formed on completion of optimization
  if (!NoVerify && !VerifyEach)
    Passes.add(createVerifierPass());

  // In run twice mode, we want to make sure the output is bit-by-bit
  // equivalent if we run the pass manager again, so setup two buffers and
  // a stream to write to them. Note that llc does something similar and it
  // may be worth to abstract this out in the future.
  SmallVector<char, 0> Buffer;
  SmallVector<char, 0> CompileTwiceBuffer;
  std::unique_ptr<raw_svector_ostream> BOS;
  raw_ostream *OS = nullptr;

  // Write bitcode or assembly to the output as the last step...
  if (!NoOutput && !AnalyzeOnly) {
    OS = &outs();
    if (OutputAssembly) {
      if (EmitSummaryIndex)
        report_fatal_error("Text output is incompatible with -module-summary");
      if (EmitModuleHash)
        report_fatal_error("Text output is incompatible with -module-hash");
      Passes.add(createPrintModulePass(*OS, "", PreserveAssemblyUseListOrder));
    } else if (OutputThinLTOBC)
      Passes.add(createWriteThinLTOBitcodePass(*OS));
    else
      Passes.add(createBitcodeWriterPass(*OS, PreserveBitcodeUseListOrder,
                                         EmitSummaryIndex, EmitModuleHash));
  }

  // Before executing passes, print the final values of the LLVM options.
  cl::PrintOptionValues();

  // Now that we have all of the passes ready, run them.
  Passes.run(*M);

  if (YamlFile)
    YamlFile->keep();

  return true;
}

//===----------------------------------------------------------------------===//
// Dummy implementation of getLazyIRFileModule.
// It is to remove the dependency on LLVMIRReader.lib and LLVMAsmParser.lib
std::unique_ptr<Module> llvm::getLazyIRFileModule(StringRef Filename,
                                                  SMDiagnostic &Err,
                                                  LLVMContext &Context,
                                                  bool ShouldLazyLoadMetadata) {
  LLPC_NEVER_CALLED();
  return nullptr;
}
