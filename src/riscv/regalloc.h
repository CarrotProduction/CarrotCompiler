#ifndef REGALLOCH
#define REGALLOCH

#include "instruction.h"
#include "../ir/ir.h"
#include "riscv.h"
#include <cassert>

extern int IntRegID, FloatRegID; // 测试阶段使用

// findReg map：根据寄存器名字找到寄存器对应的指针（访问对应寄存器的功能）
extern const std::map<std::string, Register *> findReg;

// RegAlloca类被放置在**每个函数**内，每个函数内是一个新的寄存器分配类。
// 因而约定x8-x9 x18-27、f8-9、f18-27 是约定的所有函数都要保护的寄存器，用完要恢复原值
// 其他的寄存器（除函数参数所用的a0-a7等寄存器）都视为是不安全的，可能会在之后的运算中发生变化
// TODO:3 在该类的实例生存周期内，使用到的需要保护的寄存器使用一个vector<Register*> 存储


// 寄存器分配（IR变量到汇编变量地址映射）
// 所有的临时变量均分配在栈上（从当前函数开始的地方开始计算栈地址，相对栈偏移地址），所有的全局变量放置在内存中（首地址+偏移量形式）
// 当存在需要寄存器保护的时候，直接找回原地址去进行
class RegAlloca {

public:
  // 如果所有寄存器均被分配，则需要额外发射lw和sw指令
  // inReg 参数如果为1，则要求必须返回寄存器；否则可以返回内存或栈上地址
  // instr 表示需要插入到哪条指令的前面，默认为最后面
  
  // TODO:1 find函数 实现一个find——对于IR的一个给定值value*，找到一个寄存器（RiscvOperand*）以存放该值
  RiscvOperand *find(Value *val, RiscvBasicBlock *bb, RiscvInstr *instr = nullptr, int inReg = 0) {
    /*下面的代码是模拟一个大致结构
      // 首先找 空闲寄存器

      if (inReg) {
        // 如果找不到，需要到对应的bb处对应指令发射一条push指令或lw、sw指令
      }
    */
  }
  // TODO:2 findNonuse 实现一个函数，以找到一个当前尚未使用的寄存器以存放某个值。
  RiscvOperand *findNonuse(RiscvBasicBlock *bb, RiscvInstr *instr = nullptr) {
    ++IntRegID;
    RiscvIntReg *cur = new RiscvIntReg(new Register(Register::Int, ++IntRegID));
    return cur;
  }
  // 已经实现：建立IR中变量到实际变量（内存固定地址空间）的映射
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
