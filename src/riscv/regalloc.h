#ifndef REGALLOCH
#define REGALLOCH

#include "riscv.h"
#include <cassert>

template <typename T> class DSU {

  std::map<T, T> father;
  T getfather(T x) {
    return father[x] == x ? x : (father[x] = getfather(father[x]));
  }

public:
  DSU() = default;
  T query(T id) {
    // 不存在变量初值为自己
    if (father.find(id) == father.end())
      return father[id] = id;
    else
      return getfather(id);
  }

  /**
   * Merge DSU's node u to v.
   * @param u child
   * @param v father
   */
  void merge(T u, T v) {
    u = query(u), v = query(v);
    assert(u != nullptr && v != nullptr);
    if (u == v)
      return;
    father[u] = v;
  }
};

// 关于额外发射指令问题说明
// 举例：如果当前需要使用特定寄存器（以a0为例）以存取返回值
// 1. 如果当前变量在内存：a)
// a)
// 由regalloca发射一个sw指令（使用rbb中addInstrback函数）将现在的a0送回对应内存或栈上地址
// b) 由regalloca发射一个lw指令（使用rbb中addInstrback函数）将该变量移入a0中
// 2. 如果该变量在寄存器x中：
// a)
// 由regalloca发射一个sw指令（使用rbb中addInstrback函数）将现在的a0送回对应内存或栈上地址
// b) 由regalloca发射一个mv指令（使用rbb中addInstrback函数）将该变量从x移入a0中

// 举例：为当前一个未指定寄存器，或当前寄存器堆中没有存放该变量的寄存器。现在需要为该变量找一个寄存器以进行运算
// 存在空寄存器：找一个空闲未分配寄存器，然后返回一个寄存器指针riscvOperand*
// 不存在空寄存器：
// a) 找一个寄存器
// b)
// 由regalloca发射一个sw指令（使用rbb中addInstrback函数）将现在该寄存器的数字送回对应内存或栈上地址
// c) 返回该寄存器指针riscvOperand*

// 注意区分指针类型（如*a0）和算数值（a0）的区别

// 每个变量会有一个固定的内存或栈地址，可能会被分配一个固定寄存器地址

extern int IntRegID, FloatRegID; // 测试阶段使用

Register *NamefindReg(std::string reg);
RiscvOperand *getRegOperand(std::string reg);

// 辅助函数
// 根据寄存器 riscvReg 的类型返回存储指令的类型
Type *getStoreTypeFromRegType(RiscvOperand *riscvReg);

// RegAlloca类被放置在**每个函数**内，每个函数内是一个新的寄存器分配类。
// 因而约定x8-x9 x18-27、f8-9、f18-27
// 是约定的所有函数都要保护的寄存器，用完要恢复原值
// 其他的寄存器（除函数参数所用的a0-a7等寄存器）都视为是不安全的，可能会在之后的运算中发生变化
// TODO:3
// 在该类的实例生存周期内，使用到的需要保护的寄存器使用一个vector<Register*>
// 存储

// 寄存器分配（IR变量到汇编变量地址映射）
// 所有的临时变量均分配在栈上（从当前函数开始的地方开始计算栈地址，相对栈偏移地址），所有的全局变量放置在内存中（首地址+偏移量形式）
// 当存在需要寄存器保护的时候，直接找回原地址去进行
class RegAlloca {
public:
  DSU<Value *> DSU_for_Variable;
  // 如果所有寄存器均被分配，则需要额外发射lw和sw指令
  // inReg 参数如果为1，则要求必须返回寄存器；否则可以返回内存或栈上地址
  // instr 表示需要插入到哪条指令的前面，默认为最后面

  // TODO:1 find函数
  // 实现一个find——对于IR的一个给定值value*，找到一个寄存器（RiscvOperand*，返回值）以存放该值
  // bb是当前访问该变量的IR语句所在的RISCVBasicBlock块，如果当需要发射lw和sw指令时，使用该bb调用
  // instr是当前需要在哪一条指令**前面**插入lw和sw指令，默认为nullptr表示插入到bb最后
  // inReg参数表示该分配地址是否必须是寄存器。如果为0则可以为内存或栈地址
  // load: 是否将值对应的内存地址读取到寄存器中
  RiscvOperand *findReg(Value *val, RiscvBasicBlock *bb,
                        RiscvInstr *instr = nullptr, int inReg = 0,
                        int load = 1, RiscvOperand *specified = nullptr);

  // 找到IR的value的内存地址（用于IR级别的store和load指令）
  // 如果regalloca中没有存储它的内存地址（没有被setPosition过），assert报错
  RiscvOperand *findMem(Value *val, RiscvBasicBlock *bb, RiscvInstr *instr);
  RiscvOperand *findMem(Value *val);

  // TODO:2 findNonuse
  // 实现一个函数，以找到一个当前尚未使用的寄存器以存放某个值。
  RiscvOperand *findNonuse(RiscvBasicBlock *bb, RiscvInstr *instr = nullptr);

  // TODO:5 findSpecificReg
  // 将一个IR的value*找到一个特定的寄存器（以RegName标识）进行分配
  // bb是当前访问该变量的IR语句所在的RISCVBasicBlock块，如果当需要发射lw和sw指令时，使用该bb调用
  // instr是当前需要在哪一条指令**前面**插入lw和sw指令，默认为nullptr表示插入到bb最后
  RiscvOperand *findSpecificReg(Value *val, std::string RegName,
                                RiscvBasicBlock *bb,
                                RiscvInstr *instr = nullptr);

  // 建立IR中变量到实际变量（内存固定地址空间，用riscvVal指针表示）或寄存器的映射
  void setPosition(Value *val, RiscvOperand *riscvVal);

  // 建立IR中变量到实际寄存器（用riscvReg指针表示）的映射
  void setPositionReg(Value *val, RiscvOperand *riscvReg, RiscvBasicBlock *bb,
                      RiscvInstr *instr = nullptr);
  void setPositionReg(Value *val, RiscvOperand *riscvReg);
  // 建立指针指向的内存地址映射
  void setPointerPos(Value *val, RiscvOperand *PointerMem);

  /**
   * 将寄存器内容写回到内存地址中，并移除该寄存器在pos中映射的地址。
   * @param riscvReg 将要写回的寄存器
   * @param bb 指令将要插入的基本块
   * @param instr 当前需要在哪一条指令前方插入sw指令
   * @return 返回最后写入的指令
   */
  RiscvInstr *writeback(RiscvOperand *riscvReg, RiscvBasicBlock *bb,
                        RiscvInstr *instr = nullptr);

  // 返回 riscvReg 所对应的 Value
  Value *findRegVal(RiscvOperand *riscvReg);

  // 根据返回值是浮点型还是整型决定使用什么寄存器
  // RiscvOperand *storeRet(Value *val);
  // TODO:6
  // 使用到的callee保存寄存器，使用这个vector存储。具体为x8-x9,x18-x27（整型）；f8-f9,f18-f27（浮点）
  // 这些寄存器会在函数开始的时候压入栈中，return时会恢复
  std::vector<RiscvOperand *> savedRegister;

  // Initialize savedRegister
  RegAlloca();

  std::map<Value *, RiscvOperand *> ptrPos; // 指针所指向的内存地址
  Value *getRegPosition(RiscvOperand *reg);
  RiscvOperand *findPtr(Value *val, RiscvBasicBlock *bb,
                        RiscvInstr *instr = nullptr);

private:
  std::map<Value *, RiscvOperand *> pos, curReg;
  std::map<RiscvOperand *, Value *> regPos;
};
#endif // !REGALLOCH