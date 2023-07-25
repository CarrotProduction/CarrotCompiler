#ifndef REGALLOCH
#define REGALLOCH

#include "riscv.h"
#include <cassert>

extern int IntRegID, FloatRegID; // 测试阶段使用
extern Register *findReg(std::string reg);
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
  RiscvOperand *find(Value *val, RiscvBasicBlock *bb,
                     RiscvInstr *instr = nullptr, int inReg = 0);
  // TODO:2 findNonuse 实现一个函数，以找到一个当前尚未使用的寄存器以存放某个值。
  RiscvOperand *findNonuse(RiscvBasicBlock *bb, RiscvInstr *instr = nullptr);
  // TODO:5 findSpecificReg 将一个IR的value*找到一个特定的寄存器进行分配
  RiscvOperand *findSpecificReg(Value *val, std::string RegName, RiscvBasicBlock *bb, RiscvInstr *instr = nullptr);
  // 建立IR中变量到实际变量（内存固定地址空间）的映射
  void setPosition(Value *val, RiscvOperand *riscvVal);
  // 根据返回值是浮点型还是整型决定使用什么寄存器
  RiscvOperand *storeRet(Value *val);
  // TODO:6 使用到的callee保存寄存器，使用这个vector存储。具体为x8-x9,x18-x27（整型）；f8-f9,f18-f27（浮点）
  // 这些寄存器会在函数开始的时候压入栈中，return时会恢复
  std::vector<RiscvOperand *> savedRegister;

private:
  std::map<Value *, RiscvOperand *> pos;
};
#endif // !REGALLOCH