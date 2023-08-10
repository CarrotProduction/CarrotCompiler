#include "opt.h"
#include <functional>
#include <vector>

void DomainTree::execute() {
  for (auto foo : m->function_list_)
    if (!foo->basic_blocks_.empty()) {
      getBlockDom(foo);
      getBlockDomFront(foo);
    }
}

bool DomainTree::isLoopEdge(BasicBlock *a, BasicBlock *b) {
  return TraverseInd[a] > TraverseInd[b];
}

std::vector<BasicBlock *> DomainTree::postTraverse(BasicBlock *bb) {
  std::set<BasicBlock *> vis;
  std::vector<BasicBlock *> ans;
  std::function<void(BasicBlock *)> dfs = [&](BasicBlock *place) {
    vis.insert(place);
    for (auto child : place->succ_bbs_)
      if (vis.find(child) == vis.end())
        dfs(child);
    ans.push_back(place);
  };
  dfs(bb);
  return ans;
}

void DomainTree::getReversePostTraverse(Function *f) {
  doms.clear();
  reversePostTraverse.clear();
  TraverseInd.clear();
  auto entryBlock = *f->basic_blocks_.begin();
  auto seq = postTraverse(entryBlock);
  std::reverse(reversePostTraverse.begin(), reversePostTraverse.end());
  for (int i = 0; i < seq.size(); i++)
    TraverseInd[seq[i]] = i;
  reversePostTraverse = seq;
}

void DomainTree::getBlockDom(Function *f) {
  getReversePostTraverse(f);
  auto root = *f->basic_blocks_.begin();
  auto root_id = TraverseInd[root];
  doms.resize(root_id + 1, nullptr);
  doms.back() = root;
  bool change = true;
  while (change) {
    change = false;
    for (auto bb : reversePostTraverse)
      if (bb != root) {
        auto preds = bb->pre_bbs_;
        BasicBlock *curDom = nullptr;
        for (auto pred_bb : preds)
          if (doms[TraverseInd[pred_bb]] != nullptr) {
            curDom = pred_bb;
            break;
          }
        for (auto pred_bb : preds)
          if (doms[TraverseInd[pred_bb]] != nullptr)
            curDom = intersect(pred_bb, curDom);
        if (doms[TraverseInd[bb]] != curDom) {
          doms[TraverseInd[bb]] = curDom;
          change = true;
        }
      }
  }
  for (auto bb : reversePostTraverse)
    bb->idom_ = doms[TraverseInd[bb]];
}

void DomainTree::getBlockDomFront(Function *foo) {
  for (auto b : foo->basic_blocks_) {
    auto b_pred = b->pre_bbs_;
    if (b_pred.size() >= 2) {
      for (auto pred : b_pred) {
        auto runner = pred;
        while (runner != doms[TraverseInd[b]]) {
          runner->dom_frontier_.insert(b);
          runner = doms[TraverseInd[runner]];
        }
      }
    }
  }
}

BasicBlock *DomainTree::intersect(BasicBlock *b1, BasicBlock *b2) {
  auto head1 = b1;
  auto head2 = b2;
  while (head1 != head2) {
    while (TraverseInd[head1] < TraverseInd[head2])
      head1 = doms[TraverseInd[head1]];
    while (TraverseInd[head2] < TraverseInd[head1])
      head2 = doms[TraverseInd[head2]];
  }
  return head1;
}

void ReverseDomainTree::execute() {
  for (auto f : m->function_list_)
    if (!f->basic_blocks_.empty()) {
      for (auto bb : f->basic_blocks_) {
        bb->rdoms_.clear();
        bb->rdom_frontier_.clear();
      }
      getBlockDomR(f);
      getBlockDomFrontR(f);
      getBlockRdoms(f);
    }
}

void ReverseDomainTree::getPostTraverse(BasicBlock *bb,
                                    std::set<BasicBlock *> &visited) {
  visited.insert(bb);
  for (auto parent : bb->pre_bbs_)
    if (visited.find(parent) == visited.end())
      getPostTraverse(parent, visited);
  reverseTraverseInd[bb] = reverseTraverse.size();
  reverseTraverse.push_back(bb);
}

void ReverseDomainTree::getReversePostTraverse(Function *f) {
  reverseDomainBlock.clear();
  reverseTraverse.clear();
  reverseTraverseInd.clear();
  for (auto bb : f->basic_blocks_) {
    auto terminate_instr = bb->get_terminator();
    if (terminate_instr->op_id_ == Instruction::Ret) {
      exitBlock = bb;
      break;
    }
  }
  assert(exitBlock != nullptr);
  std::set<BasicBlock *> visited = {};
  getPostTraverse(exitBlock, visited);
  reverse(reverseTraverse.begin(), reverseTraverse.end());
}

void ReverseDomainTree::getBlockDomR(Function *f) {
  getReversePostTraverse(f);
  auto root = exitBlock;
  auto root_id = reverseTraverseInd[root];
  for (int i = 0; i < root_id; i++)
    reverseDomainBlock.push_back(nullptr);
  reverseDomainBlock.push_back(root);
  bool change = true;
  while (change) {
    change = false;
    for (auto bb : reverseTraverse) {
      if (bb != root) {
        BasicBlock *new_irdom = nullptr;
        for (auto rpred_bb : bb->succ_bbs_)
          if (reverseDomainBlock[reverseTraverseInd[rpred_bb]] != nullptr) {
            new_irdom = rpred_bb;
            break;
          }
        for (auto rpred_bb : bb->succ_bbs_)
          if (reverseDomainBlock[reverseTraverseInd[rpred_bb]] != nullptr)
            new_irdom = intersect(rpred_bb, new_irdom);
        if (reverseDomainBlock[reverseTraverseInd[bb]] != new_irdom) {
          reverseDomainBlock[reverseTraverseInd[bb]] = new_irdom;
          change = true;
        }
      }
    }
  }
}
void ReverseDomainTree::getBlockRdoms(Function *f) {
  for (auto bb : f->basic_blocks_) {
    if (bb == exitBlock)
      continue;
    auto current = bb;
    while (current != exitBlock) {
      bb->rdoms_.insert(current);
      current = reverseDomainBlock[reverseTraverseInd[current]];
    }
  }
}
void ReverseDomainTree::getBlockDomFrontR(Function *f) {
  for (auto bb_iter = f->basic_blocks_.rbegin();
       bb_iter != f->basic_blocks_.rend(); bb_iter++) {
    auto bb = *bb_iter;
    if (bb->succ_bbs_.size() >= 2) {
      for (auto rpred : bb->succ_bbs_) {
        auto runner = rpred;
        while (runner != reverseDomainBlock[reverseTraverseInd[bb]]) {
          runner->rdom_frontier_.insert(bb);
          runner = reverseDomainBlock[reverseTraverseInd[runner]];
        }
      }
    }
  }
}
BasicBlock *ReverseDomainTree::intersect(BasicBlock *b1, BasicBlock *b2) {
  auto head1 = b1;
  auto head2 = b2;
  while (head1 != head2) {
    while (reverseTraverseInd[head1] < reverseTraverseInd[head2])
      head1 = reverseDomainBlock[reverseTraverseInd[head1]];
    while (reverseTraverseInd[head2] < reverseTraverseInd[head1])
      head2 = reverseDomainBlock[reverseTraverseInd[head2]];
  }
  return head1;
}
