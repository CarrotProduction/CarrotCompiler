#pragma once
#include <string>
#include <type_traits>
#include <unordered_map>
#ifndef REGANALYSISH
#define REGANALYSISH

#include "LoopInfo.h"
#include "regalloc.h"
#include "riscv.h"

class RegAnalysis {
public:
  struct Interval {
    int first, second;

    // Check if two interval intersected.
    int intersect(Interval b) {
      Interval a = *this;
      if (a.first > b.first)
        std::swap(a, b);
      if (a.second >= b.first)
        return b.first;
      return 0;
    }
  };

  struct LifeInterval {
    std::set<Interval> intervals;
    RiscvOperand *reg = nullptr;
    Value *val = nullptr;
    bool splitted = false;

    // Set the start position of the first range.
    void setFrom(int index);

    // Add a range to intervals.
    void addRange(int L, int R);

    // Check if the intervals are valid ones.
    void checkValid();

    // Split the interval in split_time to two intervals.
    LifeInterval splitInterval(int split_time);

    // Return if the interval covers the given time.
    bool cover(int time);

    // Check if intersect with interval it.
    int intersect(Interval it);
    int intersect(LifeInterval it);

    // Get the intervals' begin time
    int begin() {
      assert(intervals.size() > 0);
      return intervals.begin()->first;
    }

    // Get the intervals' end time
    int end() {
      assert(intervals.size() > 0);
      return intervals.rbegin()->second;
    }

    // Print debug info.
    std::string print() {
      std::string result;
      for (auto it : intervals)
        result += "[" + std::to_string(it.first) + "," +
                  std::to_string(it.second) + "]";
      return result;
    }
  };

private:
  // Related regAlloca.
  RegAlloca *regAlloca;

  // Instruction Index.
  int instr_ind;

  // Map from instr to index.
  std::unordered_map<Instruction *, int> instr2ind;

  // Basic block analysis order.
  std::vector<BasicBlock *> bb_order;

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

  // Parts of LinearScan Algorithm
  std::unordered_map<RiscvOperand *, int> freeUntilPos, nextUsePos;
  std::multiset<LifeInterval> unhandled, active, inactive, handled;
  std::unordered_set<RiscvOperand *> regIntAvailable;
  std::unordered_set<RiscvOperand *> regFloatAvailable;
  bool TryAllocateFreeReg(LifeInterval &);
  bool AllocateBlockedReg(LifeInterval &);

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

inline bool operator<(const RegAnalysis::Interval &a,
                      const RegAnalysis::Interval &b) {
  return a.first == b.first ? a.second < b.second : a.first < b.first;
}

inline bool operator==(const RegAnalysis::Interval &a,
                       const RegAnalysis::Interval &b) {
  return a.first == b.first && a.second == b.second;
}

inline bool operator<(const RegAnalysis::LifeInterval &a,
                      const RegAnalysis::LifeInterval &b) {
  if (a.intervals.size() == 0)
    return true;
  if (b.intervals.size() == 0)
    return false;
  return a.intervals.begin()->first < b.intervals.begin()->first;
}

#endif