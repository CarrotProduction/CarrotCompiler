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
  std::stack<node *> tarjan_stack;
  std::stack<std::set<BasicBlock *> *> loopBBStack;
  std::map<std::set<BasicBlock *> *, BasicBlock *> enterBBMap;

public:
  explicit LoopInvariant(Module *m) : Optimization(m) {}
  void search();
  bool tarjan(std::set<node *> &nodes, std::set<std::set<node *> *> &SCCs);
  void dfs(node *pos, std::set<std::set<node *> *> &SCCs);
  void execute();
};

#endif // !LOOPH