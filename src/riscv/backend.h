#ifndef BACKENDH
#define BACKENDH

#include "instruction.h"
#include "ir.h"
#include "regalloc.h"
#include "riscv.h"
#include <cassert>

template <typename T> class DSU {

  std::map<T, T> father;
  T getfather(T x) {
    return father[x] == x ? x : father[x] = getfather(father[x]);
  }

public:
  DSU() = default;
  T query(T id) {
    // 不存在变量初值为自己
    if (father.count(id) == 0)
      return father[id] = id;
    else
      return getfather(id);
  }
  void merge(T u, T v) {
    u = getfather(u), v = getfather(v);
    if (u == v)
      return;
    father[u] = v;
  }
};

// 建立IR到RISCV指令集的映射
const std::map<Instruction::OpID, RiscvInstr::InstrType> toRiscvOp = {};

int LableCount = 0;
std::map<BasicBlock *, RiscvBasicBlock *> rbbLabel;
std::map<Function *, RiscvFunction *> functionLabel;
// 下面的函数仅为一个basic
// block产生一个标号，指令集为空，需要使用总控程序中具体遍历该bb才会产生内部指令
RiscvBasicBlock *createRiscvBasicBlock(BasicBlock *bb = nullptr);
RiscvFunction *createRiscvFunction(Function *foo = nullptr);
std::string toLable(int ind) { return ".L" + std::to_string(ind); }

// 总控程序
class RiscvBuilder {

public:
  Module *m;
  RiscvBasicBlock *rbb;
  RiscvModule *rm;
  RegAlloca *regAlloca;

  // znext 删掉
  void solveZnext();
  // alloca 删掉，在寄存器分配完成。
  void solveAlloca();
  // phi语句的合流：此处建立一个并查集DSU_for_Variable维护相同的变量。
  // 例如，对于if (A) y1=do something else y2=do another thing. Phi y3 y1, y2
  // 考虑将y1、y2全部合并到y3上
  DSU<std::string> DSU_for_Variable;
  void solvePhiInstr(PhiInst *instr);

  // 下面的语句是需要生成对应riscv语句的
  BinaryRiscvInst *createBinaryInstr(BinaryInst *binaryInstr);
  UnaryRiscvInst *createUnaryInstr(UnaryInst *unaryInstr);
  StoreRiscvInst *createStoreInstr(StoreInst *storeInstr);
  LoadRiscvInst *createLoadInstr(LoadInst *loadInstr);
  ICmpRiscvInstr *creatreICMPInstr(ICmpInst *icmpInstr, BranchInst *brInstr);
  FCmpRiscvInstr *createFCMPInstr(FCmpInst *fcmpInstr, BranchInst *brInstr);
  CallRiscvInst *createCallInstr(CallInst *callInstr);
};
#endif // !BACKENDH