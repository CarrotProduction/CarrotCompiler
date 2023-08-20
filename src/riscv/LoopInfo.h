#pragma once
#include "LoopInvariant.h"
#include "ir.h"
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class node;

/**
 * @brief 描述一个由基本块组成的循环。
 *
 */
class Loop {
public:
  std::set<BasicBlock *> loop_nodes;
  BasicBlock *loop_header;

  void insert(BasicBlock *bb);
  void erase(BasicBlock *bb);
  void setHeader(BasicBlock *bb);
  BasicBlock *getHeader();

  void printInfo();
};

/**
 * @brief 描述一个函数包含的循环信息。
 *
 */
class LoopInfo {
public:
  /// Loop info related function.
  Function *foo;
  std::unordered_map<BasicBlock *, Loop *> bb2loop;
  std::unordered_set<Loop *> loops;

  LoopInfo(Function *_foo);
  Loop *getBBLoop(BasicBlock *bb);
  void printAllLoopInfo();

private:
  // Below are copied from LoopInvariant.cpp
  bool searchSCC(std::set<node *> &basicBlock,
                 std::set<std::set<node *> *> &SCCs);
  void tarjan(node *cur, std::set<std::set<node *> *> &SCCs);
  std::stack<node *> tarjanStack;
  void mapBBtoLoop(BasicBlock *bb, Loop *loop);
  void loopAnalysis();
  int ind; // scc index
};