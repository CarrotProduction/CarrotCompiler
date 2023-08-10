#ifndef SIMPLIFYJUMPH
#define SIMPLIFYJUMPH
#include "opt.h"

class SimplifyJump : public Optimization {

public:
  SimplifyJump(Module *m) : Optimization(m) {}
  void execute();
  void deleteUselessBlock(Function *foo,
                          std::vector<BasicBlock *> &uselessBlock);
  bool checkUselessJump(BasicBlock *bb);
  void deleteUselessPhi(Function *foo);
  void deleteUselessJump(Function *foo);
  void mergePreBlock(Function *foo);
  void deleteUnReachableBlock(Function *foo);
};

#endif // !SIMPLIFYJUMPH