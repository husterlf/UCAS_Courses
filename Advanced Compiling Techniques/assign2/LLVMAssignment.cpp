//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include <llvm/Support/CommandLine.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/ScaledNumber.h>

#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>

using namespace llvm;
static ManagedStatic<LLVMContext> GlobalContext;
static LLVMContext &getGlobalContext() { return *GlobalContext; }
/* In LLVM 5.0, when  -O0 passed to clang , the functions generated with clang will
 * have optnone attribute which would lead to some transform passes disabled, like mem2reg.
 */
struct EnableFunctionOptPass : public FunctionPass
{
  static char ID;
  EnableFunctionOptPass() : FunctionPass(ID) {}
  bool runOnFunction(Function &F) override
  {
    if (F.hasFnAttribute(Attribute::OptimizeNone))
    {
      F.removeFnAttr(Attribute::OptimizeNone);
    }
    return true;
  }
};

char EnableFunctionOptPass::ID = 0;

///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 2
///Updated 11/10/2017 by fargo: make all functions
///processed by mem2reg before this pass.
struct FuncPtrPass : public ModulePass
{
  static char ID; // Pass identification, replacement for typeid
  FuncPtrPass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override
  {
    errs() << "Module Name: ";
    errs().write_escaped(M.getName()) << '\n';
    int count = 0;
    //iterator funcs

    for (auto fi = M.begin(); fi != M.end(); ++fi)
    {
      Function &f = *fi;
      for (auto bi = f.begin(); bi != f.end(); ++bi)
      {
        BasicBlock &b = *bi;
        for (auto ii = b.begin(); ii != b.end(); ++ii)
        {
          if (CallInst *call_inst = dyn_cast<CallInst>(ii))
          {
            ++count;
            Function *f_call = call_inst->getCalledFunction();//call func directly
            
            if (f_call != NULL)
            {
              unsigned line = call_inst->getDebugLoc().getLine();
              if(line!=0)
                errs() << line << ":" << f_call->getName() << "\n";
            }
            else
            {//value level 
              unsigned line = call_inst->getDebugLoc().getLine();
              Value *v=call_inst->getCalledValue();
              errs()<<"line: "<<line<<"  v's type: "<<v->getType()<<"\n";
              //PHINode,Argument,CallInst
              if(PHINode *phi=dyn_cast<PHINode>(v))
              {

              }
              else if(Argument *arg=dyn_cast<Argument>(v))
              {

              }
              else if(CallInst *call=dyn_cast<CallInst>(v))
              {
                
              }
              else{
                errs()<<"other value type:"<<v->getType()<<"\n";
              }
              
            }
            
          }
        }
      }
    }

    //iterator global var
    /*for (Module::global_iterator gi = M.global_begin(), ge = M.global_end(); gi != ge; gi++)
    {
      GlobalVariable &g = *gi;
      errs().write_escaped(gi->getName()) << '\n';
    }*/

    //M.dump();
    //M.print(llvm::outs(), nullptr);
    M.size();

    M.getDataLayout();
    errs() << "------------------------------\n";
    return false; //return false if only analyse
  }

  // We don't modify the program, so we preserve // all analyses.
  void getAnalysisUsage(AnalysisUsage &AU) const
  {
    //AU.addRequired
    AU.setPreservesCFG();
  }
};

char FuncPtrPass::ID = 0;
static RegisterPass<FuncPtrPass> X("funcptrpass", "Print function call instruction");

static cl::opt<std::string>
    InputFilename(cl::Positional,
                  cl::desc("<filename>.bc"),
                  cl::init(""));

int main(int argc, char **argv)
{
  LLVMContext &Context = getGlobalContext();
  SMDiagnostic Err;
  // Parse the command line to read the Inputfilename
  cl::ParseCommandLineOptions(argc, argv,
                              "FuncPtrPass \n My first LLVM too which does not do much.\n");

  // Load the input module
  std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
  if (!M)
  {
    Err.print(argv[0], errs());
    return 1;
  }

  llvm::legacy::PassManager Passes;

  ///Remove functions' optnone attribute in LLVM5.0
  Passes.add(new EnableFunctionOptPass());
  ///Transform it to SSA
  Passes.add(llvm::createPromoteMemoryToRegisterPass());

  /// Your pass to print Function and Call Instructions
  Passes.add(new FuncPtrPass());
  Passes.run(*M.get());
}
