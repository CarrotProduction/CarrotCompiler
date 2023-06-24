#include "ir.h"

class RiscvOperand;
class RiscvConst;
class RiscvModule;
class RiscvFunction;
class RiscvGlobalVariable;
class RiscvInstr;

class RiscvOperand {
public:
  enum OpTy {
    Void = 0, // 空类型，为无函数返回值专用的类型
    IntImm,   // 整型立即数
    FloatImm, // 浮点立即数
    IntReg,   // 数值直接保存在整型寄存器
    FloatReg, // 数值直接保存在浮点寄存器
    IntMem, // 整型M[R(rd)+shift]，无寄存器可用x0，无偏移可用shift=0
    FloatMem, // 浮点，同上
    Function, // 调用函数
    Block     // 基本语句块标号
  };
  OpTy tid_;
  explicit RiscvOperand(OpTy tid) : tid_(tid) {}
  ~RiscvOperand() = default;
  virtual std::string print() = 0;
};

// 寄存器堆
class Register {

public:
  Register() = default;
  ~Register() = default;
  enum RegType {
    Int = 1, // 整型
    Float,   // 浮点
    Stack,   // 栈专用
    Zero     // 零寄存器
  };
  RegType regtype_;
  int rid_; // 寄存器编号
  Register(RegType regtype, int rid) : regtype_(regtype), rid_(rid) {}
  std::string print() {
    if (this->regtype_ == Float)
      return "f" + std::to_string(rid_);
    // 整型各类输出
    switch (this->rid_) {
    case 0:
      return "zero";
    case 1:
      return "ra";
    case 2:
      return "sp";
    case 3:
      return "gp";
    case 4:
      return "tp";
    case 5:
    case 6:
    case 7:
      return "t" + std::to_string(this->rid_ - 5);
    case 8:
      return "fp";
    case 9:
      return "s1";
    }
    if (this->rid_ >= 10 && this->rid_ <= 17)
      return "a" + std::to_string(this->rid_ - 10);
    if (this->rid_ >= 18 && this->rid_ <= 27)
      return "s" + std::to_string(this->rid_ - 16);
    return "t" + std::to_string(this->rid_ - 25);
  }
};

const int REG_NUMBER = 32;
const std::map<std::string, Register *> findReg;

// 常数
class RiscvConst : public RiscvOperand {

public:
  int intval;
  float floatval;
  RiscvConst() = default;
  explicit RiscvConst(int val) : RiscvOperand(IntImm), intval(val) {}
  explicit RiscvConst(float val) : RiscvOperand(FloatImm), floatval(val) {}
  std::string print() {
    if (this->tid_ == IntImm)
      return std::to_string(intval);
    else
      return std::to_string(floatval);
  }
};

// 整型寄存器直接存储
class RiscvIntReg : public RiscvOperand {

public:
  Register *reg_;
  RiscvIntReg(Register *reg) : RiscvOperand(IntReg), reg_(reg) {
    assert(reg_->regtype_ == Register::Int); // 判断整型寄存器存储
  }
  std::string print() { return reg_->print(); }
};

class RiscvFloatReg : public RiscvOperand {

public:
  Register *reg_;
  RiscvFloatReg(Register *reg) : RiscvOperand(IntReg), reg_(reg) {
    assert(reg_->regtype_ == Register::Float); // 判断整型寄存器存储
  }
  std::string print() { return reg_->print(); }
};

// 需间接寻址得到的数据，整型
class RiscvIntPhiReg : public RiscvOperand {

public:
  int shift_;
  Register *base_;
  RiscvIntPhiReg(Register *base, int shift = 0)
      : RiscvOperand(IntMem), base_(base), shift_(shift) {}
  std::string print() {
    std::string ans = "(" + base_->print() + ")";
    if (shift_)
      ans = std::to_string(shift_) + ans;
    return ans;
  }
};

// 需间接寻址得到的数据，浮点
class RiscvFloatPhiReg : public RiscvOperand {

public:
  int shift_;
  Register *base_;
  RiscvFloatPhiReg(Register *base, int shift = 0)
      : RiscvOperand(FloatMem), base_(base), shift_(shift) {}
  std::string print() {
    std::string ans = "(" + base_->print() + ")";
    if (shift_)
      ans = std::to_string(shift_) + ans;
    return ans;
  }
};

class RiscvLabel : public RiscvOperand {
public:
  std::string name_; // 标号名称
  ~RiscvLabel() = default;
  RiscvLabel(OpTy Type, std::string name) : RiscvOperand(Type), name_(name) {}
  virtual std::string print() = 0;
};

// 全局变量
// 需指明是浮点还是整型
// 最后拼装
class RiscvGlobalVariable : public RiscvLabel {
public:
  int shift_;
  RiscvGlobalVariable(OpTy type, std::string name, int shift)
      : RiscvLabel(type, name), shift_(shift) {}
  std::string print() {
    std::string ans = "(" + name_ + ")";
    if (shift_)
      ans = std::to_string(shift_) + ans;
    return ans;
  }
  // 使用一个变量
  std::string print() {
    std::string ans = "(" + this->name_ + ")";
    if (shift_)
      ans = std::to_string(shift_) + ans;
    return ans;
  }
  // 全局变量声明，需传入数组大小
  std::string definition(int size) {
    std::string ans = this->name_ + "\n";
    ans += "\t\t.zero\t" + std::to_string(size * 4) + "\n";
    return ans;
  }
};

// 用标号标识函数
// 函数挂靠在module下，接入若干条instruction
// 需配合alloca、znext语句对栈进行分析
class RiscvFunction : public RiscvLabel {
public:
  RiscvModule *par_;
  int num_args_;
  int callerSP_; // 父函数中SP的值，便于恢复。但是需要正常保存
  OpTy resType_;
  std::vector<RiscvOperand *> args;
  RiscvFunction(std::string name, RiscvModule *par, int num_args, OpTy Ty,
                int callerSP) // 返回值，无返回使用void类型
      : RiscvLabel(Function, name), par_(par), num_args_(num_args),
        resType_(Ty), base_(0), callerSP_(callerSP) {
    par->addFunction(this);
  }
  void setArgs(int ind, RiscvOperand *op) {
    assert(ind >= 0 && ind < args.size());
    args[ind] = op;
  }
  void deleteArgs(int ind) {
    assert(ind >= 0 && ind < args.size());
    args[ind] = nullptr;
  }
  RiscvFunction(std::string name, int num_args, OpTy Ty,
                int callerSP) // 返回值，无返回使用void类型
      : RiscvLabel(Function, name), par_(nullptr), num_args_(num_args),
        resType_(Ty), base_(0), callerSP_(callerSP) {}
  ~RiscvFunction() = default;
  std::string print() { return name_; }
  std::vector<RiscvBasicBlock *> blk;
  bool is_libfunc() {
    if (name_ == "putint" || name_ == "putch" || name_ == "putarray" ||
        name_ == "_sysy_starttime" || name_ == "_sysy_stoptime" ||
        name_ == "__aeabi_memclr4" || name_ == "__aeabi_memset4" ||
        name_ == "__aeabi_memcpy4" || name_ == "getint" || name_ == "getch" ||
        name_ == "getarray" || name_ == "getfloat" || name_ == "getfarray" ||
        name_ == "putfloat" || name_ == "putfarray") {
      return true;
    } else {
      return false;
    }
  }
  std::map<RiscvOperand *, int>
      argsOffset; // 函数使用到的参数（含调用参数、局部变量和返回值）在栈中位置。需满足字对齐（4的倍数）
                  // 届时将根据该函数的参数情况决定sp下移距离
  void addArgs(RiscvOperand *val) // 在栈上新增操作数映射
  {
    if (argsOffset.count(val) == 0) {
      argsOffset[val] = base_;
      stackOrder[base_] = val;
      base_ -= 4;
    }
  }
  void deleteArgs(RiscvOperand *val) { argsOffset.erase(val); } // 删除一个参数
  int findArgs(RiscvOperand *val) // 查询栈上位置
  {
    if (argsOffset.count(val) == 0)
      addArgs(val);
    return argsOffset[val];
  }
  void addBlock(RiscvBasicBlock *bb) { blk.push_back(bb); }
  std::string print()
      override; // 函数语句，需先push保护现场，然后pop出需要的参数，再接入各block
  std::string storeRegisterInstr();   // 输出保护现场的语句
  void addRestoredBlock();
  // 建议函数返回直接使用一个跳转语句跳转到返回语句块
  /*
  预留的栈参数接口，对接alloca、znext等语句
  */
private:
  int base_;
  std::set<RiscvOperand *> storedEnvironment; // 栈中要保护的地址
  std::map<int, RiscvOperand *> stackOrder; // 记录栈中从高到低地址顺序
};

// 语句块，也使用标号标识
// 必须挂靠在函数下，否则无法正常生成标号
// 可以考虑转化到riscv basic block做数据流分析，预留接口
class RiscvBasicBlock : public RiscvLabel {
public:
  RiscvFunction *func_;
  std::vector<RiscvInstr *> instruction;
  int blockInd_; // 表示了各个block之间的顺序
  RiscvBasicBlock(std::string name, RiscvFunction *func, int blockInd)
      : RiscvLabel(Block, name), func_(func),
        blockInd_(blockInd) {
    func->addBlock(this);
  }
  RiscvBasicBlock(std::string name, int blockInd)
      : RiscvLabel(Block, name), func_(nullptr),
        blockInd_(blockInd) {}
  void addFunction(RiscvFunction *func) {
    func->addBlock(this);
  }
  std::string print() { return name_; }
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
  bool addInstrBefore(RiscvInstr *instr) {
    auto it = std::find(instruction.begin(), instruction.end(), instr);
    if (it != instruction.end())
      instruction.insert(it, instr);
  }
  bool addInstrAfter(RiscvInstr *instr, RiscvInstr *dst) {
    auto it = std::find(instruction.begin(), instruction.end(), instr);
    if (it != instruction.end()) {
      if (next(it) == instruction.end())
        instruction.push_back(instr);
      else
        instruction.insert(next(it), instr);
    }
  }
  std::string print() override;
  /*
    此处加入所需数据流分析的参数和函数
    下面为示例
  */
  std::vector<RiscvBasicBlock *> outB; // 出边
  std::vector<RiscvBasicBlock *> inB;  // 入边
};

// 总指针
class RiscvModule {

public:
  std::vector<RiscvFunction *> func_;
  std::vector<RiscvGlobalVariable *> globalVariable_;
  void addFunction(RiscvFunction *foo) { func_.push_back(foo); }
  void addGlobalVariable(RiscvGlobalVariable *g) {
    globalVariable_.push_back(g);
  }
};