#ifndef BASICOPERATION
#define BASICOPERATION
#include "../ir/ir.h"
#include <vector>
#include <map>
#include <set>
#include <stack>
#include "opt.h"

void deleteUse(Value* opnd,Instruction *inst);
void dfsGraph(BasicBlock *bb, std::set<BasicBlock *> &vis);
void SolvePhi(BasicBlock *bb, BasicBlock *succ_bb);
void DeleteUnusedBB(Function *func);

#endif // !BASICOPERATION