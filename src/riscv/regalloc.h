#ifndef REGALLOCH
#define REGALLOCH

#include "instruction.h"
#include "ir.h"
#include "riscv.h"

int IntRegID = 0, FloatRegID = 0; // 测试阶段使用
// 寄存器分配
class RegAlloca {

public:
  // 如果所有寄存器均被分配，则需要额外发射lw和sw指令
  RiscvOperand *find(Value *val, RiscvBasicBlock *bb) {
    if (val->type_->tid_ == Type::IntegerTyID) {
      ++IntRegID;
      RiscvIntReg *cur =
          new RiscvIntReg(new Register(Register::Int, ++IntRegID));
      return cur;
    } else {
      ++FloatRegID;
      RiscvFloatReg *cur =
          new RiscvFloatReg(new Register(Register::Float, ++IntRegID));
      return cur;
    }
  }
  RiscvOperand *findNonuse(RiscvBasicBlock *bb) {
    ++IntRegID;
    RiscvIntReg *cur = new RiscvIntReg(new Register(Register::Int, ++IntRegID));
    return cur;
  }
};
#endif // !REGALLOCH