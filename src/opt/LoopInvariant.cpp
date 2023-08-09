#include "LoopInvariant.h"

void LoopInvariant::execute() {
  search();
  while (!loopBBStack.empty()) {
    auto loop = loopBBStack.top();
    loopBBStack.pop();
    std::string bbname_in_loop = "";
    for (auto bb : *loop)
      bbname_in_loop += bb->name_ + " ";
    std::set<Value *> assignVals;
    std::set<Instruction *> mark_invarInsts;
    std::vector<Instruction *> invarInst_vec;
    std::map<Instruction *, BasicBlock *> inst2BB;
    for (auto bb : *loop)
      for (Instruction *inst : bb->instr_list_)
        if (inst->is_binary() || inst->is_cmp() || inst->is_fcmp() ||
            inst->is_call() || inst->is_phi() || inst->is_zext() ||
            inst->is_fptosi() || inst->is_sitofp() || inst->is_gep() ||
            inst->is_load())
          assignVals.insert(inst);
    bool changed = true;
    while (changed) {
      changed = false;
      for (auto bb : *loop) {
        for (auto inst : bb->instr_list_) {
          if (mark_invarInsts.find(inst) != mark_invarInsts.end())
            continue;
          if (!(inst->is_br() || inst->is_ret() || inst->is_phi() ||
                inst->is_alloca() || inst->is_gep() || inst->is_store() ||
                inst->is_load() ||
                (inst->is_call() &&
                 inst->get_operand(inst->num_ops_ - 1)->print() != "rand"))) {
            bool move = true;
            for (int i = 0; i < (int)inst->num_ops_; i++)
              if (assignVals.find(inst->get_operand(i)) != assignVals.end())
                move = false;
            if (move) {
              assignVals.erase(inst);
              mark_invarInsts.emplace(inst);
              invarInst_vec.emplace_back(inst);
              inst2BB[inst] = bb;
              changed = true;
            }
          }
        }
      }
    }
    auto enter = enterBBMap[loop];
    for (auto prev : enter->pre_bbs_)
      if (loop->find(prev) == loop->end())
        for (auto inst : invarInst_vec)
          prev->add_instruction_before_terminator(inst);
  }
}

void LoopInvariant::search() {
  for (auto foo : m->function_list_) {
    if (foo->basic_blocks_.empty())
      continue;
    std::set<node *> nodes;
    std::set<node *> enters;
    std::set<std::set<node *> *> SCCs;
    std::map<BasicBlock *, node *> bb2node;
    for (auto bb : foo->basic_blocks_) {
      auto cur = new node(bb, -1, -1, 0);
      nodes.insert(cur);
      bb2node[bb] = cur;
    }
    for (auto bb : foo->basic_blocks_) {
      auto node = bb2node[bb];
      for (auto succ_bb : bb->succ_bbs_)
        node->suc.insert(bb2node[succ_bb]);
      for (auto prev_bb : bb->succ_bbs_)
        node->pre.insert(bb2node[prev_bb]);
    }
    while (LoopInvariant::tarjan(nodes, SCCs)) {
      for (auto SCC : SCCs) {
        node *enter = nullptr;
        for (auto node : *SCC)
          for (auto prev : node->pre)
            if (SCC->find(prev) == SCC->end()) {
              enter = node;
            } else if (enters.find(prev) != enters.end()) {
              enter = prev;
            }
        auto loopBBs = new std::set<BasicBlock *>;
        for (auto node : *SCC)
          loopBBs->insert(node->bb);
        enterBBMap[loopBBs] = enter->bb;
        loopBBStack.push(loopBBs);
        enters.insert(enter);
        nodes.erase(enter);
        for (auto prev : enter->pre)
          prev->suc.erase(enter);
        for (auto succ : enter->suc)
          succ->pre.erase(enter);
      }
      for (auto SCC : SCCs)
        delete SCC;
      SCCs.clear();
      for (auto node : nodes) {
        node->dfn = -1;
        node->low = -1;
        node->inStack = false;
      }
    }
    for (auto node : nodes)
      delete node;
  }
}
bool LoopInvariant::tarjan(std::set<node *> &nodes,
                           std::set<std::set<node *> *> &SCCs) {
  ind = 0;
  while (!tarjan_stack.empty())
    tarjan_stack.pop();
  for (auto node : nodes)
    if (node->dfn == -1)
      dfs(node, SCCs);
  return SCCs.size() != 0;
}

void LoopInvariant::dfs(node *cur, std::set<std::set<node *> *> &SCCs) {
  cur->dfn = cur->low = ++ind;
  cur->inStack = true;
  tarjan_stack.push(cur);
  for (auto succ : cur->suc)
    if (succ->dfn == -1) {
      dfs(succ, SCCs);
      if (succ->low < cur->low)
        cur->low = succ->low;
    } else if (succ->inStack && succ->low < cur->low)
      cur->low = succ->low;
  if (cur->dfn == cur->low) {
    if (cur == tarjan_stack.top()) {
      tarjan_stack.pop();
      cur->inStack = false;
      return;
    }
    auto SCC = new std::set<node *>;
    while (true) {
      auto tp = tarjan_stack.top();
      SCC->insert(tp);
      tarjan_stack.pop();
      tp->inStack = false;
      if (tp == cur)
        break;
    }
    SCCs.insert(SCC);
  }
}