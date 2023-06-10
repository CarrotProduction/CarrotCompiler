#include "ir.h"

// 该类中的type用于标识到底是整型还是浮点型
class Register : public Value {

public:
// 寄存器适用类型——专用于栈指令STK、计算指令CAL、存储地址用ADR
  enum RegType {
    CAL = 1,// 计算用，为数值 
    ADR, // 表示变量地址
    STK, // 栈
    UND, // 未定义
    CON // 常量，具体数值在value中name变量保存
  };
  RegType regtype;
  Register(Type *ty, RegType RegTy, const std::string &name = "") : Value(ty, name), regtype(RegTy) {}
  ~Register() = default;
  // value 本身的name用作寄存器名称
  std::string print() override;
};
const int REG_NUMBER = 32;
std::vector<Register> Int, Float; // 寄存器堆

class Position {
public:
  enum StoreType {
    IMM = 1, // 立即数寻址
    REG, // 寄存器本身保存数据
    REG_STORE, // 寄存器保存存储地址
    REG_STORE_SHIFT // 寄存器保存地址+偏移量
    // 暂时不考虑其他寻址方式，如[内存地址+偏移量]使用寄存器+偏移两步实现
  };
  StoreType ty_;
  int shift_; // 地址偏移量
  Register *reg_;
  Position() = default;
  Position(StoreType ty, Register *reg = nullptr, int shift = 0) : ty_(ty), reg_(reg), shift_(shift){}
};

void InitRegFile();
// 在这个文件中实现从IR地址到register的分配和映射，实现完成后将IR地址名称映射到寄存器或寄存器所表示的地址上：Addr2Reg
// 注意：如果某个值用作左值，则必须输出到寄存器。
std::map<std::string, Position> Addr2Reg;
void addrMapping(int op);
void NormalAddrMapping();
void OptimizedAddrMapping();