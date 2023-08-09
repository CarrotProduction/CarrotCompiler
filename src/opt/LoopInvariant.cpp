#include "LoopInvariant.h"

void LoopInvariant::execute() {
  searchLoop();
  while (!loopStack.empty()) {
    auto loop = loopStack.top();
    loopStack.pop();
    std::set<Value *> assignVals;
    std::set<Instruction *> visInstr; // 标记下这条语句是不是被操作过
    std::vector<Instruction *> invarInstrs; // 存在不变量的语句集合
    std::map<Instruction *, BasicBlock *> instrPos;
    for (auto bb : *loop)
      for (Instruction *inst : bb->instr_list_)
        // 赋值语句做操作
        if (inst->is_binary() || inst->is_cmp() || inst->is_fcmp() ||
            inst->is_call() || inst->is_phi() || inst->is_zext() ||
            inst->is_fptosi() || inst->is_sitofp() || inst->is_gep() ||
            inst->is_load())
          assignVals.insert(inst);
    bool changed = true;
    while (changed) {
      changed = false;
      for (auto bb : *loop) {
        for (auto instr : bb->instr_list_) {
          if (visInstr.find(instr) != visInstr.end())
            continue;
          if (!instr->is_gep() && !instr->is_alloca() && !instr->is_br() &&
              !instr->is_ret() && !instr->is_phi() && !instr->is_store() &&
              !instr->is_load() &&
              !(instr->is_call() &&
                instr->get_operand(instr->num_ops_ - 1)->print() != "rand")) {
            bool move = true;
            // 一个操作数不是不变量就不能动
            for (unsigned int i = 0; i < instr->num_ops_; i++)
              if (assignVals.find(instr->get_operand(i)) != assignVals.end())
                move = false;
            if (move) {
              instrPos[instr] = bb;
              invarInstrs.push_back(instr);
              assignVals.erase(instr);
              visInstr.insert(instr);
              changed = true;
            }
          }
        }
      }
    }
    auto enter = entryPos[loop];
    for (auto prev : enter->pre_bbs_)
      if (loop->find(prev) == loop->end())
        for (auto inst : invarInstrs)
          prev->add_instruction_before_terminator(inst);
  }
}

void LoopInvariant::searchLoop() {
  for (auto foo : m->function_list_) {
    if (foo->basic_blocks_.empty())
      continue;
    std::set<node *> nodes;
    std::set<node *> entry;
    std::set<std::set<node *> *> SCCs;
    std::map<BasicBlock *, node *> nodeMap;
    for (auto bb : foo->basic_blocks_) {
      auto cur = new node(bb, -1, -1, 0);
      nodeMap[bb] = cur;
      nodes.insert(cur);
    }
    for (auto bb : foo->basic_blocks_) {
      auto BlockNode = nodeMap[bb];
      for (auto suc : bb->succ_bbs_)
        BlockNode->suc.insert(nodeMap[suc]);
      for (auto pre : bb->succ_bbs_)
        BlockNode->pre.insert(nodeMap[pre]);
    }

    while (LoopInvariant::searchSCC(nodes, SCCs)) {
      for (auto SCC : SCCs) {
        node *enter = nullptr;
        for (auto curBlock : *SCC)
          for (auto pre : curBlock->pre)
            if (SCC->find(pre) == SCC->end())
              enter = curBlock;
            else if (entry.find(pre) != entry.end())
              enter = pre;

        auto curLoop = new std::set<BasicBlock *>;
        for (auto curBlock : *SCC)
          curLoop->insert(curBlock->bb);
        entryPos[curLoop] = enter->bb;
        loopStack.push(curLoop);
        entry.insert(enter);
        nodes.erase(enter);
        for (auto pre : enter->pre)
          pre->suc.erase(enter);
        for (auto suc : enter->suc)
          suc->pre.erase(enter);
      }
      for (auto SCC : SCCs)
        SCC->clear();
      SCCs.clear();
      for (auto NodeBlock : nodes)
        NodeBlock = new node(nullptr, -1, -1, false);
    }
    for (auto node : nodes)
      delete node;
  }
}

bool LoopInvariant::searchSCC(std::set<node *> &basicBlocks,
                              std::set<std::set<node *> *> &SCCs) {
  ind = 0;
  while (!tarjanStack.empty())
    tarjanStack.pop();
  for (auto cur : basicBlocks)
    if (cur->dfn == -1)
      tarjan(cur, SCCs);
  return SCCs.size() != 0;
}

void LoopInvariant::tarjan(node *cur, std::set<std::set<node *> *> &SCCs) {
  cur->dfn = cur->low = ++ind;
  cur->inStack = true;
  tarjanStack.push(cur);
  for (auto succ : cur->suc)
    if (succ->dfn == -1) {
      tarjan(succ, SCCs);
      if (succ->low < cur->low)
        cur->low = succ->low;
    } else if (succ->inStack && succ->low < cur->low)
      cur->low = succ->low;
  // 找到low=dfn的，出现强连通分量
  if (cur->dfn == cur->low) {
    if (cur == tarjanStack.top()) {
      tarjanStack.pop();
      cur->inStack = false;
      return;
    }
    auto SCC = new std::set<node *>;
    node *tp = nullptr;
    do {
      tp = tarjanStack.top();
      SCC->insert(tp);
      tarjanStack.pop();
      tp->inStack = false;
    } while (tp != cur);
    SCCs.insert(SCC);
  }
}