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
    //map<Value*,set<Function*>> valFuncList;
    set<Instruction *> LiveVars; /// Set of variables which are live
    LivenessInfo() : LiveVars() {}
    LivenessInfo(const LivenessInfo &info) : LiveVars(info.LiveVars) {}

    bool operator==(const LivenessInfo &info) const
    {
        return LiveVars == info.LiveVars;
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
    std::map<int, std::vector<std::string>> callFuncResults;

public:
    LivenessVisitor() {}

    void printResult()
    {
        for (auto &it : callFuncResults)
        {
            errs() << it.first << ":";
            std::string line_content;
            for (auto str : it.second)
            {
                line_content += (str + ",");
            }
            line_content.substr(0, line_content.size() - 1);
            errs() << line_content << "\n";
        }
    }
    //required func merge
    void merge(LivenessInfo *dest, const LivenessInfo &src) override
    {
        for (std::set<Instruction *>::const_iterator ii = src.LiveVars.begin(),
                                                     ie = src.LiveVars.end();
             ii != ie; ++ii)
        {
            dest->LiveVars.insert(*ii);
        }
    }

    //required func compDFVal
    void compDFVal(Instruction *inst, LivenessInfo *dfval) override
    {
        if (isa<DbgInfoIntrinsic>(inst))
            return;

        if (auto *callInst = dyn_cast<CallInst>(inst))
        {

            errs() << "I am in CallInst"
                   << "\n";
            Value *value = callInst->getCalledValue();
            if (auto *func = dyn_cast<Function>(value))
            {
                errs() << "Function\n";
            }
            else
            {
                set<Value *> value_worklist;
                /*if (dfval.LiveVars_map.count(value))
                {
                    value_worklist.insert(dfval.LiveVars_map[value].begin(), dfval.LiveVars_map[value].end());
                }

                while (!value_worklist.empty())
                {
                    Value *v = *(value_worklist.begin());
                    value_worklist.erase(value_worklist.begin());
                    if (auto *func = dyn_cast<Function>(v))
                    {
                        callees.insert(func);
                    }
                    else
                    {
                        value_worklist.insert(dfval.LiveVars_map[v].begin(), dfval.LiveVars_map[v].end());
                    }
                    //前向访问找到所有的func
                }*/
            }
        }

        dfval->LiveVars.erase(inst);
        for (User::op_iterator oi = inst->op_begin(), oe = inst->op_end();
             oi != oe; ++oi)
        {
            Value *val = *oi;
            if (isa<Instruction>(val))
            {
                dfval->LiveVars.insert(cast<Instruction>(val));
            }
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
