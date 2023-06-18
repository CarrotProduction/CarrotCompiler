#include "riscv.h"
#include "instruction.h"
#include "ir.h"
#include "regalloc.h"

template<typename T> class DSU {

  std::map<T, T> father;
  T getfather(T x)
  {
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
  void solvePhiInstr(PhiInst* instr);

  // 下面的语句是需要生成对应riscv语句的
  BinaryRiscvInst *createBinaryInstr(BinaryInst* binaryInstr);
  UnaryRiscvInst *createUnaryInstr(UnaryInst* unaryInstr);
};