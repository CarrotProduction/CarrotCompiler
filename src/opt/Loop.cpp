#include "Loop.h"
bool add_new_loop = false;
Loop *curloop = nullptr;
void LoopSearch::execute() {
  for (auto fun : module->function_list_) {
    if (fun->is_declaration())
      continue;
    cur_fun = fun;
    index = 0;
    std::set<BasicBlock *> bb_set;
    for (auto bb : fun->basic_blocks_) {
      bb_set.insert(bb);
    }
    curloop = nullptr;
    Tarjan(fun->basic_blocks_[0], bb_set);
    add_new_loop = false;
    while (!work_list.empty()) {
      auto loop = work_list.back();
      curloop = loop;
      work_list.pop_back();
      if (loop->get_loop().size() == 1)
        continue;
      std::set<BasicBlock *> loop_bb;
      for (auto bb : loop->get_loop()) {
        loop_bb.insert(bb);
      }
      loop_bb.erase(loop->get_loop_entry());
      index = 0;
      DFN.clear();
      Low.clear();
      in_stack.clear();
      visited.clear();
      for (auto succ : loop->get_loop_entry()->succ_bbs_) {
        if (loop_bb.find(succ) != loop_bb.end()) {
          Tarjan(succ, loop_bb);
          break;
        }
      }
    }
    for (auto loop : fun_loop[cur_fun])
      build_pre_succ_relation(loop);
  }
  build_map();
}
Loop *LoopSearch::get_inner_loop(const std::set<Loop *> &loops,
                                 BasicBlock *bb) {
  if (loops.empty())
    return nullptr;
  for (auto loop : loops) {
    if (loop->contain_bb(bb)) {
      auto inner_loop = get_inner_loop(get_child_loop(loop), bb);
      if (inner_loop != nullptr)
        return inner_loop;
      else
        return loop;
    }
  }
  return nullptr;
}

void LoopSearch::build_map() {
  for (auto f : module->function_list_) {
    if (f->is_declaration())
      continue;
    std::map<BasicBlock *, Loop *> _sub_map;
    for (auto bb : f->basic_blocks_)
      _sub_map[bb] = get_inner_loop(get_loop(f), bb);
    _nested_loop[f] = _sub_map;
  }
}

void LoopSearch::Tarjan(BasicBlock *bb, std::set<BasicBlock *> blocks) {
  DFN[bb] = index;
  Low[bb] = index;
  index++;
  loop_stack.push(bb);
  in_stack.insert(bb);
  visited.insert(bb);
  for (auto succ_bb : bb->succ_bbs_) {
    if (blocks.find(succ_bb) == blocks.end())
      continue;
    if (visited.find(succ_bb) == visited.end()) {
      Tarjan(succ_bb, blocks);
      Low[bb] = std::min(Low[bb], Low[succ_bb]);
    } else if (in_stack.find(succ_bb) != in_stack.end()) {
      Low[bb] = std::min(Low[bb], DFN[succ_bb]);
    }
  }
  if (DFN[bb] == Low[bb]) {
    BasicBlock *v = loop_stack.top();
    loop_stack.pop();
    in_stack.erase(v);
    Loop *new_loop = new Loop();
    new_loop->add_loop_block(v);
    while (bb != v) {
      v = loop_stack.top();
      loop_stack.pop();
      in_stack.erase(v);
      new_loop->add_loop_block(v);
    }
    new_loop->set_entry_block(bb);
    if (new_loop->get_loop().size() > 1) {
      fun_loop[cur_fun].insert(new_loop);
      work_list.push_back(new_loop);
      if (curloop != nullptr) {
        child_loop[curloop].insert(new_loop);
      }
    } else if (new_loop->get_loop().size() == 1) {
      for (auto succ_bb : bb->succ_bbs_) {
        if (succ_bb == bb) {
          fun_loop[cur_fun].insert(new_loop);
          work_list.push_back(new_loop);
          if (curloop != nullptr) {
            child_loop[curloop].insert(new_loop);
          }
          break;
        }
      }
    }
  }
}

void LoopSearch::build_pre_succ_relation(Loop *loop) {
  auto bb_set = loop->get_loop();
  for (auto bb : bb_set) {
    for (auto succ_bb : bb->succ_bbs_) {
      if (bb_set.find(succ_bb) != bb_set.end()) {
        loop->add_loop_bb_succ(bb, succ_bb);
      }
    }
    for (auto pre_bb : bb->pre_bbs_) {
      if (bb_set.find(pre_bb) != bb_set.end()) {
        loop->add_loop_bb_pre(bb, pre_bb);
      }
    }
  }
}
