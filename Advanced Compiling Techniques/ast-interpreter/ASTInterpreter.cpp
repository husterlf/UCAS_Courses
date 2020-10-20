//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//
#include <iostream>
#include <fstream>
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

#include "Environment.h"

class InterpreterVisitor : public EvaluatedExprVisitor<InterpreterVisitor>
{
public:
   explicit InterpreterVisitor(const ASTContext &context, Environment *env)
       : EvaluatedExprVisitor(context), mEnv(env) {} //this class provide visit interfaces
   virtual ~InterpreterVisitor() {}

   virtual void VisitBinaryOperator(BinaryOperator *bop)
   {
      if (mEnv->isReturned())
         return;
      VisitStmt(bop);
      mEnv->binop(bop);
   }
   virtual void VisitDeclRefExpr(DeclRefExpr *expr)
   {
      if (mEnv->isReturned())
         return;
      VisitStmt(expr);
      mEnv->declref(expr);
   }
   virtual void VisitCastExpr(CastExpr *expr)
   {
      if (mEnv->isReturned())
         return;
      VisitStmt(expr);
      mEnv->cast(expr);
   }
   virtual void VisitCallExpr(CallExpr *call)
   {
      if (mEnv->isReturned())
         return;
      VisitStmt(call);
      if (mEnv->isBuiltInFunc(call))
         mEnv->callBuiltIn(call);
      else
      {
         if (FunctionDecl *fdecl = call->getDirectCallee())
         {
            //custom func with a new stack
            mEnv->callCustom(call);
            Visit(fdecl->getBody());
            if (fdecl->isNoReturn())
               mEnv->callCustomFinished();
            else
            {
               int retvalue = mEnv->getCallReturn();
               mEnv->callCustomFinished();
               mEnv->pushStmVal(call, retvalue);
            }
         }
      }
   }
   virtual void VisitDeclStmt(DeclStmt *declstmt)
   {
      if (mEnv->isReturned())
         return;
      mEnv->decl(declstmt);
   }
   virtual void VisitReturnStmt(ReturnStmt *returnStmt)
   {
      if (mEnv->isReturned())
         return;
      Visit(returnStmt->getRetValue());
      mEnv->returnstmt(returnStmt);
   }
   virtual void VisitIfStmt(IfStmt *ifstmt)
   {
      if (mEnv->isReturned())
         return;
      Expr *cond = ifstmt->getCond();
      if (mEnv->expr(cond))
      { // Current cond is True
         Stmt *thenstmt = ifstmt->getThen();
         Visit(thenstmt);
      }
      else
      {
         if (ifstmt->getElse())
         { //find next Cond
            Stmt *elsestmt = ifstmt->getElse();
            Visit(elsestmt);
         }
      }
   }

   virtual void VisitWhileStmt(WhileStmt *stmt)
   {
      if (mEnv->isReturned())
         return;
      //std::cout<<"while stms:"<<std::endl;
      Expr *cond = stmt->getCond();
      while (mEnv->expr(cond))
      {
         Visit(stmt->getBody());
      }
   }

   virtual void VisitForStmt(ForStmt *stmt)
   {
      if (mEnv->isReturned())
         return;
      //Init,Cond,Inc   Body
      Stmt *initStmt = stmt->getInit();
      if (initStmt)
         Visit(initStmt);
      while (mEnv->expr(stmt->getCond()))
      {
         Visit(stmt->getBody());
         Visit(stmt->getInc());
      }
      int kk = 0;
   }

private:
   Environment *mEnv;
};

class InterpreterConsumer : public ASTConsumer
{
public:
   explicit InterpreterConsumer(const ASTContext &context) : mEnv(),
                                                             mVisitor(context, &mEnv)
   {
   }
   virtual ~InterpreterConsumer() {}

   virtual void HandleTranslationUnit(clang::ASTContext &Context)
   {
      TranslationUnitDecl *decl = Context.getTranslationUnitDecl();
      mEnv.init(decl);

      FunctionDecl *entry = mEnv.getEntry();
      mVisitor.VisitStmt(entry->getBody());
   }

private:
   Environment mEnv;
   InterpreterVisitor mVisitor;
};

class InterpreterClassAction : public ASTFrontendAction
{
public:
   virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
       clang::CompilerInstance &Compiler, llvm::StringRef InFile)
   {
      return std::unique_ptr<clang::ASTConsumer>(
          new InterpreterConsumer(Compiler.getASTContext()));
   }
};

std::string exec(const char *cmd)
{
   FILE *pipe = popen(cmd, "r");
   if (!pipe)
      return "ERROR";
   char buffer[128];
   std::string result = "";
   while (!feof(pipe))
   {
      if (fgets(buffer, 128, pipe) != NULL)
         result += buffer;
   }
   pclose(pipe);
   return result;
}

int main(int argc, char **argv)
{
   if (argc > 1)
   {
      /*std::cout << "input param: " << argv[1] << std::endl;

      std::string s = exec(argv[1]);
      std::cout << "code content: \n\n"
                << s << std::endl;

      clang::tooling::runToolOnCode(std::unique_ptr<clang::FrontendAction>(new InterpreterClassAction), s);
*/
      if (argc > 1)
      {
         clang::tooling::runToolOnCode(std::unique_ptr<clang::FrontendAction>(new InterpreterClassAction), argv[1]);
      }
   }
}
