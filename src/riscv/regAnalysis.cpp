#include "regAnalysis.h"
#include "LoopInfo.h"
#include <algorithm>
#include <unordered_map>
#include <vector>

void RegAnalysis::LifetimeAnalysis() {
  // Get Basic Block Order
  bb_order = regAlloca->loopInfo->getAnalysisOrder();
  // Reverse basic block order.
  auto bb_order_rev = bb_order;
  std::reverse(bb_order_rev.begin(), bb_order_rev.end());

  //! Debug. Delete after released.
  std::cerr << "Analysis order:" << std::endl;
  for (auto *bb : bb_order)
    std::cerr << std::hex << bb << ' ';
  std::cerr << "\n";

  // Initialize instruction index and ADS.
  instr_ind = 0;
  for (auto *bb : bb_order) {
    auto &ads = ADS_map[bb];
    ads.from = instr_ind + 1;
    for (auto *instr : bb->instr_list_)
      instr2ind[instr] = ++instr_ind;
    ads.to = instr_ind;
  }

  // Start build intervals.
  for (BasicBlock *bb : bb_order_rev) {
    std::unordered_set<Value *> live;
    auto bb_ads = ADS_map[bb];

    // Union successors liveIn set.
    for (auto *suc : bb->succ_bbs_) {
      auto ads = ADS_map[suc];
      for (auto *lives : ads.liveIn)
        live.insert(lives);
      // Get successors' phi function operands related to bb.
      for (auto *instr : bb->instr_list_)
        if (instr->is_phi()) {
          for (int i = 1; i < instr->operands_.size(); i += 2)
            if (instr->get_operand(i) == bb) {
              live.insert(instr->get_operand(i - 1));
            }
        }
    }

    // Add bb's life range for operands in live.
    for (auto *opd : live) {
      life_intervals[opd].addRange(bb_ads.from, bb_ads.to);
    }

    // For each instruction of bb in reverse order
    for (auto instr_it = bb->instr_list_.rbegin();
         instr_it != bb->instr_list_.rend(); instr_it++) {
      // For input operand
      for (auto *opd : (*instr_it)->operands_)
        life_intervals[opd].addRange(bb_ads.from, instr2ind[*instr_it]);
      // For output operand
      life_intervals[*instr_it].setFrom(instr2ind[*instr_it]);
    }

    // Remove Phi Function's output value from bb's liveIn set.
    for (auto *instr : bb->instr_list_)
      if (instr->is_phi()) {
        live.erase(instr);
      }

    // If bb is loop header then
    if (regAlloca->loopInfo->isLoopHeader(bb)) {
      int loopEnd = 0;
      auto *loop = regAlloca->loopInfo->getBBLoop(bb);
      // Get loopEnd
      for (auto *lbb : loop->loop_nodes) {
        loopEnd = std::max(loopEnd, ADS_map[lbb].to);
      }
      // Set each operands' interval in live to all loop
      for (auto *opd : live)
        life_intervals[opd].addRange(bb_ads.from, loopEnd);
    }
  }

  // Interval postcheck.
  for (auto *bb : bb_order)
    for (auto *instr : bb->instr_list_) {
      for (auto *opd : instr->operands_)
        life_intervals[opd].checkValid();
      life_intervals[instr].checkValid();
    }
}

RegAnalysis::RegAnalysis(RegAlloca *_regAlloca) : regAlloca(_regAlloca) {
  if (regAlloca->foo->basic_blocks_.size() == 0)
    return;
  LifetimeAnalysis();
}

void RegAnalysis::LifeInterval::setFrom(int index) {
  if (intervals.size() == 0) {
    intervals.insert({index, index});
  }
  auto inter = *intervals.begin();
  intervals.erase(inter);
  inter.first = index;
  intervals.insert(inter);
}

void RegAnalysis::LifeInterval::addRange(int L, int R) {
  intervals.insert({L, R});
}

void RegAnalysis::LifeInterval::checkValid() {
  std::vector<std::pair<int, int>> _intervals;
  for (auto inv : intervals)
    _intervals.push_back(inv);
  auto sort_function = [](std::pair<int, int> a, std::pair<int, int> b) {
    return a.first == b.first ? a.second > b.second : a.first < b.first;
  };
  std::sort(_intervals.begin(), _intervals.end(), sort_function);
  for(int i=0, l=_intervals.size(); i+1<l; i++) {
    // Containing interval
    if(_intervals[i].second >= _intervals[i+1].second) {
      _intervals[i+1] = _intervals[i];
    }
    // Intersecting interval
    else if(_intervals[i].second >= _intervals[i+1].first) {
      _intervals[i+1].first = _intervals[i].first;
      _intervals[i] = _intervals[i+1];
    }
  }

  // Re-insert corrected intervals.
  intervals.clear();
  for(auto interval: _intervals)
    intervals.insert(interval);
}
