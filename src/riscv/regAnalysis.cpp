#include "regAnalysis.h"
#include "LoopInfo.h"
#include "regalloc.h"
#include "riscv.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#define INT_MAX (0x7fffffff)

void RegAnalysis::LifetimeAnalysis() {
  // Get Basic Block Order
  bb_order = regAlloca->loopInfo->getAnalysisOrder();
  // Reverse basic block order.
  auto bb_order_rev = bb_order;
  // Check if value is valid type.
  auto checkValidType = [](Value *val) {
    switch (val->type_->tid_) {
    case Type::VoidTyID:
    case Type::LabelTyID:
    case Type::FunctionTyID:
      return false;
    case Type::IntegerTyID:
    case Type::FloatTyID:
    case Type::ArrayTyID:
    case Type::PointerTyID:
      break;
    }
    return !val->is_constant();
  };
  std::reverse(bb_order_rev.begin(), bb_order_rev.end());

  //! Debug. Delete after released.
#ifdef DEBUG
  std::cerr << "Analysis order:" << std::endl;
  for (auto *bb : bb_order)
    std::cerr << bb->name_ << ' ';
  std::cerr << "\n";
#endif

  // Initialize instruction index and ADS.
  instr_ind = 0;
  for (auto *bb : bb_order) {
    auto &ads = ADS_map[bb];
    ads.from = instr_ind + 1;
#ifdef DEBUG
    std::cerr << "----" << bb->name_ << "----\n";
#endif
    for (auto *instr : bb->instr_list_) {
      instr2ind[instr] = ++instr_ind;
#ifdef DEBUG
      std::cerr << std::to_string(instr_ind) << ": \t" << instr->print()
                << std::endl;
#endif
    }
    ads.to = instr_ind;
  }

  // Initialize life_intervals.
  for (auto *bb : bb_order)
    for (auto *instr : bb->instr_list_) {
      for (auto *opd : instr->operands_)
        if (checkValidType(opd))
          life_intervals[opd].val = opd;
      if (checkValidType(instr))
        life_intervals[instr].val = instr;
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
      int instr_index = instr2ind[*instr_it];
      // For input operand
      for (auto *opd : (*instr_it)->operands_)
        if (checkValidType(opd))
          life_intervals[opd].addRange(bb_ads.from, instr_index);
      // For output operand
      if (checkValidType(*instr_it))
        life_intervals[*instr_it].setFrom(instr_index);
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
        if (checkValidType(opd))
          life_intervals[opd].checkValid();
      if (checkValidType(instr))
        life_intervals[instr].checkValid();
    }

#ifdef DEBUG
  std::cerr << "===Lifetime Analysis Result:===\n";
  std::set<Value *> opds;
  for (auto *bb : bb_order)
    for (auto *instr : bb->instr_list_) {
      for (auto *opd : instr->operands_)
        if (checkValidType(opd))
          opds.insert(opd);
      if (checkValidType(instr))
        opds.insert(instr);
    }
  for (auto *val : opds) {
    std::cerr << "Value : " << val->print() << std::endl;
    std::cerr << "\tLife Intervals : \n\t";
    std::cerr << life_intervals[val].print() << std::endl;
  }
#endif
}

void RegAnalysis::LinearScanRegAlloca() {
  unhandled.clear();
  active.clear();
  inactive.clear();
  handled.clear();

  // Obtain all unhandled intervals.
  for (auto interval : life_intervals)
    unhandled.insert(interval.second);

  while (!unhandled.empty()) {
    auto current = *unhandled.begin();
    unhandled.erase(unhandled.begin());
    int position = current.begin();

    std::vector<LifeInterval> to_be_removed;
    // Check for intervals in active that are handled or inactive
    for (auto it : active) {
      if (it.end() < position) {
        handled.insert(it);
        to_be_removed.push_back(it);
      } else if (!it.cover(position)) {
        inactive.insert(it);
        to_be_removed.push_back(it);
      }
    }
    for (auto it : to_be_removed)
      active.erase(it);
    to_be_removed.clear();

    // Check for intervals in inactive that are handled or active
    for (auto it : inactive) {
      if (it.end() < position) {
        handled.insert(it);
        to_be_removed.push_back(it);
      } else if (it.cover(position)) {
        active.insert(it);
        to_be_removed.push_back(it);
      }
    }
    for (auto it : to_be_removed)
      inactive.erase(it);
    to_be_removed.clear();

    // Find a register for current.
    bool result = TryAllocateFreeReg(current);

    // If allocation failed
    if (!result) {
      result = AllocateBlockedReg(current);
      // assert(result);
    }

    if (current.reg != nullptr) {
      active.insert(current);
    }
  }

#ifdef DEBUG
  std::cerr << "====LinearScan Register Allocation Result====\n";
  std::cerr << "Unhandled Count (Should be zero): "
            << std::to_string(unhandled.size()) << std::endl;
  handled.merge(active);
  handled.merge(inactive);
  for (auto it : handled) {
    std::cerr << "Value: " << it.val->print() << std::endl;
    std::cerr << "\tInterval: " << it.print()
              << "\n\tRegister: " << it.reg->print()
              << "\n\tSplitted: " << std::to_string(it.splitted) << std::endl;
  }
#endif
}

bool RegAnalysis::TryAllocateFreeReg(LifeInterval &current) {
  auto regAvailable = [=](RiscvOperand *reg) {
    return regIntAvailable.find(reg) != regIntAvailable.end();
  };
  // Init regPool freeUntilNextPos
  freeUntilPos.clear();
  for (auto *reg : regPool)
    if (regAvailable(reg))
      freeUntilPos[reg] = INT_MAX;
  for (auto it : active)
    freeUntilPos[it.reg] = 0;

  if (current.splitted)
    for (auto it : inactive) {
      int it_time = it.intersect(current);
      if (it_time)
        freeUntilPos[it.reg] = it_time;
    }

  // Registers to allocate.
  RiscvOperand *alloc_reg = nullptr;
  for (auto *reg : regPool)
    if (regAvailable(reg)) {
      if (alloc_reg == nullptr || freeUntilPos[reg] > freeUntilPos[alloc_reg])
        alloc_reg = reg;
    }

  if (freeUntilPos[alloc_reg] == 0) {
    // Allocation failed.
    return false;
  }
  if (current.end() < freeUntilPos[alloc_reg]) {
    current.reg = alloc_reg;
  } else {
    current.reg = alloc_reg;
    LifeInterval new_it = current.splitInterval(freeUntilPos[alloc_reg] - 1);
    unhandled.insert(new_it);
  }
  return true;
}

bool RegAnalysis::AllocateBlockedReg(LifeInterval &current) { return false; }

RegAnalysis::RegAnalysis(RegAlloca *_regAlloca) : regAlloca(_regAlloca) {
  if (regAlloca->foo->basic_blocks_.size() == 0)
    return;
  // Initlizize registers available
  for (int i = 1; i <= 11; i++)
    regIntAvailable.insert(getRegOperand("s" + std::to_string(i)));

  LifetimeAnalysis();
  LinearScanRegAlloca();
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
  std::vector<Interval> _intervals;
  for (auto inv : intervals)
    _intervals.push_back(inv);
  auto sort_function = [](Interval a, Interval b) {
    return a.first == b.first ? a.second > b.second : a.first < b.first;
  };
  std::sort(_intervals.begin(), _intervals.end(), sort_function);
  for (int i = 0, l = _intervals.size(); i + 1 < l; i++) {
    // Containing interval
    if (_intervals[i].second >= _intervals[i + 1].second) {
      _intervals[i + 1] = _intervals[i];
    }
    // Intersecting interval
    else if (_intervals[i].second >= _intervals[i + 1].first) {
      _intervals[i + 1].first = _intervals[i].first;
      _intervals[i] = _intervals[i + 1];
    }
  }

  // Re-insert corrected intervals.
  intervals.clear();
  for (auto interval : _intervals)
    intervals.insert(interval);
}

RegAnalysis::LifeInterval
RegAnalysis::LifeInterval::splitInterval(int split_time) {
  Interval *interval_to_split = nullptr;
  LifeInterval new_it = *this;
  for (auto interval : intervals) {
    if (interval.first <= split_time && split_time <= interval.second) {
      interval_to_split = &interval;
      break;
    }
    new_it.intervals.erase(interval);
  }
  assert(interval_to_split != nullptr);
  intervals.erase(*interval_to_split);
  intervals.insert({interval_to_split->first, split_time});
  new_it.intervals.insert({split_time + 1, interval_to_split->second});
  return new_it;
}

bool RegAnalysis::LifeInterval::cover(int time) {
  for (auto it : intervals)
    if (time >= it.first && time <= it.second)
      return true;
  return false;
}

int RegAnalysis::LifeInterval::intersect(Interval _it) {
  auto it = intervals.lower_bound(_it);
  auto _itb = *it;
  return _it.intersect(_itb);
}

int RegAnalysis::LifeInterval::intersect(LifeInterval _it) {
  for (auto it : _it.intervals) {
    int it_time = intersect(it);
    if (it_time)
      return it_time;
  }
  return 0;
}
