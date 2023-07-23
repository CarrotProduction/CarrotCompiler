#ifndef REGALLOCH
#define REGALLOCH

#include "instruction.h"
#include "ir.h"
#include "riscv.h"
#include <cassert>

extern int IntRegID, FloatRegID; // 测试阶段使用
extern std::map<std::string, Register *> findReg;
// 寄存器分配（IR变量到汇编变量地址映射）
// 所有的临时变量均分配在栈上（从当前函数开始的地方开始计算栈地址，相对栈偏移地址），所有的全局变量放置在内存中（首地址+偏移量形式）
// 当存在需要寄存器保护的时候，直接找回原地址去进行
class RegAlloca {

public:
  // 如果所有寄存器均被分配，则需要额外发射lw和sw指令
  // inReg 参数如果为1，则要求必须返回寄存器；否则可以返回内存或栈上地址
  // instr 表示需要插入到哪条指令的前面，默认为最后面
  RiscvOperand *find(Value *val, RiscvBasicBlock *bb, RiscvInstr *instr = nullptr, int inReg = 0) {
     // 目前下面是一个没有考虑任何寄存器分配的工作，认为所有的变量都是寄存器存储，所有值可以直接使用的
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
    /*下面的代码是模拟一个大致结构
      // 首先找 空闲寄存器

      if (inReg) {
        // 如果找不到，需要到对应的bb处对应指令发射一条push指令或lw、sw指令
      }
    */ 
    
  }
  RiscvOperand *findNonuse(RiscvBasicBlock *bb, RiscvInstr *instr = nullptr) {
    ++IntRegID;
    RiscvIntReg *cur = new RiscvIntReg(new Register(Register::Int, ++IntRegID));
    return cur;
  }
  // 建立IR中变量到实际变量（内存固定地址空间）的映射
  void setPosition(Value *val, RiscvOperand *riscvVal) { pos[val] = riscvVal; }
  // 根据返回值是浮点型还是整型决定使用什么寄存器
  RiscvOperand * storeRet(Value *val) {
    if (val->type_->tid_ == Type::FloatTyID) {
      return new RiscvIntReg(findReg.at("a0"));
    }
    else {
      // 浮点寄存器约定
      return new RiscvIntReg(findReg.at("f0"));
    }
  }
private:
  std::map<Value *, RiscvOperand *> pos;
};
#endif // !REGALLOCH