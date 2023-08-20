#pragma once
#ifndef REGANALYSISH
#define REGANALYSISH

#include "regalloc.h"
#include "LoopInfo.h"
#include "riscv.h"

class RegAnalysis {
private:
  // Related regAlloca.
  RegAlloca *regAlloca;

  // Instruction Index.
  int instr_ind;

  // Map from instr to index.
  std::unordered_map<Instruction *, int> instr2ind;

  // Basic block analysis order.
  std::vector<BasicBlock *> bb_order;

  struct LifeInterval {
    std::set<std::pair<int, int>> intervals;

    // Set the start position of the first range.
    void setFrom(int index);

    // Add a range to intervals.
    void addRange(int L, int R);

    // Check if the intervals are valid ones.
    void checkValid();
  };

  // Analysis Data Structure for Basic Block.
  struct AnalysisDS {
    std::unordered_set<Value *> liveIn;

    // Instruction interval.
    int from, to;
  };

  // ADS Map.
  std::unordered_map<BasicBlock *, AnalysisDS> ADS_map;

  // Value Life Interval Map.
  std::unordered_map<Value *, LifeInterval> life_intervals;

public:
  /**
   * @brief 计算 Function 所关联变量的 Life Interval 。
   */
  void LifetimeAnalysis();

  /**
   * @brief 线性寄存器分配算法。
   */
  void LinearScanRegAlloca();

  RegAnalysis(RegAlloca *_regAlloca);
};

#endif