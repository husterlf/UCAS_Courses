//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//
#include <stdio.h>
#include <vector>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

#define DECL_DEFAULT_VALUE 0

using namespace clang;
using namespace std;

class StackFrame
{
	/// StackFrame maps Variable Declaration to Value
	/// Which are either integer or addresses (also represented using an Integer value)
	std::map<Decl *, int> mVars;
	std::map<Stmt *, int> mExprs;
	/// The current stmt
	Stmt *mPC;

	int mReturnType = -1;
	int64_t mReturnValue = 0;

	//
	vector<vector<int>> mVecs;

public:
	StackFrame() : mVars(), mExprs(), mPC()
	{
	}

	void bindDecl(Decl *decl, int val)
	{
		mVars[decl] = val;
	}
	int getDeclVal(Decl *decl)
	{
		assert(mVars.find(decl) != mVars.end());
		return mVars.find(decl)->second;
	}
	void bindStmt(Stmt *stmt, int val)
	{
		mExprs[stmt] = val;
	}
	int getStmtVal(Stmt *stmt)
	{
		assert(mExprs.find(stmt) != mExprs.end());
		return mExprs[stmt];
	}
	void setPC(Stmt *stmt)
	{
		mPC = stmt;
	}
	Stmt *getPC()
	{
		return mPC;
	}
	void setReturn(int64_t val)
	{
		//mReturnType=type;
		mReturnValue = val;
	}

	int64_t getReturnValue()
	{
		return mReturnValue;
	}
	void createIntVec(int size)
	{
		mVecs.push_back(vector<int>(size));
	}
	int getLastVecLoc()
	{
		return mVecs.size();
	}
	void setVecValue(int loc, int offset, int val)
	{
		mVecs[loc - 1][offset] = val;
	}
	int getVecValue(int loc, int offset)
	{
		return mVecs[loc - 1][offset];
	}
};

/// Heap maps address to a value
/*
class Heap {
public:
   int Malloc(int size) ;
   void Free (int addr) ;
   void Update(int addr, int val) ;
   int get(int addr);
};
*/

class Environment
{
	vector<StackFrame> mStack;

	FunctionDecl *mFree; /// Declartions to the built-in functions
	FunctionDecl *mMalloc;
	FunctionDecl *mInput;
	FunctionDecl *mOutput;

	FunctionDecl *mEntry;

public:
	/// Get the declartions to the built-in functions
	Environment() : mStack(), mFree(NULL), mMalloc(NULL), mInput(NULL), mOutput(NULL), mEntry(NULL)
	{
	}

	/// Initialize the Environment
	void init(TranslationUnitDecl *unit)
	{
		mStack.push_back(StackFrame()); //first should create a stack
		for (TranslationUnitDecl::decl_iterator i = unit->decls_begin(), e = unit->decls_end(); i != e; ++i)
		{
			if (FunctionDecl *fdecl = dyn_cast<FunctionDecl>(*i))
			{
				if (fdecl->getName().equals("FREE"))
					mFree = fdecl;
				else if (fdecl->getName().equals("MALLOC"))
					mMalloc = fdecl;
				else if (fdecl->getName().equals("GET"))
					mInput = fdecl;
				else if (fdecl->getName().equals("PRINT"))
					mOutput = fdecl;
				else if (fdecl->getName().equals("main"))
					mEntry = fdecl;
				else
				{	//other decl func
					//mStack.back().bindStmt();
				}
			}
			else if (VarDecl *vdecl = dyn_cast<VarDecl>(*i))
			{ //global var
				if (vdecl->hasInit())
				{
					if (auto intLiteral = dyn_cast<IntegerLiteral>(vdecl->getInit()))
						mStack.back().bindDecl(vdecl, intLiteral->getValue().getSExtValue());
				}
				else
				{
					mStack.back().bindDecl(vdecl, DECL_DEFAULT_VALUE);
				}
				//int val = intLiteral->getValue().getSExtValue();
				//mStack.back().bindStmt(left, val);
			}
			else
			{ //other decl type
			}
		}
	}

	FunctionDecl *getEntry()
	{
		return mEntry;
	}

	/// !TODO Support comparison operation
	void binop(BinaryOperator *bop)
	{
		Expr *left = bop->getLHS();
		Expr *right = bop->getRHS();

		if (bop->isAssignmentOp())
		{

			if (DeclRefExpr *declexpr = dyn_cast<DeclRefExpr>(left))
			{
				int64_t val = expr(right);
				mStack.back().bindStmt(left, val);
				Decl *decl = declexpr->getFoundDecl();
				mStack.back().bindDecl(decl, val);
			
			}
			else if (auto arraySubscript = dyn_cast<ArraySubscriptExpr>(left))
			{ //left is arraysubscript
				//int a [] 4;
				cout << "ArraySubscript" << endl;
				int64_t index = expr(arraySubscript->getRHS());
				cout << "index: " << index << endl;
				if (DeclRefExpr *declexpr = dyn_cast<DeclRefExpr>(arraySubscript->getLHS()->IgnoreImpCasts()))
				{
					Decl *decl = declexpr->getFoundDecl();
					int loc = mStack.back().getDeclVal(decl);
					int val = expr(right);
					mStack.back().setVecValue(loc, index, val);
				}
			}
		}
		else
		{
			int64_t result;
			switch (bop->getOpcode())
			{
			case BO_GT:
				result = expr(left) > expr(right);
				break;
			case BO_LT:
				result = expr(left) < expr(right);
				break;
			case BO_LE:
				result = expr(left) <= expr(right);
				break;
			case BO_GE:
				result = expr(left) >= expr(right);
				break;
			case BO_EQ:
				result = expr(left) == expr(right);
				break;
			case BO_NE:
				result = expr(left) != expr(right);
				break;
			case BO_Add:
				result = expr(left) + expr(right);
				break;
			case BO_Sub:
				result = expr(left) - expr(right);
				break;
			case BO_Mul:
				result = expr(left) * expr(right);
				break;
			case BO_Div:
				result = expr(left) / expr(right);
				break;
			default:
				//undefined bop
				break;
			}

			mStack.back().bindStmt(bop, result);
		}
	}

	void unaryop(UnaryOperator *op, int64_t &val)
	{
		Expr *subExpr = op->getSubExpr();
		switch (op->getOpcode())
		{
		case UO_Minus:
			val = -1 * expr(subExpr);
			break;
		case UO_Plus:
			val = expr(subExpr);
			break;
		case UO_Deref:
			break;

		default:
			break;
		}
	}
	void decl(DeclStmt *declstmt)
	{
		for (DeclStmt::decl_iterator it = declstmt->decl_begin(), ie = declstmt->decl_end();
			 it != ie; ++it)
		{
			Decl *decl = *it;
			if (VarDecl *vardecl = dyn_cast<VarDecl>(decl))
			{
				if (vardecl->getType().getTypePtr()->isIntegerType())
				{
					if (vardecl->hasInit()) //add init handler,only for int
					{
						if (auto intLiteral = dyn_cast<IntegerLiteral>(vardecl->getInit()))
							mStack.back().bindDecl(vardecl, intLiteral->getValue().getSExtValue());
					} 
					else
					{
						//default value
						mStack.back().bindDecl(vardecl, DECL_DEFAULT_VALUE);
					}
				}
				else if (vardecl->getType().getTypePtr()->isArrayType())
				{
					//
					std::cout << "Array Type" << std::endl;
					if (auto array = dyn_cast<ConstantArrayType>(vardecl->getType().getTypePtr()))
					{
						int64_t size = array->getSize().getSExtValue();
						if (array->getElementType().getTypePtr()->isIntegerType())
						{
							if (!vardecl->hasInit())
							{ //the homework hasn't init situation
								mStack.back().createIntVec(size);
								mStack.back().bindDecl(vardecl, mStack.back().getLastVecLoc());
							}
						}
					}
				}
			}
		}
	}
	void declref(DeclRefExpr *declref)
	{
		mStack.back().setPC(declref);
		if (declref->getType()->isIntegerType())
		{
			Decl *decl = declref->getFoundDecl();

			int val = mStack.back().getDeclVal(decl);
			mStack.back().bindStmt(declref, val);
		}
		else if (declref->getType()->isArrayType())
		{
			cout << "declref array type" << endl;

			/*Decl *decl = declref->getFoundDecl();

			int val = mStack.back().getDeclVal(decl);
			mStack.back().bindStmt(declref, val);*/
		}
		else
		{
		}
	}

	void cast(CastExpr *castexpr)
	{
		mStack.back().setPC(castexpr);
		if (castexpr->getType()->isIntegerType())
		{ 
			Expr *expr = castexpr->getSubExpr();
			int val = mStack.back().getStmtVal(expr);
			mStack.back().bindStmt(castexpr, val);
		}
	}

	bool isBuiltInFunc(CallExpr *callexpr)
	{
		FunctionDecl *callee = callexpr->getDirectCallee();
		if (callee == mInput || callee == mOutput || callee == mMalloc || callee == mFree)
			return true;

		return false;
	}

	/// !TODO Support Function Call
	void callBuiltIn(CallExpr *callexpr)
	{
		//std::cout << "callBuiltIn" << std::endl;
		mStack.back().setPC(callexpr);
		int val = 0;
		FunctionDecl *callee = callexpr->getDirectCallee();
		if (callee == mInput)
		{
			llvm::errs() << "Please Input an Integer Value : ";
			scanf("%d", &val);

			mStack.back().bindStmt(callexpr, val);
		}
		else if (callee == mOutput)
		{
			Expr *decl = callexpr->getArg(0); //获取第一个参数
			val = mStack.back().getStmtVal(decl);
			llvm::errs() << val;
		}
		else if (callee == mMalloc)
		{
		}
		else if (callee == mFree)
		{
		}
	}
	void callCustom(CallExpr *callexpr)
	{
		vector<int64_t> args;

		for (auto i = callexpr->arg_begin(), e = callexpr->arg_end(); i != e; i++)
		{
			args.push_back(expr(*i));
		}
		mStack.push_back(StackFrame());
		int j = 0;
		FunctionDecl *callee = callexpr->getDirectCallee();
		for (auto i = callee->param_begin(), e = callee->param_end(); i != e; i++, j++)
		{
			mStack.back().bindDecl(*i, args[j]);
		}
	}
	void callCustomFinished()
	{
		if (!mStack.empty())
			mStack.pop_back();
	}
	int64_t getCallReturn()
	{
		return mStack.back().getReturnValue();
	}

	void pushStmVal(Stmt *stmt, int64_t retvalue)
	{
		mStack.back().bindStmt(stmt, retvalue);
	}
	void returnstmt(ReturnStmt *stmt)
	{
		int64_t value = expr(stmt->getRetValue());

		mStack.back().setReturn(value);
	}

	int64_t expr(Expr *exp)
	{
		exp = exp->IgnoreImpCasts();
		if (auto intLiteral = dyn_cast<IntegerLiteral>(exp))
		{ // 1;
			llvm::APInt result = intLiteral->getValue();
			return result.getSExtValue();
		}
		else if (auto callExpr = dyn_cast<CallExpr>(exp))
		{ // f();
			return mStack.back().getStmtVal(callExpr);
		}
		else if (auto binaryExpr = dyn_cast<BinaryOperator>(exp))
		{ //+ - * / < > == expr
			binop(binaryExpr);
			int64_t result = mStack.back().getStmtVal(binaryExpr);
			return result;
		}
		else if (auto declRef = dyn_cast<DeclRefExpr>(exp))
		{ // a
			declref(declRef);
			int64_t result = mStack.back().getStmtVal(declRef);
			return result;
		}
		else if (auto unaryExpr = dyn_cast<UnaryOperator>(exp))
		{

			int64_t result;
			unaryop(unaryExpr, result);
			return result;
		}
		else if (auto arraySubscript = dyn_cast<ArraySubscriptExpr>(exp))
		{
			int64_t result;
			int64_t index = expr(arraySubscript->getRHS());
			if (DeclRefExpr *declexpr = dyn_cast<DeclRefExpr>(arraySubscript->getLHS()->IgnoreImpCasts()))
			{
				Decl *decl = declexpr->getFoundDecl();
				int loc = mStack.back().getDeclVal(decl);
				result=mStack.back().getVecValue(loc,index);
				 
			}
			return result;
		}
		else
		{
			//bind 0 if not include
			return 0;
		}
	}
};
