#ifndef INSTRUCTIONH
#define INSTRUCTIONH

#include "ir.h"
#include "riscv.h"
#include "regalloc.h"

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
  // void addOutBlock(RiscvBasicBlock *bb) { inB.push_back(bb); }
  // void addInBlock(RiscvBasicBlock *bb) { outB.push_back(bb); }
  void deleteInstr(RiscvInstr *instr) {
    auto it = std::find(instruction.begin(), instruction.end(), instr);
    if (it != instruction.end())
      instruction.erase(
          std::remove(instruction.begin(), instruction.end(), instr),
          instruction.end());
  }
  void replaceInstr(RiscvInstr *oldinst, RiscvInstr *newinst) {}
  // 在全部指令后面加入
  void addInstrBack(RiscvInstr *instr) {
    if (instr == nullptr)
      return;
    instruction.push_back(instr);
  }
  // 在全部指令之前加入
  void addInstrFront(RiscvInstr *instr) {
    if (instr == nullptr)
      return;
    instruction.insert(instruction.begin(), instr);
  }
  void addInstrBefore(RiscvInstr *instr, RiscvInstr *dst) {
    if (instr == nullptr)
      return;
    auto it = std::find(instruction.begin(), instruction.end(), dst);
    if (it != instruction.end())
      instruction.insert(it, instr);
    else
      addInstrBack(instr);
  }
  void addInstrAfter(RiscvInstr *instr, RiscvInstr *dst) {
    if (instr == nullptr)
      return;
    auto it = std::find(instruction.begin(), instruction.end(), dst);
    if (it != instruction.end()) {
      if (next(it) == instruction.end())
        instruction.push_back(instr);
      else
        instruction.insert(next(it), instr);
    } else {
      addInstrBack(instr);
    }
  }
  std::string print();
};

// 传入寄存器编号以生成一条语句，
// 上层由basic block整合
class RiscvInstr {
public:
  // 指令类型，除分支语句外，其他对应到riscv
  // 加立即数或者浮点加需要做区分
  enum InstrType {
    ADD = 0,
    ADDI,
    SUB,
    SUBI,
    MUL,
    DIV = 6,
    REM,
    FADD = 8,
    FSUB = 10,
    FMUL = 12,
    FDIV = 14,
    XOR = 16,
    XORI,
    AND,
    ANDI,
    OR,
    ORI,
    SW,
    LW,
    FSW = 30,
    FLW,
    ICMP,
    FCMP,
    PUSH,
    POP,
    CALL,
    RET,
    LI,
    MOV,
    FMV,
    FPTOSI,
    SITOFP,
    JMP,
    SHL,
    LSHR,
    ASHR,
    SHLI = 52,
    LSHRI,
    ASHRI,
    LA,
    ADDIW,
    BGT
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
      : RiscvInstr(op, 2, bb), word(flag) {
    setOperand(0, v1);
    setOperand(1, v2);
    setResult(target);
    this->parent_ = bb;
    // Optimize: 若立即数为0，则改用寄存器zero。
    if(v2->getType() == v2->IntImm && static_cast<RiscvConst*>(v2)->intval == 0){
      type_ = ADD;
      setOperand(1, getRegOperand("zero"));
    }
  }
  bool word;
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
      : RiscvInstr(InstrType::LI, 2, bb) {
    RiscvOperand *Const = new RiscvConst(Imm);
    setOperand(0, v1);
    setOperand(1, Const);
    this->parent_ = bb;
    if (flag)
      this->parent_->addInstrBack(this);
  }
  // v2->v1
  MoveRiscvInst(RiscvOperand *v1, RiscvOperand *v2, RiscvBasicBlock *bb,
                bool flag = 0)
      : RiscvInstr(InstrType::MOV, 2, bb) {
    setOperand(0, v1);
    setOperand(1, v2);
    this->parent_ = bb;
    if (flag)
      this->parent_->addInstrBack(this);
  }
};

// 注意压栈顺序问题！打印的时候严格按照lists内顺序
class PushRiscvInst : public RiscvInstr {
  int basicShift_;

public:
  PushRiscvInst(std::vector<RiscvOperand *> &lists, RiscvBasicBlock *bb,
                int basicShift)
      : RiscvInstr(InstrType::PUSH, lists.size(), bb), basicShift_(basicShift) {
    for (int i = 0; i < lists.size(); i++)
      setOperand(i, lists[i]);
  }
  std::string print() override;
};

// 注意出栈顺序问题！打印的时候严格按照lists内顺序
class PopRiscvInst : public RiscvInstr {
  int basicShift_;

public:
  // 传入所有要pop的变量
  PopRiscvInst(std::vector<RiscvOperand *> &lists, RiscvBasicBlock *bb,
               int basicShift)
      : RiscvInstr(InstrType::POP, lists.size(), bb), basicShift_(basicShift) {
    for (int i = 0; i < lists.size(); i++)
      setOperand(i, lists[i]);
  }
  std::string print() override;
};

// call调用语句+压栈语句
// 0作为函数名，1-n是函数各参数
class CallRiscvInst : public RiscvInstr {
public:
  CallRiscvInst(RiscvFunction *func, RiscvBasicBlock *bb)
      : RiscvInstr(InstrType::CALL, 1, bb) {
    setOperand(0, func);
  }
  virtual std::string print() override;
};

// 仅返回语句，返回参数由上层的block对应的function构造push语句和lw sw指令
class ReturnRiscvInst : public RiscvInstr {
public:
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
    if (source->isRegister() == false) {
      std::cerr << "[Fatal error] Invalid store instruction: " << print()
                << std::endl;
      std::terminate();
    }
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
    if (target == nullptr) {
      std::cerr << "[Fatal Error] Load Instruction's target is nullptr."
                << std::endl;
      std::terminate();
    }
    this->parent_ = bb;
  }
  std::string print() override;
};

// 整型比较
// 类型：cmpop val1, val2, true_link
// 传入参数：val1, val2, true_link, false_link（basic block指针形式）
// false_link如果为为下一条语句则不会发射j false_link指令
class ICmpRiscvInstr : public RiscvInstr {
public:
  static const std::map<ICmpInst::ICmpOp, std::string> ICmpOpName;
  static const std::map<ICmpInst::ICmpOp, std::string> ICmpOpSName;
  static const std::map<ICmpInst::ICmpOp, ICmpInst::ICmpOp> ICmpOpEquiv;
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

class ICmpSRiscvInstr : public ICmpRiscvInstr {
public:
  ICmpSRiscvInstr(ICmpInst::ICmpOp op, RiscvOperand *v1, RiscvOperand *v2,
                  RiscvOperand *target, RiscvBasicBlock *bb)
      : ICmpRiscvInstr(op, v1, v2, nullptr, bb) {
    setOperand(0, v1);
    setOperand(1, v2);
    setResult(target);
  }
  std::string print() override;
};


// 浮点比较
// 类型：cmpop val1, val2, true_link, false_link
// 假定basic block是顺序排布的，那么如果false_link恰好为下一个basic
// block，则不会发射j false_link指令
class FCmpRiscvInstr : public RiscvInstr {
public:
  static const std::map<FCmpInst::FCmpOp, std::string> FCmpOpName;
  static const std::map<FCmpInst::FCmpOp, FCmpInst::FCmpOp> FCmpOpEquiv;
  FCmpRiscvInstr(FCmpInst::FCmpOp op, RiscvOperand *v1, RiscvOperand *v2,
                  RiscvOperand *target, RiscvBasicBlock *bb)
      : RiscvInstr(FCMP, 2, bb), fcmp_op_(op) {
    setOperand(0, v1);
    setOperand(1, v2);
    setResult(target);
  }
  FCmpInst::FCmpOp fcmp_op_;
  std::string print() override;
};
class FpToSiRiscvInstr : public RiscvInstr {
public:
  FpToSiRiscvInstr(RiscvOperand *Source, RiscvOperand *Target,
                   RiscvBasicBlock *bb)
      : RiscvInstr(FPTOSI, 2, bb) {
    setOperand(0, Source);
    setOperand(1, Target);
  }
  virtual std::string print() override;
};

class SiToFpRiscvInstr : public RiscvInstr {
public:
  SiToFpRiscvInstr(RiscvOperand *Source, RiscvOperand *Target,
                   RiscvBasicBlock *bb)
      : RiscvInstr(SITOFP, 2, bb) {
    setOperand(0, Source);
    setOperand(1, Target);
  }
  virtual std::string print() override;
};

// LA rd, symbol ; x[rd] = &symbol
// `dest` : rd
// `name` : symbol
class LoadAddressRiscvInstr : public RiscvInstr {
public:
  std::string name_;
  LoadAddressRiscvInstr(RiscvOperand *dest, std::string name,
                        RiscvBasicBlock *bb)
      : RiscvInstr(LA, 1, bb), name_(name) {
    setOperand(0, dest);
  }
  virtual std::string print() override;
};

/**
 * 分支指令类。
 * BEQ rs1, zero, label1
 * J label2
 */
class BranchRiscvInstr : public RiscvInstr {
public:
  
  /// @brief 生成分支指令类。
  /// @param rs1 存储布尔值的寄存器
  /// @param trueLink 真值跳转基本块
  /// @param falseLink 假值跳转基本块
  BranchRiscvInstr(RiscvOperand *rs1, RiscvBasicBlock *trueLink,
                 RiscvBasicBlock *falseLink, RiscvBasicBlock *bb)
      : RiscvInstr(BGT, 3, bb) {
    setOperand(0, rs1);
    setOperand(1, trueLink);
    setOperand(2, falseLink);
  }
  virtual std::string print() override;
};
#endif // !INSTRUCTIONH