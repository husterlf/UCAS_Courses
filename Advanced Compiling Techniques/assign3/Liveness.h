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

#include <llvm/IR/Function.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/IntrinsicInst.h>

#include "Dataflow.h"
using namespace llvm;

//借用这个Liveness实现调用函数分析
struct LivenessInfo
{
    map<Value *, set<Value *>> valFuncMap; //
    set<Instruction *> LiveVars;           /// Set of variables which are live
    LivenessInfo() : LiveVars() {}
    LivenessInfo(const LivenessInfo &info) : LiveVars(info.LiveVars) {}

    bool operator==(const LivenessInfo &info) const
    {
        return (LiveVars == info.LiveVars && valFuncMap == info.valFuncMap);
    }
};

inline raw_ostream &operator<<(raw_ostream &out, const LivenessInfo &info)
{
    for (std::set<Instruction *>::iterator ii = info.LiveVars.begin(), ie = info.LiveVars.end();
         ii != ie; ++ii)
    {
        const Instruction *inst = *ii;
        out << inst->getName();
        out << " ";
    }
    return out;
}

class LivenessVisitor : public DataflowVisitor<struct LivenessInfo>
{
    map<int, set<string>> callFuncResults;
    set<Function*> funcSet;
public:
    LivenessVisitor() {}

    void insertFuncSet(set<Function*> &fSet)
    {
        fSet.insert(funcSet.begin(),funcSet.end());
    }

    void clearFuncSet()
    {
        funcSet.clear();
    }

    void printResult()
    {
       // callFuncResults[11].insert("foo");

        for (auto &it : callFuncResults)
        {
            errs() << it.first << ":";
            std::string line_content;
            for (auto str : it.second)
            {
                line_content += (str + ",");
            }
            line_content=line_content.substr(0, line_content.size()-1 );
            errs() << line_content << "\n";
        }
    }
    //required func merge
    void merge(LivenessInfo *dest, const LivenessInfo &src) override
    {
        for (set<Instruction *>::const_iterator ii = src.LiveVars.begin(),
                                                     ie = src.LiveVars.end();
             ii != ie; ++ii)
        {
            dest->LiveVars.insert(*ii);
        }

        for(auto val:src.valFuncMap)
        {
            dest->valFuncMap[val.first].insert(val.second.begin(),val.second.end());
        }

    }

    //required func compDFVal
    void compDFVal(Instruction *inst,DataflowResult<LivenessInfo>::Ins *insResult) override
    {
        if (isa<DbgInfoIntrinsic>(inst))
            return;

        //MemCpyInst,PHINode,CallInst,StoreInst,LoadInst,ReturnInst,GetElementPtrInst,BitCastInst
        if (auto *callInst = dyn_cast<CallInst>(inst))
        {

            errs() << "CallInst"
                   << "\n";
            Value *value = callInst->getCalledValue();

            if (auto *func = dyn_cast<Function>(value))
            { //函数直接调用
                int line=callInst->getDebugLoc().getLine();
                callFuncResults[line].insert(func->getName());
            }
            else
            {

            }
        }
        else if (auto *tmp = dyn_cast<MemCpyInst>(inst))
        {
            errs() << "MemCpyInst"
                   << "\n";
        }
        else if (auto *tmp = dyn_cast<PHINode>(inst))
        {
            errs() << "PHINode"
                   << "\n";
        }
        else if (auto *tmp = dyn_cast<StoreInst>(inst))
        {
            errs() << "StoreInst"
                   << "\n";
        }
        else if (auto *tmp = dyn_cast<LoadInst>(inst))
        {
            errs() << "LoadInst"
                   << "\n";
        }
        else if (auto *tmp = dyn_cast<ReturnInst>(inst))
        {
            errs() << "ReturnInst"
                   << "\n";
        }
        else if (auto *tmp = dyn_cast<GetElementPtrInst>(inst))
        {
            errs() << "GetElementPtrInst"
                   << "\n";
        }
        else if (auto *tmp = dyn_cast<BitCastInst>(inst))
        {
            errs() << "BitCastInst"
                   << "\n";
        }
        else
        {
            errs() << "Other Inst"
                   << "\n";
            /* code */
        }

    }
};

class Liveness : public FunctionPass
{
public:
    static char ID;
    Liveness() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override
    {
        // F.dump();
        LivenessVisitor visitor;
        DataflowResult<LivenessInfo>::Type result;
        LivenessInfo initval;

        compBackwardDataflow(&F, &visitor, &result, initval);
        printDataflowResult<LivenessInfo>(errs(), result);
        errs() << "------------------------\n";
        return false; //对分析函数未作出任何修改
    }
};
