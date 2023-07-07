#pragma once
#include "ir.hpp"
#include "riscv.hpp"

// 语句块，也使用标号标识
// 必须挂靠在函数下，否则无法正常生成标号
// 可以考虑转化到riscv basic block做数据流分析，预留接口
class RiscvBasicBlock : public RiscvLabel {
public:
  RiscvFunction *func_;
  std::vector<RiscvInstr *> instruction;
  int blockInd_; // 表示了各个block之间的顺序
  RiscvBasicBlock(std::string name, RiscvFunction *func, int blockInd)
      : RiscvLabel(Block, name), func_(func), blockInd_(blockInd) {
    func->addBlock(this);
  }
  RiscvBasicBlock(std::string name, int blockInd)
      : RiscvLabel(Block, name), func_(nullptr), blockInd_(blockInd) {}
  void addFunction(RiscvFunction *func) { func->addBlock(this); }
  std::string printname() { return name_; }
  void addOutBlock(RiscvBasicBlock *bb) { inB.push_back(bb); }
  void addInBlock(RiscvBasicBlock *bb) { outB.push_back(bb); }
  void deleteInstr(RiscvInstr *instr) {
    auto it = std::find(instruction.begin(), instruction.end(), instr);
    if (it != instruction.end())
      instruction.erase(
          std::remove(instruction.begin(), instruction.end(), instr),
          instruction.end());
  }
  void replaceInstr(RiscvInstr *oldinst, RiscvInstr *newinst) {}
  // 在全部指令后面加入
  void addInstrBack(RiscvInstr *instr) { instruction.push_back(instr); }
  // 在全部指令之前加入
  void addInstrFront(RiscvInstr *instr) {
    instruction.insert(instruction.begin(), instr);
  }
  void addInstrBefore(RiscvInstr *instr) {
    auto it = std::find(instruction.begin(), instruction.end(), instr);
    if (it != instruction.end())
      instruction.insert(it, instr);
  }
  void addInstrAfter(RiscvInstr *instr, RiscvInstr *dst) {
    auto it = std::find(instruction.begin(), instruction.end(), instr);
    if (it != instruction.end()) {
      if (next(it) == instruction.end())
        instruction.push_back(instr);
      else
        instruction.insert(next(it), instr);
    }
  }
  std::string print();
  /*
    此处加入所需数据流分析的参数和函数
    下面为示例
  */
  std::vector<RiscvBasicBlock *> outB; // 出边
  std::vector<RiscvBasicBlock *> inB;  // 入边
};

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
    FADD,
    FSUB,
    MUL,
    MULU,
    DIV,
    DIVU,
    REM,
    REMU,
    XOR,
    XORI,
    AND,
    ANDI,
    OR,
    ORI,
    SW,
    LW,
    ICMP,
    FCMP,
    PUSH,
    POP,
    CALL,
    RET,
    LI,
    MOV,
    FMV,
    FLW,
    FSW,
    FPTOSI,
    SITOFP
  };
  const static std::map<InstrType, std::string> RiscvName;

  InstrType type_;
  RiscvBasicBlock *parent_;
  std::vector<RiscvOperand *> operand_;

  RiscvInstr(InstrType type, int op_nums);
  RiscvInstr(InstrType type, int op_nums, RiscvBasicBlock *bb);
  ~RiscvInstr() = default;

  virtual std::string print() = 0;

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
};

// 二元指令
class BinaryRiscvInst : public RiscvInstr {
public:
  std::string print() override;
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
  std::string print() override;
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
  std::string print() override;
  MoveRiscvInst() = default;
  MoveRiscvInst(RiscvOperand *v1, int Imm, RiscvBasicBlock *bb, bool flag = 0)
      : RiscvInstr(InstrType::LI, 1, bb) {
    RiscvOperand *Const = new RiscvConst(Imm);
    setOperand(0, Const);
    setResult(v1);
    this->parent_ = bb;
    if (flag)
      this->parent_->addInstrBack(this);
  }
  // v2->v1
  MoveRiscvInst(RiscvOperand *v1, RiscvOperand *v2, RiscvBasicBlock *bb,
                bool flag = 0)
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
  std::string print() override;
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
  std::string print() override;
};

// call调用语句+压栈语句
// 0作为函数名，1-n是函数各参数
class CallRiscvInst : public RiscvInstr {
public:
  CallRiscvInst(RiscvFunction *func, RiscvBasicBlock *bb,
                std::vector<RiscvOperand *> args)
      : RiscvInstr(InstrType::CALL, 1 + args.size(), bb) {
    setOperand(0, func);
    for (int i = 0; i < args.size(); i++)
      setOperand(i + 1, args[i]);
    // 栈内顺序同args顺序，依次使用PUSH操作
  }
  virtual std::string print() override;
};

// 仅返回语句，返回参数由上层的block对应的function构造push语句
class ReturnRiscvInst : public RiscvInstr {
public:
  // 有返回值
  ReturnRiscvInst(RiscvOperand *retVal, RiscvBasicBlock *bb)
      : RiscvInstr(InstrType::RET, 1, bb) {
    setOperand(1, retVal);
  }
  // 无返回值
  ReturnRiscvInst(RiscvBasicBlock *bb) : RiscvInstr(InstrType::RET, 0, bb) {}
  std::string print() override;
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
  std::string print() override;
};

// 指令传入格式同store
// 先dest 后base reg
// 目的：M[base reg + shift]->dest reg
// 需指明是浮点还是整型
class LoadRiscvInst : public RiscvInstr {
public:
  int shift_; // 地址偏移量
  Type type;  // 到底是浮点还是整型
  LoadRiscvInst(Type *ty, RiscvOperand *dest, RiscvOperand *target,
                RiscvBasicBlock *bb, int shift = 0)
      : RiscvInstr(InstrType::LW, 2, bb), shift_(shift), type(ty->tid_) {
    setOperand(0, dest);
    setOperand(1, target);
    this->parent_ = bb;
  }
  std::string print() override;
};

// 整型比较
// 类型：cmpop val1, val2, true_link
// 传入参数：val1, val2, true_link, false_link（basic block指针形式）
// false_link如果为为下一条语句则不会发射jmp false_link指令
class ICmpRiscvInstr : public RiscvInstr {
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
  std::string print() override;
};

// 浮点比较
// 类型：cmpop val1, val2, true_link, false_link
// 假定basic block是顺序排布的，那么如果false_link恰好为下一个basic
// block，则不会发射jmp false_link指令
class FCmpRiscvInstr : public RiscvInstr {
public:
  static const std::map<FCmpInst::FCmpOp, std::string> FCmpOpName;
  FCmpRiscvInstr(FCmpInst::FCmpOp op, RiscvOperand *v1, RiscvOperand *v2,
                 RiscvOperand *v3, RiscvBasicBlock *trueLink,
                 RiscvBasicBlock *falseLink, RiscvBasicBlock *bb)
      : RiscvInstr(FCMP, 5, bb), fcmp_op_(op) {
    setOperand(0, v1);
    setOperand(1, v2);
    setOperand(2, v3);
    setOperand(3, trueLink);
    setOperand(4, falseLink);
  }
  FCmpInst::FCmpOp fcmp_op_;
  std::string print() override;
};

class ZExtRiscvInstr : public RiscvInstr {
public:
};

class FpToSiRiscvInstr : public RiscvInstr {
public:
  FpToSiRiscvInstr(RiscvOperand *val, RiscvBasicBlock *bb)
      : RiscvInstr(FPTOSI, 2, bb) {
    setOperand(0, val);
  }
  virtual std::string print() override;
};

class SiToFpRiscvInstr : public RiscvInstr {
public:
  SiToFpRiscvInstr(RiscvOperand *val, RiscvBasicBlock *bb)
      : RiscvInstr(SITOFP, 2, bb) {
    setOperand(0, val);
  }
  virtual std::string print() override;
};