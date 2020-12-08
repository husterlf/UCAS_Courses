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
#include <llvm/IR/InstIterator.h>
using namespace llvm;

struct LivenessInfo
{
    map<Value *, set<Value *>> valFuncMap;
    map<Value *, set<Value *>> valFieldMap; //*p value
    LivenessInfo() {}

    bool operator==(const LivenessInfo &info) const
    {
        return (valFuncMap == info.valFuncMap) && (valFieldMap == info.valFieldMap);
    }

    LivenessInfo &operator=(const LivenessInfo &info)
    {
        valFuncMap = info.valFuncMap;
        valFieldMap = info.valFieldMap;
        return *this;
    }
};

class LivenessVisitor : public DataflowVisitor<struct LivenessInfo>
{
    map<int, set<string>> outputResults;
    map<CallInst *, set<Function *>> callFuncResults;
    set<Function *> funcSet;

    DataflowResult<LivenessInfo>::Ins *pResult;

    void setDataFlowPtr(DataflowResult<LivenessInfo>::Ins *p)
    {
        pResult = p;
    }

public:
    LivenessVisitor()
    {
        pResult = NULL;
    }

    void insertFuncSet(set<Function *> &fSet)
    {
        fSet.insert(funcSet.begin(), funcSet.end());
    }

    void clearFuncSet()
    {
        funcSet.clear();
    }

    void merge(LivenessInfo *dest, const LivenessInfo &src) override
    {
        //src直接插入到dest
        for (auto val : src.valFuncMap)
        {
            dest->valFuncMap[val.first].insert(val.second.begin(), val.second.end());
        }

        for (auto val : src.valFieldMap)
        {
            dest->valFieldMap[val.first].insert(val.second.begin(), val.second.end());
        }
    }

    void ProcessPHINode(PHINode *phiNode, LivenessInfo &info)
    {
        //just insert for phi
        info.valFuncMap[phiNode].clear();
        for (Value *value : phiNode->incoming_values())
        {
            if (isa<Function>(value))
                info.valFuncMap[phiNode].insert(value);

            else
            {
                set<Value *> values = info.valFuncMap[value];
                info.valFuncMap[phiNode].insert(values.begin(), values.end());
            }
        }
    }

    //###
    void ProcessCallInst(CallInst *inst, LivenessInfo &info)
    {
        callFuncResults[inst].clear();
        set<Function *> callees;
        Value *value = inst->getCalledValue();
        if (auto *func = dyn_cast<Function>(value))
        {
            callees.insert(func);
        }
        else
        {
            set<Value *> valueSet;
            if (info.valFuncMap.count(value))
            {
                valueSet.insert(info.valFuncMap[value].begin(), info.valFuncMap[value].end());
            }

            while (!valueSet.empty())
            {
                Value *v = *(valueSet.begin());
                valueSet.erase(valueSet.begin());
                if (auto *func = dyn_cast<Function>(v))
                {
                    callees.insert(func);
                }
                else
                {
                    valueSet.insert(info.valFuncMap[v].begin(), info.valFuncMap[v].end());
                }
            }
        }

        
        callFuncResults[inst].insert(callees.begin(), callees.end());


        if (inst->getCalledFunction() && inst->getCalledFunction()->isDeclaration())
        {
            (*pResult)[inst].second = info;
            return;
        }

        //  int cc=0;
        for (auto calleei = callees.begin(), calleee = callees.end(); calleei != calleee; calleei++)
        {
            //   //errs()<<"cc:"<<cc++<<"\n";
            Function *callee = *calleei;
            if (callee->isDeclaration())
            {
                continue;
            }
            std::map<Value *, Argument *> mapValueToArg;

            for (int argNum = 0; argNum < inst->getNumArgOperands(); ++argNum)
            {
                Value *callerArg = inst->getArgOperand(argNum);
                if (callerArg->getType()->isPointerTy())
                {
                    // only consider pointer
                    Argument *calleeArg = callee->arg_begin() + argNum;
                    mapValueToArg.insert(std::make_pair(callerArg, calleeArg));
                }
            }

            if (mapValueToArg.empty())
                continue;

            // replace valFuncMap
            LivenessInfo calleeInstFirst = (*pResult)[inst].first;
            LivenessInfo &calleeInstFisrt = (*pResult)[&*inst_begin(callee)].first;
            LivenessInfo oldCalleeInstFisrt = calleeInstFisrt;
            for (auto bi = calleeInstFirst.valFuncMap.begin(); bi != calleeInstFirst.valFuncMap.end(); ++bi)
            {
                for (auto argi = mapValueToArg.begin(); argi != mapValueToArg.end(); ++argi)
                {
                    if (bi->second.count(argi->first) && !isa<Function>(argi->first))
                    {
                        // 函数
                        bi->second.erase(argi->first);
                        bi->second.insert(argi->second);
                    }
                }
            }

            for (auto bi = calleeInstFirst.valFieldMap.begin(); bi != calleeInstFirst.valFieldMap.end(); ++bi)
            {
                for (auto argi = mapValueToArg.begin(); argi != mapValueToArg.end(); ++argi)
                {
                    if (bi->second.count(argi->first) && !isa<Function>(argi->first))
                    {
                        bi->second.erase(argi->first);
                        bi->second.insert(argi->second);
                    }
                }
            }

            for (auto argi = mapValueToArg.begin(); argi != mapValueToArg.end(); ++argi)
            {
                if (calleeInstFirst.valFuncMap.count(argi->first))
                {
                    set<Value *> values = calleeInstFirst.valFuncMap[argi->first];
                    calleeInstFirst.valFuncMap.erase(argi->first);
                    calleeInstFirst.valFuncMap[argi->second].insert(values.begin(), values.end());
                }

                if (calleeInstFirst.valFieldMap.count(argi->first))
                {
                    set<Value *> values = calleeInstFirst.valFieldMap[argi->first];
                    calleeInstFirst.valFieldMap.erase(argi->first);
                    calleeInstFirst.valFieldMap[argi->second].insert(values.begin(), values.end());
                }
            }

            for (auto argi = mapValueToArg.begin(); argi != mapValueToArg.end(); ++argi)
            {
                if (isa<Function>(argi->first))
                {
                    calleeInstFirst.valFuncMap[argi->second].insert(argi->first);
                }
            }

            merge(&calleeInstFisrt, calleeInstFirst);
            if (!(oldCalleeInstFisrt == calleeInstFisrt))
                funcSet.insert(callee);
        
        }
    }

    //###
    void ProcessStoreInst(StoreInst *inst, LivenessInfo &info)
    {
        set<Value *> values;
        if (info.valFuncMap[inst->getValueOperand()].empty())
        {
            values.insert(inst->getValueOperand());
        }
        else
        {
            set<Value *> &tmp = info.valFuncMap[inst->getValueOperand()];
            values.insert(tmp.begin(), tmp.end());
        }

        if (auto *getElementPtrInst = dyn_cast<GetElementPtrInst>(inst->getPointerOperand()))
        {
            Value *pointerOperand = getElementPtrInst->getPointerOperand();
            if (info.valFuncMap[pointerOperand].empty())
            {
                info.valFieldMap[pointerOperand].clear();
                info.valFieldMap[pointerOperand].insert(values.begin(), values.end());
            }
            else
            {
                set<Value *> tmp = info.valFuncMap[pointerOperand];
                for (auto tmpi = tmp.begin(), tmpe = tmp.end(); tmpi != tmpe; tmpi++)
                {
                    Value *v = *tmpi;
                    info.valFieldMap[v].clear();
                    info.valFieldMap[v].insert(values.begin(), values.end());
                }
            }
        }
        else
        {
            info.valFuncMap[inst->getPointerOperand()].clear();
            info.valFuncMap[inst->getPointerOperand()].insert(values.begin(), values.end());
        }
    }

    //###
    void ProcessLoadInst(LoadInst *inst, LivenessInfo &info)
    {
        info.valFuncMap[inst].clear();
        if (auto *getElementPtrInst = dyn_cast<GetElementPtrInst>(inst->getPointerOperand()))
        {
            Value *pointerOperand = getElementPtrInst->getPointerOperand();
            if (info.valFuncMap[pointerOperand].empty())
            {
                set<Value *> &tmp = info.valFieldMap[pointerOperand];
                info.valFuncMap[inst].insert(tmp.begin(), tmp.end());
            }
            else
            {
                set<Value *> &values = info.valFuncMap[pointerOperand];
                for (auto valuei = values.begin(), valuee = values.end(); valuei != valuee; valuei++)
                {
                    Value *v = *valuei;
                    set<Value *> &tmp = info.valFieldMap[v];
                    info.valFuncMap[inst].insert(tmp.begin(), tmp.end());
                }
            }
        }
        else
        {
            set<Value *> tmp = info.valFuncMap[inst->getPointerOperand()];
            info.valFuncMap[inst].insert(tmp.begin(), tmp.end());
        }
    }

    void ProcessReturnInst(ReturnInst *inst, LivenessInfo &info)
    {
        Function *returnFunc = inst->getFunction();
        
        for (auto fi = callFuncResults.begin(); fi != callFuncResults.end();  ++fi)
        {
            //包含这个fun的callInst
            if (fi->second.count(returnFunc))
            { 
                CallInst *callInst = fi->first;
                Function *caller = callInst->getFunction();

                //函数调用对应好的参数映射
                map<Value *, Argument *> mapValueToArg;
                for (unsigned argNum = 0; argNum < callInst->getNumArgOperands();  ++argNum)
                {
                    Value *callerArg = callInst->getArgOperand(argNum);
                    if (!callerArg->getType()->isPointerTy())
                        continue;
                    Argument *calleeArg = returnFunc->arg_begin() + argNum;
                    mapValueToArg.insert(make_pair(callerArg, calleeArg));
                }

                LivenessInfo returnInstFirst = (*pResult)[inst].first;
                LivenessInfo &callInstSecond= (*pResult)[callInst].second;
                LivenessInfo oldCallInstSecond = (*pResult)[callInst].second;

                if (inst->getReturnValue() &&
                    inst->getReturnValue()->getType()->isPointerTy())
                {
                    set<Value *> values = returnInstFirst.valFuncMap[inst->getReturnValue()];
                    returnInstFirst.valFuncMap.erase(inst->getReturnValue());
                    returnInstFirst.valFuncMap[callInst].insert(values.begin(), values.end());
                }
                //  valFuncMap
                for (auto bi = returnInstFirst.valFuncMap.begin(); bi != returnInstFirst.valFuncMap.end(); ++bi)
                {
                    for (auto argi = mapValueToArg.begin();argi != mapValueToArg.end(); ++argi)
                    {
                        if (bi->second.count(argi->second))
                        {
                            bi->second.erase(argi->second);
                            bi->second.insert(argi->first);
                        }
                    }
                }

                // valFieldMap
                for (auto bi = returnInstFirst.valFieldMap.begin(); bi != returnInstFirst.valFieldMap.end(); ++bi)
                {
                    for (auto argi = mapValueToArg.begin(); argi != mapValueToArg.end();++argi)
                    {
                        if (bi->second.count(argi->second))
                        {
                            bi->second.erase(argi->second);
                            bi->second.insert(argi->first);
                        }
                    }
                }
                for (auto argi = mapValueToArg.begin(); argi != mapValueToArg.end(); ++argi)
                {
                    if (returnInstFirst.valFuncMap.count(argi->second))
                    {
                        set<Value *> values = returnInstFirst.valFuncMap[argi->second];
                        returnInstFirst.valFuncMap.erase(argi->second);
                        returnInstFirst.valFuncMap[argi->first].insert(values.begin(), values.end());
                    }
                    if (returnInstFirst.valFieldMap.count(argi->second))
                    {
                        set<Value *> values = returnInstFirst.valFieldMap[argi->second];
                        returnInstFirst.valFieldMap.erase(argi->second);
                        returnInstFirst.valFieldMap[argi->first].insert(values.begin(), values.end());
                    }
                }

                merge(&callInstSecond, returnInstFirst);
                if (!(callInstSecond == oldCallInstSecond))
                {
                    funcSet.insert(caller);
                }
            }
        }
    }

    void ProcessGetElementPtrInst(GetElementPtrInst *inst, LivenessInfo &info)
    {
        info.valFuncMap[inst].clear();

        Value *ptrVal = inst->getPointerOperand();
        if (info.valFuncMap[ptrVal].empty())
        {
            info.valFuncMap[inst].insert(ptrVal);
        }
        else
        {
            info.valFuncMap[inst].insert(info.valFuncMap[ptrVal].begin(), info.valFuncMap[ptrVal].end());
        }
    }

    void ProcessMemCpyInst(MemCpyInst *inst, LivenessInfo &info)
    {
        //Malloc() inst
        BitCastInst *bcInst1 = dyn_cast<BitCastInst>(inst->getArgOperand(0));
        BitCastInst *bcInst2 = dyn_cast<BitCastInst>(inst->getArgOperand(1));
        if (bcInst1 == NULL || bcInst2 == NULL)
            return;

        Value *dst = bcInst1->getOperand(0);
        Value *src = bcInst2->getOperand(0);

        info.valFuncMap[dst].clear();
        info.valFuncMap[dst].insert(info.valFuncMap[src].begin(), info.valFuncMap[src].end());

        info.valFieldMap[dst].clear();
        info.valFieldMap[dst].insert(info.valFieldMap[src].begin(), info.valFieldMap[src].end());
    }

    void compDFVal(Instruction *inst, DataflowResult<LivenessInfo>::Ins *result) override
    {
        //set pResult
        this->setDataFlowPtr(result);
        // if (isa<DbgInfoIntrinsic>(inst))
        //    return;
        //  int line = inst->getDebugLoc().getLine();
        //errs() << "line:" << line << ":";

        LivenessInfo &instFirstInfo = (*pResult)[inst].first;
        LivenessInfo &instSecondInfo = (*pResult)[inst].second;

        if (isa<IntrinsicInst>(inst))
        {
            if (auto *memCpyInst = dyn_cast<MemCpyInst>(inst))
            {
                //errs() << "MemCpyInst\n";
                ProcessMemCpyInst(memCpyInst, instFirstInfo);
            }
            else
            {
                //errs() << "IntrinsicInst\n";
            }
        }
        else if (auto *callInst = dyn_cast<CallInst>(inst))
        {

            //errs() << "CallInst\n";
            ProcessCallInst(callInst, instFirstInfo);
            return;
        }
        else if (auto *phiNode = dyn_cast<PHINode>(inst))
        {
            //errs() << "PHINode\n";

            ProcessPHINode(phiNode, instFirstInfo);
        }
        else if (auto *storeInst = dyn_cast<StoreInst>(inst))
        {
            //errs() << "StoreInst\n";
            ProcessStoreInst(storeInst, instFirstInfo);
        }
        else if (auto *loadInst = dyn_cast<LoadInst>(inst))
        {
            //errs() << "LoadInst\n";
            ProcessLoadInst(loadInst, instFirstInfo);
        }
        else if (auto *returnInst = dyn_cast<ReturnInst>(inst))
        {
            //errs() << "ReturnInst\n";
            ProcessReturnInst(returnInst, instFirstInfo);
        }
        else if (auto *getElementPtrInst = dyn_cast<GetElementPtrInst>(inst))
        {

            //errs() << "GetElementPtrInst\n";
            ProcessGetElementPtrInst(getElementPtrInst, instFirstInfo);
        }
        else
        {
            //errs() << "Other Instruction \n";
        }

        instSecondInfo = instFirstInfo;
    }

    void ProcessPHI(PHINode *inst, LivenessInfo &info)
    {
        info.valFuncMap[inst].clear();
        for (Value *value : inst->incoming_values())
        {
            if (isa<Function>(value))
            {
                info.valFuncMap[inst].insert(value);
            }
            else
            {
                std::set<Value *> &values = info.valFuncMap[value];
                info.valFuncMap[inst].insert(values.begin(), values.end());
            }
        }
    }

    void printResult()
    {
        outputResults.clear();
        for (auto &i : callFuncResults)
        {
            int line = i.first->getDebugLoc().getLine();
            for (auto &j : i.second)
            {

                string name = j->getName();
                outputResults[line].insert(name);
            }
        }

        for (auto &it : outputResults)
        {
            //errs() << it.first << ":";
            cout << it.first << ":";
            std::string line_content;
            for (auto str : it.second)
            {
                line_content += (str + ",");
            }
            line_content = line_content.substr(0, line_content.size() - 1);
            //errs() << line_content << "\n";
            cout << line_content << "\n";
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
        return false;
    }
};