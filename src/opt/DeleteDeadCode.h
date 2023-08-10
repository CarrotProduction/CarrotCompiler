#ifndef DELETEDEADCODEH
#define DELETEDEADCODEH

#include "opt.h"

extern std::set<std::string> sysLibFunc;

class DeadCodeDeletion : public Optimization {
  std::map<Function *, std::set<Value *>> funcPtrArgs;
  std::map<Value *, std::vector<Value *>> storePos;
  BasicBlock *exitBlock;
  std::set<Instruction *> uselessInstr;
  std::set<BasicBlock *> uselessBlock;

public:
  DeadCodeDeletion(Module *m) : Optimization(m), exitBlock(nullptr) {}
  void execute();
  void initFuncPtrArg();
  void Init(Function *foo);
  bool checkOpt(Function *foo, Instruction *instr);
  void findInstr(Function *foo);
  void deleteInstr(Function *foo);
};

#endif // !DELETEDEADCODEH