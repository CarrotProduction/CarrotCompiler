#ifndef RISCVH
#define RISCVH

class RiscvLabel;
class RiscvBasicBlock;
class RiscvInstr;
class RegAlloca;
class Register;
class RiscvOperand;

const int VARIABLE_ALIGN_BYTE = 8;

#include "ir.h"
#include "string.h"

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
  OpTy getType();

  // If this operand is a register, return true.
  bool isRegister();
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
    using std::to_string;
    if (this->regtype_ == Float) {
      if (this->rid_ <= 7)
        return "ft" + to_string(rid_);
      else if (this->rid_ <= 9)
        return "fs" + to_string(rid_ - 8);
      else if (this->rid_ <= 17)
        return "fa" + to_string(rid_ - 10);
      else if (this->rid_ <= 27)
        return "fs" + to_string(rid_ - 18 + 2);
      else if (this->rid_ <= 31)
        return "ft" + to_string(rid_ - 28 + 8);
      else
        return "wtf";
    }
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
      return "t" + to_string(this->rid_ - 5);
    case 8:
      return "fp"; // another name: s0
    case 9:
      return "s1";
    }
    if (this->rid_ >= 10 && this->rid_ <= 17)
      return "a" + to_string(this->rid_ - 10);
    if (this->rid_ >= 18 && this->rid_ <= 27)
      return "s" + to_string(this->rid_ - 16);
    return "t" + to_string(this->rid_ - 25);
  }
};

extern const int REG_NUMBER;

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
  RiscvFloatReg(Register *reg) : RiscvOperand(FloatReg), reg_(reg) {
    assert(reg_->regtype_ == Register::Float); // 判断整型寄存器存储
  }
  std::string print() { return reg_->print(); }
};

// 需间接寻址得到的数据，整型
class RiscvIntPhiReg : public RiscvOperand {

public:
  int shift_;
  int isGlobalVariable;
  Register *base_;
  std::string MemBaseName;
  RiscvIntPhiReg(Register *base, int shift = 0, int isGVar = false)
      : RiscvOperand(IntMem), base_(base), shift_(shift),
        isGlobalVariable(isGVar), MemBaseName(base_->print()) {}
  // 内存以全局形式存在的变量（常量）
  RiscvIntPhiReg(std::string s, int shift = 0, int isGVar = false)
      : RiscvOperand(IntMem), base_(nullptr), shift_(shift), MemBaseName(s),
        isGlobalVariable(isGVar) {}
  std::string print() {
    std::string ans = "";
    if (base_ != nullptr)
      ans += "(" + base_->print() + ")";
    else {
      if (isGlobalVariable)
        return MemBaseName; // If global variable, use direct addressing
      else
        ans += "(" + MemBaseName + ")";
    }
    if (shift_)
      ans = std::to_string(shift_) + ans;
    return ans;
  }
  /**
   * Return if shift value overflows.
   */
  bool overflow() { return std::abs(shift_) >= 1024; }
};

// 需间接寻址得到的数据，浮点
class RiscvFloatPhiReg : public RiscvOperand {

public:
  int shift_;
  Register *base_;
  std::string MemBaseName;
  int isGlobalVariable;
  RiscvFloatPhiReg(Register *base, int shift = 0, int isGVar = false)
      : RiscvOperand(FloatMem), base_(base), shift_(shift),
        isGlobalVariable(isGVar), MemBaseName(base_->print()) {}
  // 内存以全局形式存在的变量（常量）
  RiscvFloatPhiReg(std::string s, int shift = 0, int isGVar = false)
      : RiscvOperand(FloatMem), base_(nullptr), shift_(shift), MemBaseName(s),
        isGlobalVariable(isGVar) {}
  std::string print() {
    std::string ans = "";
    if (base_ != nullptr)
      ans += "(" + base_->print() + ")";
    else {
      if (isGlobalVariable)
        return MemBaseName; // If global variable, use direct addressing
      else
        ans += "(" + MemBaseName + ")";
    }
    if (shift_)
      ans = std::to_string(shift_) + ans;
    return ans;
  }
  /**
   * Return if shift value overflows.
   */
  bool overflow() { return std::abs(shift_) >= 1024; }
};

class RiscvLabel : public RiscvOperand {
public:
  std::string name_; // 标号名称
  ~RiscvLabel() = default;
  RiscvLabel(OpTy Type, std::string name) : RiscvOperand(Type), name_(name) {
    // std::cout << "CREATE A LABEL:" << name << "\n";
  }
  virtual std::string print() = 0;
};

// 全局变量
// 需指明是浮点还是整型
// 最后拼装
class RiscvGlobalVariable : public RiscvLabel {
public:
  bool isConst_;
  bool isData; // 是否是给定初值的变量
  int elementNum_;
  Constant *initValue_;
  // 对于一般单个全局变量的定义
  RiscvGlobalVariable(OpTy Type, std::string name, bool isConst,
                      Constant *initValue)
      : RiscvLabel(Type, name), isConst_(isConst), initValue_(initValue),
        elementNum_(1) {}
  // 对于数组全局变量的定义
  RiscvGlobalVariable(OpTy Type, std::string name, bool isConst,
                      Constant *initValue, int elementNum)
      : RiscvLabel(Type, name), isConst_(isConst), initValue_(initValue),
        elementNum_(elementNum) {}
  // 输出全局变量定义
  // 根据ir中全局变量定义转化
  // 问题在于全局变量如果是数组有初值如何处理
  std::string print();
  std::string print(bool print_name, Constant *initVal);
};

// 用标号标识函数
// 函数挂靠在module下，接入若干条instruction，以及function不设置module指针
// 默认不保护现场，如果当寄存器不够的时候再临时压栈
// 父函数调用子函数的参数算在子函数的栈空间内，子函数结束后由子函数清除这部分栈空间
class RiscvFunction : public RiscvLabel {
public:
  RegAlloca *regAlloca;
  int num_args_;
  OpTy resType_;
  std::vector<RiscvOperand *> args;
  RiscvFunction(std::string name, int num_args,
                OpTy Ty); // 返回值，无返回使用void类型
  void setArgs(int ind, RiscvOperand *op) {
    assert(ind >= 0 && ind < args.size());
    args[ind] = op;
  }
  void deleteArgs(int ind) {
    assert(ind >= 0 && ind < args.size());
    args[ind] = nullptr;
  }
  ~RiscvFunction() = default;
  std::string printname() { return name_; }
  std::vector<RiscvBasicBlock *> blk;
  bool is_libfunc() {
    if (name_ == "putint" || name_ == "putch" || name_ == "putarray" ||
        name_ == "_sysy_starttime" || name_ == "_sysy_stoptime" ||
        name_ == "__aeabi_memclr4" || name_ == "__aeabi_memset4" ||
        name_ == "__aeabi_memcpy4" || name_ == "getint" || name_ == "getch" ||
        name_ == "getarray" || name_ == "getfloat" || name_ == "getfarray" ||
        name_ == "putfloat" || name_ == "putfarray" ||
        name_ == "llvm.memset.p0.i32") {
      return true;
    } else
      return false;
  }
  std::map<RiscvOperand *, int>
      argsOffset; // 函数使用到的参数（含调用参数、局部变量和返回值）在栈中位置。需满足字对齐（4的倍数）
                  // 届时将根据该函数的参数情况决定sp下移距离
  void addArgs(RiscvOperand *val) { // 在栈上新增操作数映射
    if (argsOffset.count(val) == 0) {
      argsOffset[val] = base_;
      base_ -= VARIABLE_ALIGN_BYTE;
    }
  }
  int querySP() { return base_; }
  void setSP(int SP) { base_ = SP; }
  void addTempVar(RiscvOperand *val) {
    addArgs(val);
    tempRange += VARIABLE_ALIGN_BYTE;
  }
  void shiftSP(int shift_value) { base_ += shift_value; }
  void storeArray(int elementNum) {
    if(elementNum & 7) {
      elementNum += 8 - (elementNum & 7);   // Align to 8 byte.
    }
    base_ -= elementNum;
  }
  void deleteArgs(RiscvOperand *val) { argsOffset.erase(val); } // 删除一个参数
  // 默认所有寄存器不保护
  // 如果这个时候寄存器不够了，则临时把其中一个寄存器对应的值压入栈上，等函数结束的时候再恢复
  // 仅考虑函数内部SP相对关系而不要计算其绝对关系
  void saveOperand(RiscvOperand *val) {
    storedEnvironment[val] = base_;
    argsOffset[val] = base_;
    base_ -= VARIABLE_ALIGN_BYTE;
  }
  int findArgs(RiscvOperand *val) { // 查询栈上位置
    if (argsOffset.count(val) == 0)
      addArgs(val);
    return argsOffset[val];
  }
  void ChangeBlock(RiscvBasicBlock *bb, int ind) {
    assert(ind >= 0 && ind < blk.size());
    blk[ind] = bb;
  }
  void addBlock(RiscvBasicBlock *bb) { blk.push_back(bb); }
  std::string
  print(); // 函数语句，需先push保护现场，然后pop出需要的参数，再接入各block
private:
  int base_;
  int tempRange; // 局部变量的数量，需要根据这个数量进行栈帧下移操作
  std::map<RiscvOperand *, int>
      storedEnvironment; // 栈中要保护的地址。该部分需要在函数结束的时候全部恢复
};

class RiscvModule {
public:
  std::vector<RiscvFunction *> func_;
  std::vector<RiscvGlobalVariable *> globalVariable_;
  void addFunction(RiscvFunction *foo) { func_.push_back(foo); }
  void addGlobalVariable(RiscvGlobalVariable *g) {
    globalVariable_.push_back(g);
  }
};

Type *findPtrType(Type *ty);

RiscvFunction *createSyslibFunc(Function *foo);
#endif // !RISCVH