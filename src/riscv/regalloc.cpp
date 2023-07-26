#include "regalloc.h"

int IntRegID = 8, FloatRegID = 0; // 测试阶段使用
// 寄存器堆分配工作

// 根据输入的寄存器的名字`reg`返回相应的寄存器类指针。
// TODO:
// 需要涉及sp、fp、a0-a7（整数函数参数）、fa0-fa7（浮点函数参数）等寄存器。
Register *findReg(std::string reg) {
  if (reg == "a0") {
    return new Register(Register::RegType::Int, 10); // a0 is x10
  } else if (reg == "f0") {
    return new Register(Register::RegType::Float, 0);
  } else if (reg == "fp") {
    return new Register(Register::RegType::Int, 10); // fp is x8
  } else {
    assert(false);
  }
}

RiscvOperand *RegAlloca::find(Value *val, RiscvBasicBlock *bb,
                              RiscvInstr *instr, int inReg) {
  if (curReg.count(val))
    return curReg[val];
  // 目前下面是一个没有考虑任何寄存器分配的工作，认为所有的变量都是寄存器存储，所有值可以直接使用的
  if (val->type_->tid_ == Type::IntegerTyID || Type::PointerTyID) {
    ++IntRegID;
    RiscvIntReg *cur = new RiscvIntReg(new Register(Register::Int, ++IntRegID));
    return curReg[val] = cur;
  } else {
    ++FloatRegID;
    RiscvFloatReg *cur =
        new RiscvFloatReg(new Register(Register::Float, ++IntRegID));
    return curReg[val] = cur;
  }
  /*下面的代码是模拟一个大致结构
      // 首先找 空闲寄存器

      if (inReg) {
      // 如果找不到，需要到对应的bb处对应指令发射一条push指令或lw、sw指令
      }
  */
}
RiscvOperand *RegAlloca::findNonuse(RiscvBasicBlock *bb, RiscvInstr *instr) {
  ++IntRegID;
  RiscvIntReg *cur = new RiscvIntReg(new Register(Register::Int, ++IntRegID));
  return cur;
}
void RegAlloca::setPosition(Value *val, RiscvOperand *riscvVal) {
  pos[val] = riscvVal;
}
RiscvOperand *RegAlloca::storeRet(Value *val) {
  if (val->type_->tid_ == Type::FloatTyID) {
    return new RiscvIntReg(findReg("a0"));
  } else {
    // 浮点寄存器约定
    return new RiscvIntReg(findReg("f0"));
  }
}
RiscvOperand *RegAlloca::findSpecificReg(Value *val, std::string RegName, RiscvBasicBlock *bb, RiscvInstr *instr) {
  // TODO
  assert(false);
}