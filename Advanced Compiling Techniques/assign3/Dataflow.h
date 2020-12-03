/************************************************************************
 *
 * @file Dataflow.h
 *
 * General dataflow framework
 *
 ***********************************************************************/

#ifndef _DATAFLOW_H_
#define _DATAFLOW_H_

#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Function.h>

#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

using namespace llvm;

///
/// Dummy class to provide a typedef for the detailed result set
/// For each basicblock, we compute its input dataflow val and its output dataflow val
///
template <class T>
struct DataflowResult
{
    typedef typename std::map<BasicBlock *, std::pair<T, T>> Type;
    typedef typename std::map<Instruction *, std::pair<T, T>> Ins;
};

///Base dataflow visitor class, defines the dataflow function

template <class T>
class DataflowVisitor
{
public:
    virtual ~DataflowVisitor() {}

    /// Dataflow Function invoked for each basic block
    ///
    /// @block the Basic Block
    /// @dfval the input dataflow value
    /// @isforward true to compute dfval forward, otherwise backward
    virtual void compDFVal(BasicBlock *block,typename DataflowResult<T>::Ins *result , bool isforward)
    {
        if (isforward == true)
        {
            for (BasicBlock::iterator ii = block->begin(), ie = block->end();
                 ii != ie; ++ii)
            {
                Instruction *inst = &*ii;
                compDFVal(inst, result);
            }
        }
        else
        {
            for (BasicBlock::reverse_iterator ii = block->rbegin(), ie = block->rend();
                 ii != ie; ++ii)
            {
                Instruction *inst = &*ii;
                compDFVal(inst, result);
            }
        }
    }

    ///
    /// Dataflow Function invoked for each instruction
    ///
    /// @inst the Instruction
    /// @dfval the input dataflow value
    /// @return true if dfval changed
    virtual void compDFVal(Instruction *inst,typename DataflowResult<T>::Ins *insResult) = 0;

    ///
    /// Merge of two dfvals, dest will be ther merged result
    /// @return true if dest changed
    ///
    virtual void merge(T *dest, const T &src) = 0;
};



///
/// Compute a forward iterated fixedpoint dataflow function, using a user-supplied
/// visitor function. Note that the caller must ensure that the function is
/// in fact a monotone function, as otherwise the fixedpoint may not terminate.
///
/// @param fn The function
/// @param visitor A function to compute dataflow vals
/// @param result The results of the dataflow
/// @initval the Initial dataflow value
template <class T>
void compForwardDataflow(Function *fn,
                         DataflowVisitor<T> *visitor,
                         typename DataflowResult<T>::Type *result,
                         const T &initval)
{
    return;
}
///
/// Compute a backward iterated fixedpoint dataflow function, using a user-supplied
/// visitor function. Note that the caller must ensure that the function is
/// in fact a monotone function, as otherwise the fixedpoint may not terminate.
///
/// @param fn The function
/// @param visitor A function to compute dataflow vals
/// @param result The results of the dataflow
/// @initval The initial dataflow value
template <class T>
void compBackwardDataflow(Function *fn,
                          DataflowVisitor<T> *visitor,
                          typename DataflowResult<T>::Type *result,
                          const T &initval)
{

    std::set<BasicBlock *> worklist;

    // Initialize the worklist with all exit blocks
    for (Function::iterator bi = fn->begin(); bi != fn->end(); ++bi)
    {
        BasicBlock *bb = &*bi;
        result->insert(std::make_pair(bb, std::make_pair(initval, initval)));//初始化基本块对应的活跃信息
                                                                            //block的信息对应两条，
        worklist.insert(bb);
    }

    // Iteratively compute the dataflow result
    // 迭代计算数据流的结果
    while (!worklist.empty())
    {
        BasicBlock *bb = *worklist.begin();
        worklist.erase(worklist.begin());

        // Merge all incoming value
        T bbexitval = (*result)[bb].second;
        for (auto si = succ_begin(bb), se = succ_end(bb); si != se; si++)
        {
            BasicBlock *succ = *si;
            visitor->merge(&bbexitval, (*result)[succ].first);
        }

        (*result)[bb].second = bbexitval;
        //visitor->compDFVal(bb, &bbexitval, false);//经过bb后的数据变化

        // If outgoing value changed, propagate it along the CFG
        // 输出值变化，沿着控制流图进行更新
        // 流表示
        if (bbexitval == (*result)[bb].first)//未发生变化
            continue;
        (*result)[bb].first = bbexitval;//bb.first更新，且需要将数据更新到前向的所有基本块
                                        //前向基本块有闭环也会在该基本块处断开

        for (pred_iterator pi = pred_begin(bb), pe = pred_end(bb); pi != pe; pi++)
        {
            worklist.insert(*pi);
        }
    }
}

template <class T>
void printDataflowResult(raw_ostream &out,
                         const typename DataflowResult<T>::Type &dfresult)
{
    for (typename DataflowResult<T>::Type::const_iterator it = dfresult.begin();
         it != dfresult.end(); ++it)
    {
        if (it->first == NULL)
            out << "*";
        // else it->first->dump();
        else
            it->first->print(llvm::errs(), nullptr);
        out << "\n\tin : "
            << it->second.first
            << "\n\tout :  "
            << it->second.second
            << "\n";
    }
}

template <class T>
void compCallFuncDataflow(Function *fn,
                          DataflowVisitor<T> *visitor,
                          typename DataflowResult<T>::Ins *result,
                          const T &initval)
{

    std::set<BasicBlock *> worklist;

    // Initialize the worklist with all exit blocks
    for (Function::iterator bi = fn->begin(); bi != fn->end(); ++bi)
    {
        BasicBlock *bb = &*bi;
        worklist.insert(bb);
        for (auto ii = bb->begin(); ii != bb->end(); ++ii)
        {
            if(auto inst=dyn_cast<Instruction>(ii))
                result->insert(std::make_pair(inst, std::make_pair(initval, initval)));
        }
    }

    // Iteratively compute the dataflow result
    while (!worklist.empty())
    {
        BasicBlock *bb = *worklist.begin();
        worklist.erase(worklist.begin());

        Instruction *bb_first_inst = dyn_cast<Instruction>(bb->begin());
        Instruction *bb_last_inst = bb->getTerminator();
        //bb->getTerminator();
        // Merge all incoming value
        T bbexitval = (*result)[bb_last_inst].second;
        for (pred_iterator pi = pred_begin(bb), pe = pred_end(bb); pi != pe; pi++)
        {
            BasicBlock *p = *pi;
            Instruction *bpi=dyn_cast<Instruction>(--p->end());
            visitor->merge(&bbexitval, (*result)[bpi].first);
        }

        (*result)[bb_first_inst].second = bbexitval;
        visitor->compDFVal(bb, result, true);

        // If outgoing value changed, propagate it along the CFG
        if (bbexitval == (*result)[bb_first_inst].first)
            continue;
        (*result)[bb_first_inst].first = bbexitval;

        for (succ_iterator si = succ_begin(bb), se = succ_end(bb); si != se; si++)
        {
            worklist.insert(*si);
        }
    }
}

#endif /* !_DATAFLOW_H_ */
