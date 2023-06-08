#include "ir.h"
#include "register.h"

// 传入寄存器编号以生成一条语句，该语句不负责实现lw和sw。该功能建议由寄存器分配+basic block拼装完成。
// 同时为生成该指令的汇编，你只需要传入register类型即可，内部自行转换到通用value类型。
// 最后的riscv指令与正常的IR指令格式相同，只是print方式和存储的地址名称不同而已。
// 因为最后不需要再进行类型检查，因而在实现上仅保存了操作数的名称。

class RiscvInstr : public Instruction {
public:
  RiscvInstr(Type *ty, OpID id, unsigned num_ops)
      : Instruction(ty, id, num_ops) {}
  RiscvInstr(Type *ty, OpID id, unsigned num_ops, BasicBlock *bb) : Instruction(ty, id, num_ops, bb) {}
};

// 实例化的时候必须显式给出目标地址寄存器
// 对于某些特殊的指令如addi、subi等使用立即数的指令，此处不保证opid与register的配套。
// 由于左值必须是寄存器，因而仅保存其名称
class BinaryRiscvInst : public RiscvInstr {
public:
  virtual std::string print() override;
  BinaryRiscvInst() = default;
  // target = v1 op v2，target 由instruction继承的value给出
  BinaryRiscvInst(Type *ty, OpID op, Register *v1, Register *v2, Register *target, BasicBlock *bb) 
    : RiscvInstr(ty, op, 2, bb) {
    Value *regv1 = static_cast<Value *>(v1), *regv2 = static_cast<Value *>(v2);
    set_operand(0, v1);
    set_operand(1, v2);
    this->name_ = target->name_;
  }
  BinaryRiscvInst(Type *ty, OpID op, Register *v1, Register *v2, Register *target, BasicBlock *bb, bool flag)
      : RiscvInstr(ty, op, 2) {
    Value *regv1 = static_cast<Value *>(v1), *regv2 = static_cast<Value *>(v2);
    set_operand(0, v1);
    set_operand(1, v2);
    this->name_ = target->name_;
    this->parent_ = bb;
  }
};

class UnaryRiscvInst : public RiscvInstr {
public:
  virtual std::string print() override;
  UnaryRiscvInst() = default;
  // target = v1 op v2，target 由instruction继承的value给出
  UnaryRiscvInst(Type *ty, OpID op, Register *v1, Register *target, BasicBlock *bb) 
    : RiscvInstr(ty, op, 1, bb) {
    Value *regv1 = static_cast<Value *>(v1);
    set_operand(0, v1);
    this->name_ = target->name_;
  }
  UnaryRiscvInst(Type *ty, OpID op, Register *v1, Register *target, BasicBlock *bb, bool flag)
      : RiscvInstr(ty, op, 1) {
    Value *regv1 = static_cast<Value *>(v1);
    set_operand(0, v1);
    this->name_ = target->name_;
    this->parent_ = bb;
  }
};

class CallRiscvInst : public RiscvInstr {
public:
  CallRiscvInst(Function *func, std::vector<Value *> args, BasicBlock *bb)
      : RiscvInstr(static_cast<FunctionType *>(func->type_)->result_, Instruction::Call, args.size() + 1, bb) {
    int num_ops = args.size() + 1;
    for (int i = 0; i < num_ops - 1; i++)
      set_operand(i, args[i]);
    set_operand(num_ops - 1, func);
  }
  virtual std::string print() override;
};

// Store 指令格式：sw source_value(reg), shift(base reg)
// 目的：source_value->M[base reg + shift]
// 传入源寄存器，目的寄存器和偏移地址（默认为0）
// 如果是直接寻址，则base填x0号寄存器
class StoreRiscvInst : public RiscvInstr {

public:
  int shift_; // 地址偏移量
  StoreRiscvInst(Type *ty, Register *source, Register *target, int shift = 0,
                 BasicBlock *bb)
      : RiscvInstr(ty, Instruction::Store, 2, bb), shift_(shift) {
    Value *val = static_cast<Value *>(source), *tar = static_cast<Value *>(target);
    set_operand(0, val);
    set_operand(1, tar);
    this->parent_ = bb;
  }
  virtual std::string print() override;
};

// 指令传入格式同store
// 先dest 后base reg
// 目的：M[base reg + shift]->dest reg
class LoadRiscvInst : public RiscvInstr {
  int shift_; // 地址偏移量
  LoadRiscvInst(Type *ty, Register *dest, Register *target, int shift = 0,
                 BasicBlock *bb)
      : RiscvInstr(ty, Instruction::Load, 2, bb), shift_(shift) {
    Value *val = static_cast<Value *>(dest), *tar = static_cast<Value *>(target);
    set_operand(0, val);
    set_operand(1, tar);
    this->parent_ = bb;
  }
  virtual std::string print() override;
};
