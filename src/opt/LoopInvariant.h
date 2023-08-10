#ifndef LOOPH
#define LOOPH

#include "BasicOperation.h"

struct node {
  BasicBlock *bb;
  std::set<node *> pre;
  std::set<node *> suc;
  int dfn, low;
  bool inStack;
  node() = default;
  node(BasicBlock *bb_, int dfn_, int low_, bool inStack_)
      : bb(bb_), dfn(dfn_), low(low_), inStack(inStack_) {}
};

class LoopInvariant : public Optimization {
  int ind;
  std::stack<node *> tarjanStack;
  std::stack<std::set<BasicBlock *> *> loopStack;
  std::map<std::set<BasicBlock *> *, BasicBlock *> entryPos;

public:
  LoopInvariant(Module *m) : Optimization(m) {}
  void execute();
  void searchLoop();
  bool searchSCC(std::set<node *> &basicBlock, std::set<std::set<node *> *> &SCCs);
  void tarjan(node *pos, std::set<std::set<node *> *> &SCCs);
};

#endif // !LOOPH