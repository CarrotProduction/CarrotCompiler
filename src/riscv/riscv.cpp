#include "riscv.h"
#include "backend.h"
#include "ir.h"

// 输出函数对应的全部riscv语句序列
// 由于一个函数可能有若干个出口，因而恢复现场的语句根据basic block
// 语句中的ret语句前面附带出现，因而不在此出现
std::string RiscvFunction::print() {
  std::string riscvInstr = this->name_ + ":\n"; // 函数标号打印
  riscvInstr += this->storeRegisterInstr();
  // 对各个basic block进行拼接
  for (auto x : this->blk)
    riscvInstr += x->print();
  return riscvInstr;
}

std::string RiscvBasicBlock::print() {
  std::string riscvInstr = this->name_ + ":\n";
  for (auto x : this->instruction)
    riscvInstr += x->print();
  return riscvInstr;
}

// 对各寄存器进行保存。此处需要查询当前使用寄存器情况。考虑维护一个全局寄存器状态以及数值R
extern std::map<Register *, int> regUsed;
std::string RiscvFunction::storeRegisterInstr() {
  std::string riscvInstr = "";
  for (auto [reg, _] : regUsed) {
    RiscvOperand *cur;
    if (reg->regtype_ == Register::Float)
      cur = new RiscvFloatReg(reg);
    else
      cur = new RiscvIntReg(reg);
    this->storedEnvironment.insert(cur);
    std::string name = cur->print();
    riscvInstr += "\t\tpush\t" + name;
    this->addArgs(cur);
  }
  return riscvInstr;
}

// 出栈顺序和入栈相反
// 建议不使用pop语句，直接从栈中取值，最后直接修改sp的值即可
// 使用一个单独的return block以防止多出口return
extern int LableCount;
void RiscvFunction::addRestoredBlock() {
  RiscvBasicBlock *bb = createRiscvBasicBlock(nullptr);
  // 先恢复sp的值
  // 下面这几个语句有点问题！MoveRiscvInst提示说“不允许使用抽象类类型”。
  // RiscvInstr *restoreSP = new MoveRiscvInst(findReg.at("sp"),
  // this->callerSP_, bb, 1); bb->addInstrBack(restoreSP);
  // 插入各种pop指令，按照顺序
  for (auto reg : storedEnvironment) {
    // 分浮点+整型
  }
  this->addBlock(bb);
}