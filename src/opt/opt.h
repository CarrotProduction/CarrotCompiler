// Currently a dummy file
#ifndef OPTH
#define OPTH

#include "ir.h"

class Optimization {
public:
  Module *m;
  explicit Optimization(Module *m_) : m(m_) {}
  virtual void execute() = 0;
};

class DomainTree : public Optimization {
  std::vector<BasicBlock *> reversePostTraverse;
  std::map<BasicBlock *, int> TraverseInd;
  std::vector<BasicBlock *> doms;

public:
  DomainTree(Module *m) : Optimization(m) {}
  void execute();
  void getReversePostTraverse(Function *foo);
  std::vector<BasicBlock *> postTraverse(BasicBlock *bb);
  void getBlockDom(Function *foo);
  void getBlockDomFront(Function *foo);
  BasicBlock *intersect(BasicBlock *b1, BasicBlock *b2);
  bool isLoopEdge(BasicBlock *a, BasicBlock *b);
};

class ReverseDomainTree : public Optimization {
  std::map<BasicBlock *, int> reverseTraverseInd;
  std::vector<BasicBlock *> reverseDomainBlock;
  std::vector<BasicBlock *> reverseTraverse;
  BasicBlock *exitBlock;

public:
  ReverseDomainTree(Module *m) : Optimization(m), exitBlock(nullptr) {}
  void execute();
  BasicBlock *intersect(BasicBlock *b1, BasicBlock *b2);
  void getReversePostTraverse(Function *foo);
  void getBlockDomR(Function *foo);
  void getBlockRdoms(Function *foo);
  void getBlockDomFrontR(Function *foo);
  void getPostTraverse(BasicBlock *bb, std::set<BasicBlock *> &visited);
};
#endif // !OPTH