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

#include <string>
#include <map>
#include <set>

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

  map<int, set<string>> results;
  set<string> names;
  string rescurTmp="";

  void printResults()
  {
    for(auto it=results.begin();it!=results.end();++it)
    {
      errs()<<it->first<<":";
      int tmpSize=it->second.size();
      for(auto it1=it->second.begin();it1!=it->second.end();++it1)
      {
        errs()<<*it1;

        --tmpSize;
        if(tmpSize>0)
          errs()<<",";
      }

      errs()<<"\n";
    }
  }

  bool runOnModule(Module &M) override
  {
    //target:
    //1.print line and func name if direct call
    //2.print the possible called func name
    //3.print the direct call func if it is determined
    //exp:do not consider func pointer is stored into memory

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
          { //target instruction
            ++count;
            Function *f_call = call_inst->getCalledFunction();

            unsigned line = call_inst->getDebugLoc().getLine();

            if (f_call != NULL)
            {
              if (line != 0)
              { //"llvm.dbg.value" is only in line 0 AND f_call!=NULL
                //So the below code is for directly func call
               // errs() << line << ":" << f_call->getName() << "\n";
                set<string> tmp;
                tmp.insert(f_call->getName());
                results[line]=tmp;
              }
            }
            else
            {
              
              // value level
              // unsigned line = call_inst->getDebugLoc().getLine();
              Value *v = call_inst->getCalledValue();
              //errs() << line << ":"; //<< "  v's type: " << v->getType() << "\n";
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
              { //
                callCallIns(call);
              }
              else
              {
                errs() << "other value type:" << v->getType() << "\n";
              }

              if(!names.empty())
              {
                results[line]=names;
                names.clear();
              }
              rescurTmp.clear();
            }
          }
        }
      }
    }

    printResults();
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
        int argNum=tmp3->getNumOperands();
        names.insert(tmp3->getName()); 
      }
      else
      {
        if (op == (phi->op_end() - 1))
        {
          string strTmp = ",";
          //errs() << "NULL";
        }
      }

      if (op == (phi->op_end() - 1))
      {
        //errs() << "\n";
      }
        
    }
  }

  void callArgument(Argument *arg)
  {
    //func trans by args
    unsigned int argIndex = arg->getArgNo();
  //  unsigned int argNum=arg->
    Function *fParent = arg->getParent();

    for (User *funcUser : fParent->users())
    {
      if (CallInst *callInst = dyn_cast<CallInst>(funcUser))
      {
        
          unsigned int argOpNum=callInst->getNumArgOperands();
          Value *value = callInst->getArgOperand(argIndex);
          if (callInst->getCalledFunction() != fParent)
          { 
            Function *func = callInst->getCalledFunction();
            for (auto bi = func->begin();bi!= func->end();++bi)
            {
              // for instruction in basicblock
              for (auto ii = bi->begin();ii != bi->end();++ii)
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
      else if (PHINode *phiNode = dyn_cast<PHINode>(funcUser))
      {//func pointer meet in phiNode
        for (User *phiUser : phiNode->users())
        {
          if (CallInst *callInst = dyn_cast<CallInst>(phiUser))
          {
              unsigned int argOpNum=callInst->getNumArgOperands();
              Value *value = callInst->getArgOperand(argIndex);
              handleValue(value);
          }
        }
      }
    }
  }

  void callCallIns(CallInst *call)
  {
    Function *f_call = call->getCalledFunction();//pointer func type
    unsigned int argNum=call->getNumOperands();
    if (f_call)
    {
      names.insert(f_call->getName());
      rescurTmp=f_call->getName();
    //  names.erase(f_call->getName());
      handleFunc(f_call);

    }
    else
    {
      Value *v = call->getCalledValue();
      if (PHINode *phi = dyn_cast<PHINode>(v))
      {
        for (auto op = phi->op_begin(); op != phi->op_end(); ++op)
        {
          if (Function *f = dyn_cast<Function>(op))
          {
            argNum=f->getNumOperands();
            //names.insert(f->getName());
            handleFunc(f);
          }
        }
      }
    }
  }

  void handleValue(Value *val)
  {
    if(!rescurTmp.empty())
        names.erase(rescurTmp);
      rescurTmp.clear();
    if (PHINode *tmp1 = dyn_cast<PHINode>(val))
    {
      
      callPHINode(tmp1);
    }
    else if (Function *tmp2 = dyn_cast<Function>(val))
    {
      names.insert(tmp2->getName());
    }
    else if (Argument *tmp3 = dyn_cast<Argument>(val))
    {
      callArgument(tmp3);
    }
  }

  void handleFunc(Function *func)
  {
    if(func==NULL)
      return;
    for (auto bi = func->begin(); bi != func->end(); ++bi)
    {
      for (auto ii = bi->begin(); ii != bi->end(); ++ii)
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
