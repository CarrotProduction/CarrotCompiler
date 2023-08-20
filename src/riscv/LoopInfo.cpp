#include "LoopInfo.h"

LoopInfo::LoopInfo(Function *_foo) : foo(_foo) { loopAnalysis(); }

void LoopInfo::AnalysisOrderDFS(std::vector<BasicBlock *> &ret_order,
                                BasicBlock *bb) {
  AODFS_Flag.insert(bb);
  ret_order.push_back(bb);
  auto loop = getBBLoop(bb);
  // If in loop
  if (loop != nullptr) {
    for (auto lbb : loop->loop_nodes)
      if (lbb != bb) {
        AODFS_Flag.insert(lbb);
        ret_order.push_back(lbb);
      }
    for (auto lbb : loop->loop_nodes)
      for (auto suc : lbb->succ_bbs_)
        if (AODFS_Flag.find(suc) == AODFS_Flag.end())
          AnalysisOrderDFS(ret_order, suc);
  } else {
    for (auto suc : bb->succ_bbs_)
      if (AODFS_Flag.find(suc) == AODFS_Flag.end())
        AnalysisOrderDFS(ret_order, suc);
  }
  return;
}

std::vector<BasicBlock *> LoopInfo::getAnalysisOrder() {
  std::vector<BasicBlock *> ret_order;
  AODFS_Flag.clear();
  for (auto bb : foo->basic_blocks_)
    if (bb->pre_bbs_.size() == 0) {
      AnalysisOrderDFS(ret_order, bb);
    }
  assert(ret_order.size() == foo->basic_blocks_.size());
  return ret_order;
}

bool LoopInfo::isLoopHeader(BasicBlock *bb) {
  Loop *loop = getBBLoop(bb);
  if (loop == nullptr)
    return false;
  return bb == loop->getHeader();
}

bool LoopInfo::searchSCC(std::set<node *> &basicBlock,
                         std::set<std::set<node *> *> &SCCs) {
  ind = 0;
  while (!tarjanStack.empty())
    tarjanStack.pop();
  for (auto cur : basicBlock)
    if (cur->dfn == -1)
      tarjan(cur, SCCs);
  return SCCs.size() != 0;
}

void LoopInfo::mapBBtoLoop(BasicBlock *bb, Loop *loop) { bb2loop[bb] = loop; }

void LoopInfo::loopAnalysis() {
  if (foo->basic_blocks_.empty())
    return;
  std::set<node *> nodes;
  std::set<node *> entry;
  std::set<std::set<node *> *> SCCs;
  std::unordered_map<BasicBlock *, node *> nodeMap;
  for (auto bb : foo->basic_blocks_) {
    auto cur = new node(bb, -1, -1, 0);
    nodeMap[bb] = cur;
    nodes.insert(cur);
  }
  for (auto bb : foo->basic_blocks_) {
    auto BlockNode = nodeMap[bb];
    for (auto suc : bb->succ_bbs_)
      BlockNode->suc.insert(nodeMap[suc]);
    for (auto pre : bb->pre_bbs_)
      BlockNode->pre.insert(nodeMap[pre]);
  }

  while (searchSCC(nodes, SCCs)) {
    for (auto SCC : SCCs) {
      node *enter = nullptr;
      for (auto curBlock : *SCC)
        for (auto pre : curBlock->pre)
          if (SCC->find(pre) == SCC->end())
            enter = curBlock;
          else if (entry.find(pre) != entry.end())
            enter = pre;

      auto curLoop = new Loop();
      for (auto curBlock : *SCC) {
        curLoop->insert(curBlock->bb);
        mapBBtoLoop(curBlock->bb, curLoop);
      }
      curLoop->setHeader(enter->bb);
      loops.push_back(curLoop);

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

Loop *LoopInfo::getBBLoop(BasicBlock *bb) {
  if (bb2loop.find(bb) == bb2loop.end())
    return nullptr;
  return bb2loop[bb];
}

void LoopInfo::printAllLoopInfo() {
  for (auto lo : loops) {
    lo->printInfo();
    std::cerr << std::endl;
  }
}

void Loop::insert(BasicBlock *bb) { loop_nodes.insert(bb); }

void Loop::erase(BasicBlock *bb) { loop_nodes.erase(bb); }

void Loop::setHeader(BasicBlock *bb) {
  if (loop_nodes.find(bb) == loop_nodes.end()) {
    std::cerr << "Loop header is not in loop nodes set.\n";
    std::terminate();
  }
  loop_header = bb;
}

BasicBlock *Loop::getHeader() { return loop_header; }

void Loop::printInfo() {
  using std::cerr;
  cerr << "Loop nodes (" << std::dec << loop_nodes.size() << "):" << std::endl;
  for (auto bb : loop_nodes) {
    cerr << std::hex << bb << " ";
  }
  cerr << "\nLoop header: " << loop_header << std::endl;
}

void LoopInfo::tarjan(node *cur, std::set<std::set<node *> *> &SCCs) {
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