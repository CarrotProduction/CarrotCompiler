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
  std::vector<Loop *> loops;

  LoopInfo(Function *_foo);
  /**
   * @brief 返回一个 Basic Block 所属的循环。
   * 若循环不存在，则返回 nullptr 。
   * @param bb 查询的 Basic Block 对象。
   * @return Loop* BB 所属的循环。
   */
  Loop *getBBLoop(BasicBlock *bb);
  void printAllLoopInfo();

  /**
   * @brief 获取用于寄存器分析的最优 Basic Block 顺序。
   *
   * @return std::vector<BasicBlock *>
   */
  std::vector<BasicBlock *> getAnalysisOrder();

  /**
   * @brief 返回 BasicBlock 是否为某个循环的 Header.
   */
  bool isLoopHeader(BasicBlock *bb);

private:
  // Below are copied from LoopInvariant.cpp

  std::unordered_set<BasicBlock *> AODFS_Flag;
  void AnalysisOrderDFS(std::vector<BasicBlock *> &ret_order, BasicBlock *bb);
  bool searchSCC(std::set<node *> &basicBlock,
                 std::set<std::set<node *> *> &SCCs);
  void tarjan(node *cur, std::set<std::set<node *> *> &SCCs);
  std::stack<node *> tarjanStack;
  void mapBBtoLoop(BasicBlock *bb, Loop *loop);
  void loopAnalysis();
  int ind; // scc index
};