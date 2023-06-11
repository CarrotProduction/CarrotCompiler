#include "ir.h"
#include "register.h"

// 基本语句块，用于存储语句序列，并确定标号
// 所有的跳转地址全部使用标号而不用偏移地址
// 所有的riscv语句挂靠到riscvBasicBlock下
// 参数和basic block相同，仅重新标号而已
// 用法同basic block
class RiscvBasicBlock : public BasicBlock {

public:
  RiscvBasicBlock() = default;
  explicit RiscvBasicBlock(Module *m, const std::string &name, Function *parent)
      : BasicBlock(m, name, parent) {
    parent_->add_basic_block(this);
  }
};

// 传入寄存器编号以生成一条语句，该语句不负责实现lw和sw。该功能建议由寄存器分配+basic
// block拼装完成。
// 同时为生成该指令的汇编，你只需要传入register类型即可，内部自行转换到通用value类型。
// 最后的riscv指令与正常的IR指令格式相同，只是print方式和存储的地址名称不同而已。
// 因为最后不需要再进行类型检查，因而在实现上仅保存了操作数的名称。

class RiscvInstr : public Instruction {
public:
  RiscvInstr(Type *ty, OpID id, unsigned num_ops)
      : Instruction(ty, id, num_ops) {}
  RiscvInstr(Type *ty, OpID id, unsigned num_ops, RiscvBasicBlock *bb)
      : Instruction(ty, id, num_ops, bb) {}
};

// 实例化的时候必须显式给出目标地址寄存器
// 对于某些特殊的指令如addi、subi等使用立即数的指令，此处不保证opid与register的配套。
// 由于左值必须是寄存器，因而仅保存其名称
class BinaryRiscvInst : public RiscvInstr {
public:
  virtual std::string print() override;
  BinaryRiscvInst() = default;
  // target = v1 op v2，target 由instruction继承的value给出
  BinaryRiscvInst(Type *ty, OpID op, Register *v1, Register *v2,
                  Register *target, RiscvBasicBlock *bb)
      : RiscvInstr(ty, op, 2, bb) {
    Value *regv1 = static_cast<Value *>(v1), *regv2 = static_cast<Value *>(v2);
    set_operand(0, v1);
    set_operand(1, v2);
    this->name_ = target->name_;
  }
  BinaryRiscvInst(Type *ty, OpID op, Register *v1, Register *v2,
                  Register *target, RiscvBasicBlock *bb, bool flag)
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
  UnaryRiscvInst(Type *ty, OpID op, Register *v1, Register *target,
                 RiscvBasicBlock *bb)
      : RiscvInstr(ty, op, 1, bb) {
    Value *regv1 = static_cast<Value *>(v1);
    set_operand(0, v1);
    this->name_ = target->name_;
  }
  UnaryRiscvInst(Type *ty, OpID op, Register *v1, Register *target,
                 RiscvBasicBlock *bb, bool flag)
      : RiscvInstr(ty, op, 1) {
    Value *regv1 = static_cast<Value *>(v1);
    set_operand(0, v1);
    this->name_ = target->name_;
    this->parent_ = bb;
  }
};

class PushRiscvInst : public RiscvInstr {

public:
  PushRiscvInst(Register *val, RiscvBasicBlock *bb)
      : RiscvInstr(bb->parent_->parent_->void_ty_, Instruction::Store, 1, bb) {
    set_operand(0, val);
  }
  virtual std::string print() override;
};

class PopRiscvInst : public RiscvInstr {

public:
  PopRiscvInst(Register *val, RiscvBasicBlock *bb)
      : RiscvInstr(bb->parent_->parent_->void_ty_, Instruction::Load, 1, bb) {
    set_operand(0, val);
  }
  virtual std::string print() override;
};

// 仅调用语句
class CallRiscvInst : public RiscvInstr {
public:
  CallRiscvInst(Function *func, RiscvBasicBlock *bb)
      : RiscvInstr(static_cast<FunctionType *>(func->type_)->result_,
                   Instruction::Call, 1, bb) {
    set_operand(0, func);
  }
  virtual std::string print() override;
};

// 仅返回语句
class ReturnRiscvInst : public RiscvInstr {
public:
  ReturnRiscvInst(RiscvBasicBlock *bb)
      : RiscvInstr(bb->parent_->parent_->void_ty_, Instruction::Ret, 0, bb) {}
  virtual std::string print() override;
};

// Store 指令格式：sw source_value(reg), shift(base reg)
// 目的：source_value->M[base reg + shift]
// 传入源寄存器，目的寄存器和偏移地址（默认为0）
// 如果是直接寻址，则base填x0号寄存器
class StoreRiscvInst : public RiscvInstr {

public:
  int shift_; // 地址偏移量
  Type type; // 到底是浮点还是整型
  StoreRiscvInst(Type *ty, Register *source, Register *target, RiscvBasicBlock *bb, int shift = 0)
      : RiscvInstr(ty, Instruction::Store, 2, bb), shift_(shift),
        type(ty->tid_) {
    Value *val = static_cast<Value *>(source),
          *tar = static_cast<Value *>(target);
    set_operand(0, val);
    set_operand(1, tar);
    this->parent_ = bb;
  }
  virtual std::string print() override;
};

// 指令传入格式同store
// 先dest 后base reg
// 目的：M[base reg + shift]->dest reg
// 需指明是浮点还是整型
class LoadRiscvInst : public RiscvInstr {
  int shift_; // 地址偏移量
  Type type; // 到底是浮点还是整型
  LoadRiscvInst(Type *ty, Register *dest, Register *target, RiscvBasicBlock *bb, int shift = 0)
      : RiscvInstr(bb->parent_->parent_->void_ty_, Instruction::Load, 2, bb),
        shift_(shift), type(ty->tid_) {
    Value *val = static_cast<Value *>(dest),
          *tar = static_cast<Value *>(target);
    set_operand(0, val);
    set_operand(1, tar);
    this->parent_ = bb;
  }
  virtual std::string print() override;
};

// 整型比较
// 类型：cmpop val1, val2, true_link
// 传入参数：val1, val2, （basic block指针形式）
// false_link为下一条语句
class ICmpRiscvInstr : RiscvInstr {
  static const std::map<ICmpInst::ICmpOp, std::string> ICmpOpName;
  ICmpRiscvInstr(ICmpInst::ICmpOp op, Register *v1, Register *v2,
                 RiscvBasicBlock *trueLink, RiscvBasicBlock *bb)
      : RiscvInstr(bb->parent_->parent_->int1_ty_, Instruction::ICmp, 2, bb),
        icmp_op_(op), TrueLink_(trueLink) {
    Value *value1 = static_cast<Value *>(v1),
          *Value2 = static_cast<Value *>(v2);
    set_operand(0, value1);
    set_operand(1, Value2);
  }
  ICmpInst::ICmpOp icmp_op_;
  RiscvBasicBlock *TrueLink_;
  virtual std::string print() override;
};

// 浮点比较
// 类型：cmpop val1, val2, true_link
// false_link为下一条语句
class FCmpRiscvInstr : RiscvInstr {
  static const std::map<FCmpInst::FCmpOp, std::string> FCmpOpName;
  FCmpRiscvInstr(FCmpInst::FCmpOp op, Register *v1, Register *v2,
                 RiscvBasicBlock *trueLink, RiscvBasicBlock *bb)
      : RiscvInstr(bb->parent_->parent_->float32_ty_, Instruction::FCmp, 2, bb),
        fcmp_op_(op), TrueLink_(trueLink) {
    Value *value1 = static_cast<Value *>(v1),
          *Value2 = static_cast<Value *>(v2);
    set_operand(0, value1);
    set_operand(1, Value2);
  }
  FCmpInst::FCmpOp fcmp_op_;
  RiscvBasicBlock *TrueLink_;
  virtual std::string print() override;
};
