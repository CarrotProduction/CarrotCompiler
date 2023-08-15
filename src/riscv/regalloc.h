#ifndef REGALLOCH
#define REGALLOCH

#include "riscv.h"
#include <cassert>

template <typename T> class DSU {
private:
  std::map<T, T> father;
  T getfather(T x) {
    return father[x] == x ? x : (father[x] = getfather(father[x]));
  }

public:
  DSU() = default;
  T query(T id) {
    // 不存在变量初值为自己
    if (father.find(id) == father.end()) {
      // std::cerr << std::hex << "[Debug] [DSU] [" << this << "] New value " <<
      // id
      //           << std::endl;
      return father[id] = id;
    } else {
      // std::cerr << std::hex << "[Debug] [DSU] [" << this << "] Find value "
      // << id
      //           << std::endl;
      return getfather(id);
    }
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
    // std::cerr << std::hex << "[Debug] [DSU] [" << this << "] Merge " << u
    //           << " to " << v << std::endl;
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

  /**
   * 返回 Value 所关联的寄存器操作数。若 Value
   * 未关联寄存器，则分配寄存器并进行关联，并处理相关的数据载入与写回。
   * @param val 需要查找寄存器的 Value
   * @param bb 插入指令需要提供的基本块
   * @param instr 在 instr 之前插入指令（可选）
   * @param inReg 是否一定返回一个寄存器操作数，若为 true 则是
   * @param load 是否将 Value 所对应的物理地址内容载入到寄存器中，若为 true 则是
   * @param specified 是否指定关联的寄存器，若为 nullptr
   * 则不指定，若为一个寄存器操作数则在任何情况下都将 Value
   * 与指定的寄存器进行强制关联。
   * @param direct 仅在 Value 为指针时生效，用于在载入物理地址时传递参数给
   * findMem
   * @return 返回一个 IntegerReg* 或 FloatReg* 类型的操作数 rs 。
   * @attention 该函数将在 Value 为 Alloca 指令时进行特殊处理，在 load=1 时将
   * Alloca 指针所指向的地址载入到分配的寄存器中。
   * @attention 该函数将在 Value 为常量时进行特殊处理，在 load=1 时将常量通过 LI
   * 指令载入到分配的寄存器中。
   * @attention 目前不应使用 direct 参数。
   */
  RiscvOperand *findReg(Value *val, RiscvBasicBlock *bb,
                        RiscvInstr *instr = nullptr, int inReg = 0,
                        int load = 1, RiscvOperand *specified = nullptr,
                        bool direct = true);

  /**
   * 对传递过来的 Value 返回其所处的物理地址操作数 offset(rs) 。
   * @param val 需要查找物理地址的 Value
   * @param bb 插入指令需要提供的基本块
   * @param instr 在 instr 之前插入指令（可选）
   * @param direct 当 direct 为 false 时将使用间接寻址。若使用间接寻址且
   * Value 为指针 (getElementInstr) ，则 findMem()
   * 将会将指针所指向的地址载入临时寄存器 t5，并返回操作数 0(t5) 。
   * @return 返回一个 IntegerPhiReg* 或 FloatPhiReg* 类型的操作数 offset(rs)
   * 。若操作数偏移量溢出，则将会计算返回操作数所指代的地址 (t5)。
   */
  RiscvOperand *findMem(Value *val, RiscvBasicBlock *bb, RiscvInstr *instr,
                        bool direct);
  /**
   * 对传递过来的 Value 返回其所处的物理地址操作数 offset(rs) 。
   * @attention 这是一个重载形式的函数，其不支持间接寻址。
   * @param val 需要查找物理地址的 Value
   */
  RiscvOperand *findMem(Value *val);

  // TODO:2 findNonuse
  // 实现一个函数，以找到一个当前尚未使用的寄存器以存放某个值。
  RiscvOperand *findNonuse(Type *ty, RiscvBasicBlock *bb,
                           RiscvInstr *instr = nullptr);

  /**
   * 将 Value 与指定的寄存器强制关联并返回寄存器操作数。
   * @param val 需要查找寄存器的 Value
   * @param RegName 需要强制关联的寄存器接口名称
   * @param bb 插入指令需要提供的基本块
   * @param instr 在 instr 之前插入指令（可选）
   * @param direct 仅在 Value 为指针时生效，用于在载入物理地址时传递参数给
   * findMem
   * @return 返回强制关联的寄存器操作数 rs 。
   * @note 该函数将在 Value 为 Alloca 指令时进行特殊处理，在 load=1 时将
   * Alloca 指针所指向的地址载入到分配的寄存器中。
   * @note 该函数将在 Value 为常量时进行特殊处理，在 load=1 时将常量通过 LI
   * 指令载入到分配的寄存器中。
   * @attention 目前不应使用 direct 参数。
   */
  RiscvOperand *findSpecificReg(Value *val, std::string RegName,
                                RiscvBasicBlock *bb,
                                RiscvInstr *instr = nullptr,
                                bool direct = true);

  /**
   * 将 Value 与指定的物理地址操作数 offset(rs) 相关联。
   * @param val 需要关联的 Value
   * @param riscvVal 被关联的物理地址操作数 offset(rs)
   * @attention 该函数也被用于关联 Alloca 指针与其指向的地址。
   */
  void setPosition(Value *val, RiscvOperand *riscvVal);

  /**
   * 将 Value 与指定的寄存器 rs 相关联。若寄存器内已有值，则将值写回。
   * @param val 需要关联的 Value
   * @param riscvReg 被关联的寄存器 rs
   * @param bb 用于插入的基本块
   * @param instr 在 instr 之前插入指令（可选）
   * @attention 在大多数情况下你不应直接使用此函数。作为替代，你应该使用 findReg
   * 与 findSpecificReg 函数来进行关联。
   */
  void setPositionReg(Value *val, RiscvOperand *riscvReg, RiscvBasicBlock *bb,
                      RiscvInstr *instr = nullptr);
  /**
   * 将 Value 与指定的寄存器 rs 相关联。若寄存器内已有值，则将值写回。
   * @param val 需要关联的 Value
   * @param riscvReg 被关联的寄存器 rs
   * @attention 在大多数情况下你不应直接使用此函数。作为替代，你应该使用 findReg
   * 与 findSpecificReg 函数来进行关联。
   */
  void setPositionReg(Value *val, RiscvOperand *riscvReg);

  /**
   * 记录 getElementPtr 类型的 Value 所指向的常量相对物理地址操作数 offset(sp)
   * 。
   * @param val 指针类型的 Value
   * @param PointerMem 被关联的操作数 offset(sp)
   * @attention offset 只能为一常量。
   */
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
  /**
   * 将 Value 所关联的寄存器写回到内存地址中，并移除该寄存器在pos中映射的地址。
   * @param val 将要写回的 Value
   * @param bb 指令将要插入的基本块
   * @param instr 当前需要在哪一条指令前方插入sw指令
   * @return 返回最后写入的指令
   */
  RiscvInstr *writeback(Value *val, RiscvBasicBlock *bb,
                        RiscvInstr *instr = nullptr);

  /**
   * 返回寄存器 reg 所对应的 Value 。
   */
  Value *getRegPosition(RiscvOperand *reg);

  /**
   * 返回 Value 所对应的寄存器 reg 。
   */
  RiscvOperand *getPositionReg(Value *val);

  /**
   * 保护的寄存器对象数组。
   */
  std::vector<RiscvOperand *> savedRegister;

  /**
   * 初始化寄存器对象池，需要保护的寄存器对象等。
   */
  RegAlloca();

  // 指针所指向的内存地址
  std::map<Value *, RiscvOperand *> ptrPos;

  /**
   * 返回指针类型的 Value 所指向的常量相对物理地址操作数 offset(sp) 。
   * @attention 参数 bb, instr 目前不被使用。
   */
  RiscvOperand *findPtr(Value *val, RiscvBasicBlock *bb,
                        RiscvInstr *instr = nullptr);

  /**
   * 写回所有与内存存在关联的寄存器并删除关联。
   * @param bb 被插入基本块
   * @param instr 在特定指令前插入
   */
  void writeback_all(RiscvBasicBlock *bb, RiscvInstr *instr = nullptr);

  /**
   * 清空所有寄存器关系。
   */
  void clear();

  /**
   * 返回所有被使用过的寄存器集合。
   */
  std::set<RiscvOperand *> getUsedReg() { return regUsed; }

private:
  std::map<Value *, RiscvOperand *> pos, curReg;
  std::map<RiscvOperand *, Value *> regPos;
  /**
   * 安全寄存器寻找。用于确保寄存器在被寻找之后的 SAFE_FIND_LIMIT
   * 个时间戳内不被写回。
   */
  std::map<RiscvOperand *, int> regFindTimeStamp;
  int safeFindTimeStamp = 0;
  static const int SAFE_FIND_LIMIT = 3;
  /**
   * 被使用过的寄存器。
   */
  std::set<RiscvOperand *> regUsed;
};

/**
 * 寄存器对象池。
 */
static std::vector<RiscvOperand *> regPool;

/**
 * 根据提供的寄存器名，从寄存器池中返回操作数。
 */
RiscvOperand *getRegOperand(std::string reg);

/**
 * 根据提供的寄存器类型与编号，从寄存器池中返回操作数。
 */
RiscvOperand *getRegOperand(Register::RegType op_ty_, int id);

#endif // !REGALLOCH