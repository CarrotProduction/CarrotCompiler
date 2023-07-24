#ifndef REGALLOCH
#define REGALLOCH

#include "instruction.h"
#include "ir.h"
#include "riscv.h"
#include <cassert>

extern int IntRegID, FloatRegID; // 测试阶段使用
extern Register *findReg(string reg);
// 寄存器分配（IR变量到汇编变量地址映射）
// 所有的临时变量均分配在栈上（从当前函数开始的地方开始计算栈地址，相对栈偏移地址），所有的全局变量放置在内存中（首地址+偏移量形式）
// 当存在需要寄存器保护的时候，直接找回原地址去进行
class RegAlloca {
public:
  // 如果所有寄存器均被分配，则需要额外发射lw和sw指令
  // inReg 参数如果为1，则要求必须返回寄存器；否则可以返回内存或栈上地址
  // instr 表示需要插入到哪条指令的前面，默认为最后面
  RiscvOperand *find(Value *val, RiscvBasicBlock *bb,
                     RiscvInstr *instr = nullptr, int inReg = 0);
  RiscvOperand *findNonuse(RiscvBasicBlock *bb, RiscvInstr *instr = nullptr);
  // 建立IR中变量到实际变量（内存固定地址空间）的映射
  void setPosition(Value *val, RiscvOperand *riscvVal) { pos[val] = riscvVal; }
  // 根据返回值是浮点型还是整型决定使用什么寄存器
  RiscvOperand *storeRet(Value *val);

private:
  std::map<Value *, RiscvOperand *> pos;
};
#endif // !REGALLOCH