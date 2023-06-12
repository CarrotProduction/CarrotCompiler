#include "ir.h"
#include "riscv.h"

// 传入寄存器编号以生成一条语句，
// 上层由basic block整合
class RiscvInstr {
public:
  // 指令类型，除分支语句外，其他对应到riscv
  // 加立即数或者浮点加需要做区分
  enum InstrType {
    ADD = 1,
    ADDI,
    SUB,
    SUBI,
    FAND,
    FSUB,
    MUL,
    DIV,
    REM,
    XOR,
    XORI,
    AND,
    ANDI,
    OR,
    ORI,
    ANDI,
    ORI,
    SW,
    LW,
    ICMP,
    FCMP,
    PUSH,
    POP,
    CALL,
    RET,
    SW,
    LW, 
    LI, 
    MOV
  };
  const std::map<InstrType, std::string> RiscvName;
  const std::map<ICmpInst::ICmpOp, std::string> ICMPOPName;
  const std::map<FCmpInst::FCmpOp, std::string> FCMPOPName;
  InstrType type_;
  RiscvInstr(InstrType type, int op_nums) : type_(type), parent_(nullptr) {
    operand_.resize(op_nums);
  }
  RiscvInstr(InstrType type, int op_nums, RiscvBasicBlock *bb)
      : type_(type), parent_(bb) {
    operand_.resize(op_nums);
  }
  ~RiscvInstr() = default;
  RiscvBasicBlock *parent_;
  virtual std::string print() = 0;
  std::vector<RiscvOperand *> operand_;
  RiscvOperand *result_;
  void setOperand(int ind, RiscvOperand *val) {
    assert(ind >= 0 && ind < operand_.size());
    operand_[ind] = val;
  }
  void setResult(RiscvOperand *result) { result_ = result; }
  void removeResult() { result_ = nullptr; }
  // 消除一个操作数
  void removeOperand(int i) { operand_[i] = nullptr; }
  // 清除指令
  void clear() { operand_.clear(), removeResult(); }
  // 先操作数后返回值
  std::vector<RiscvOperand *> getOperandResult() {
    std::vector<RiscvOperand *> ans(operand_);
    if (result_ != nullptr)
      ans.push_back(result_);
    return ans;
  }
  RiscvOperand *getOperand(int i) const { return operand_[i]; }
  virtual std::string print() = 0;
};

// 二元指令
class BinaryRiscvInst : public RiscvInstr {
public:
  virtual std::string print() override;
  BinaryRiscvInst() = default;
  // target = v1 op v2，后面接一个flag参数表示要不要加入到对应的basic block中
  BinaryRiscvInst(InstrType op, RiscvOperand *v1, RiscvOperand *v2,
                  RiscvOperand *target, RiscvBasicBlock *bb, bool flag = 0)
      : RiscvInstr(op, 2, bb) {
    setOperand(0, v1);
    setOperand(1, v2);
    setResult(target);
    this->parent_ = bb;
    if (flag)
      this->parent_->addInstrBack(this);
  }
};

// 一元指令
class UnaryRiscvInst : public RiscvInstr {
public:
  virtual std::string print() override;
  UnaryRiscvInst() = default;
  // target = op v1，后面接一个flag参数表示要不要加入到对应的basic block中
  UnaryRiscvInst(InstrType op, RiscvOperand *v1, RiscvOperand *target,
                 RiscvBasicBlock *bb, bool flag = 0)
      : RiscvInstr(op, 1, bb) {
    setOperand(0, v1);
    setResult(target);
    this->parent_ = bb;
    if (flag)
      this->parent_->addInstrBack(this);
  }
};

// 加入一个整型mov指令(LI)
class MoveRiscvInst : public RiscvInstr {
public:
  virtual std::string print() override;
  MoveRiscvInst() = default;
  MoveRiscvInst(RiscvOperand *v1, int Imm,
                 RiscvBasicBlock *bb, bool flag = 0)
      : RiscvInstr(InstrType::LI, 1, bb) {
    RiscvOperand *Const = new RiscvConst(Imm);
    setOperand(0, Const);
    setResult(v1);
    this->parent_ = bb;
    if (flag)
      this->parent_->addInstrBack(this);
  }
  // v2->v1
  MoveRiscvInst(RiscvOperand *v1, RiscvOperand *v2,
                 RiscvBasicBlock *bb, bool flag = 0)
      : RiscvInstr(InstrType::MOV, 1, bb) {
    setOperand(0, v2);
    setResult(v1);
    this->parent_ = bb;
    if (flag)
      this->parent_->addInstrBack(this);
  }
};

// 注意压栈顺序问题！打印的时候严格按照lists内顺序
class PushRiscvInst : public RiscvInstr {

public:
  PushRiscvInst(std::vector<RiscvBasicBlock *> &lists, RiscvBasicBlock *bb)
      : RiscvInstr(InstrType::PUSH, lists.size(), bb) {
    for (int i = 0; i < lists.size(); i++)
      setOperand(i, lists[i]);
  }
  virtual std::string print() override;
};

// 注意出栈顺序问题！打印的时候严格按照lists内顺序
class PopRiscvInst : public RiscvInstr {

public:
  // 传入所有要pop的变量
  PopRiscvInst(std::vector<RiscvBasicBlock *> &lists, RiscvBasicBlock *bb)
      : RiscvInstr(InstrType::POP, lists.size(), bb) {
    for (int i = 0; i < lists.size(); i++)
      setOperand(i, lists[i]);
  }
  virtual std::string print() override;
};

// 仅call调用语句
class CallRiscvInst : public RiscvInstr {
public:
  CallRiscvInst(RiscvFunction *func, RiscvBasicBlock *bb)
      : RiscvInstr(InstrType::CALL, 1, bb) {
    setOperand(0, func);
  }
  virtual std::string print() override;
};

// 仅返回语句，返回参数由上层的block对应的function构造push语句
class ReturnRiscvInst : public RiscvInstr {
public:
  ReturnRiscvInst(RiscvBasicBlock *bb) : RiscvInstr(InstrType::RET, 0, bb) {}
  virtual std::string print() override;
};

// Store 指令格式：sw source_value(reg), shift(base reg)
// 目的：source_value->M[base reg + shift]
// 传入源寄存器，目的寄存器和偏移地址（默认为0）
// 如果是直接寻址，则base填x0号寄存器
class StoreRiscvInst : public RiscvInstr {

public:
  int shift_; // 地址偏移量
  Type type;  // 到底是浮点还是整型
  StoreRiscvInst(Type *ty, RiscvOperand *source, RiscvOperand *target,
                 RiscvBasicBlock *bb, int shift = 0)
      : RiscvInstr(InstrType::SW, 2, bb), shift_(shift), type(ty->tid_) {
    setOperand(0, source);
    setOperand(1, target);
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
  Type type;  // 到底是浮点还是整型
  LoadRiscvInst(Type *ty, RiscvOperand *dest, RiscvOperand *target,
                RiscvBasicBlock *bb, int shift = 0)
      : RiscvInstr(InstrType::LW, 2, bb), shift_(shift), type(ty->tid_) {
    setOperand(0, dest);
    setOperand(1, target);
    this->parent_ = bb;
  }
  virtual std::string print() override;
};

// 整型比较
// 类型：cmpop val1, val2, true_link
// 传入参数：val1, val2, true_link, false_link（basic block指针形式）
// false_link如果为为下一条语句则不会发射jmp false_link指令
class ICmpRiscvInstr : RiscvInstr {
public:
  static const std::map<ICmpInst::ICmpOp, std::string> ICmpOpName;
  ICmpRiscvInstr(ICmpInst::ICmpOp op, RiscvOperand *v1, RiscvOperand *v2,
                 RiscvBasicBlock *trueLink, RiscvBasicBlock *falseLink,
                 RiscvBasicBlock *bb)
      : RiscvInstr(ICMP, 4, bb), icmp_op_(op) {
    setOperand(0, v1);
    setOperand(1, v2);
    setOperand(2, trueLink);
    setOperand(3, falseLink);
  }
  ICmpRiscvInstr(ICmpInst::ICmpOp op, RiscvOperand *v1, RiscvOperand *v2,
                 RiscvBasicBlock *trueLink, RiscvBasicBlock *bb)
      : RiscvInstr(ICMP, 4, bb), icmp_op_(op) {
    setOperand(0, v1);
    setOperand(1, v2);
    setOperand(2, trueLink);
    setOperand(3, nullptr);
  }
  ICmpInst::ICmpOp icmp_op_;
  virtual std::string print() override;
};

// 浮点比较
// 类型：cmpop val1, val2, true_link, false_link
// 假定basic block是顺序排布的，那么如果false_link恰好为下一个basic
// block，则不会发射jmp false_link指令
class FCmpRiscvInstr : RiscvInstr {
public:
  static const std::map<FCmpInst::FCmpOp, std::string> FCmpInst::FCmpOpName;
  FCmpRiscvInstr(FCmpInst::FCmpOp op, RiscvOperand *v1, RiscvOperand *v2,
                 RiscvBasicBlock *trueLink, RiscvBasicBlock *falseLink,
                 RiscvBasicBlock *bb)
      : RiscvInstr(ICMP, 4, bb), fcmp_op_(op) {
    setOperand(0, v1);
    setOperand(1, v2);
    setOperand(2, trueLink);
    setOperand(3, falseLink);
  }
  FCmpRiscvInstr(FCmpInst::FCmpOp op, RiscvOperand *v1, RiscvOperand *v2,
                 RiscvBasicBlock *trueLink, RiscvBasicBlock *bb)
      : RiscvInstr(ICMP, 4, bb), fcmp_op_(op) {
    setOperand(0, v1);
    setOperand(1, v2);
    setOperand(2, trueLink);
    setOperand(3, nullptr);
  }
  FCmpInst::FCmpOp fcmp_op_;
  virtual std::string print() override;
};