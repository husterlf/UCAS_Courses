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
#include <llvm/Analysis/DominanceFrontier.h>

#include<string>
#include<map>
#include<vector>

using namespace std;

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

  map<int,vector<string>> results;
  vector<string> names;

  bool runOnModule(Module &M) override
  {
    //直接调用函数时的输出
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
            Function *f_call = call_inst->getCalledFunction();

            unsigned line = call_inst->getDebugLoc().getLine();
            if (line != 0)
              {
                if(results.count(line)==0)
                {
                  vector<string> names;
                  results[line]=names;
                }
              }

            if (f_call != NULL)
            {
              //call func directly
              if (line != 0)
              {
                errs() << line << ":"<<f_call->getName()<<"\n";
                results[line].push_back(f_call->getName());
              }
                
            }
            else
            {
              // value level
              // unsigned line = call_inst->getDebugLoc().getLine();
              Value *v = call_inst->getCalledValue();
              errs() << line << ":"; //<< "  v's type: " << v->getType() << "\n";
              //PHINode,Argument,CallInst
              if (PHINode *phi = dyn_cast<PHINode>(v))
              {
                //PHI node,contains combine SSA
                //errs() << "phi \n";
                //errs()<<phi->getNumOperands()<<"\n";
                callPHINode(phi);
              }
              else if (Argument *arg = dyn_cast<Argument>(v))
              {
                callArgument(arg);
              }
              else if (CallInst *call = dyn_cast<CallInst>(v))
              {//
                callCallIns(call);
              }
              else
              {
                errs() << "other value type:" << v->getType() << "\n";
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
    //errs() << "------------------------------\n";
    // M.print(llvm::outs(), nullptr);
    M.size();

   // M.getDataLayout();
    errs() << "------------------------------\n";
    return false; //return false if only analyse
  }

  // We don't modify the program, so we preserve // all analyses.
  void getAnalysisUsage(AnalysisUsage &AU) const
  {
    //AU.addRequired
    AU.setPreservesCFG();
  }

  //
  void callPHINode(PHINode *phi)
  {
    for (auto op = phi->op_begin(); op != phi->op_end(); ++op)
    {
      if (auto tmp1 = dyn_cast<PHINode>(op))
      {
        callPHINode(tmp1);
      }
      else if (auto tmp2 = dyn_cast<Argument>(op))
      {
        callArgument(tmp2);
      }
      else if (auto tmp3 = dyn_cast<Function>(op))
      {
        //errs()<<"tmp3\n";
        string strTmp=(op==(phi->op_end()-1))?"":",";
        errs() << tmp3->getName() <<strTmp;
      }
      else
      {
        if(op==(phi->op_end()-1))
        {
          string strTmp=",";
          errs() <<"NULL";
        }
        
      }

      if(op==(phi->op_end()-1))
        errs()<<"\n";
    }
  }

  void callArgument(Argument *arg)
  {
    //func trans by args
    unsigned int argIndex = arg->getArgNo();
    Function *fParent = arg->getParent();
    //errs()<<argIndex;
    //errs()<<fParent->getName()<<"\n";
    //for(auto ui=funcParent->use_begin();ui!=funcParent)

    for (User *funcUser : fParent->users())
    {
      if (CallInst *callInst = dyn_cast<CallInst>(funcUser))
      {
        // if argument at 3 , then foo(arg1,arg2) will be pass
        if (argIndex + 1 <= callInst->getNumArgOperands())
        {
          Value *value = callInst->getArgOperand(argIndex);
          if (callInst->getCalledFunction() != fParent)
          { // é€’å½’é—®é¢˜
            Function *func = callInst->getCalledFunction();
            for (Function::iterator bi = func->begin(), be = func->end(); bi != be; bi++)
            {
              // for instruction in basicblock
              for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ii++)
              {
                Instruction *inst = dyn_cast<Instruction>(ii);
                if (ReturnInst *retInst = dyn_cast<ReturnInst>(inst))
                {
                  Value *v = retInst->getReturnValue();
                  if (CallInst *call_inst = dyn_cast<CallInst>(v))
                  {
                    Value *value = call_inst->getArgOperand(argIndex);
                    if (Argument *argument = dyn_cast<Argument>(value))
                    {
                      callArgument(argument);
                    }
                  }
                }
              }
            }
          }
          else
          {
            handleValue(value);
          }
        }
      }
      else if (PHINode *phiNode = dyn_cast<PHINode>(funcUser))
      {
        for (User *phiUser : phiNode->users())
        {
          if (CallInst *callInst = dyn_cast<CallInst>(phiUser))
          {
            if (argIndex + 1 <= callInst->getNumArgOperands())
            {
              Value *value = callInst->getArgOperand(argIndex);
              handleValue(value);
            }
          }
        }
      }
    }
  
  }

  void callCallIns(CallInst *call)
  {
    Function *f_call=call->getFunction();
    if(f_call)
    {
      errs()<<f_call->getName()<<"\n";
      //
    }
    else
    {
      Value *v=call->getCalledValue();
      if(PHINode *phi=dyn_cast<PHINode>(v))
      {
        for(auto op=phi->op_begin();op!=phi->op_end();++op)
        {
          if(Function *f=dyn_cast<Function>(op))
          {
            errs()<<f->getName()<<"\n";
          }
        }
      }
    }
    
  }

  void handleValue(Value *val)
  {
    if (PHINode *tmp1 = dyn_cast<PHINode>(val))
    {
      callPHINode(tmp1);
    }
    else if (Function *tmp2 = dyn_cast<Function>(val))
    {
      errs()<<tmp2->getName();
      //Push(func->getName());
    }
    else if (Argument *tmp3 = dyn_cast<Argument>(val))
    {
      callArgument(tmp3);
    }
  }


  void handleFunc(Function *func)
  {
    for (auto bi = func->begin(); bi != func->end();  ++bi)
    {
      for (auto ii = bi->begin(); ii!= bi->end(); ++ii)
      {
        Instruction *inst = dyn_cast<Instruction>(ii);
        if (ReturnInst *retInst = dyn_cast<ReturnInst>(inst))
        {
          Value *value = retInst->getReturnValue();
          if (Argument *argument = dyn_cast<Argument>(value))
          {
            callArgument(argument);
          }
          else if (PHINode *pHINode = dyn_cast<PHINode>(value))
          {
            callPHINode(pHINode);
          }
          else if (CallInst *callIns = dyn_cast<CallInst>(value))
          {
            callCallIns(callIns);
          }
        }
      }
    }
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
