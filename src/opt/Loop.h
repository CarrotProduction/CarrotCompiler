#ifndef SYSYCOMPILER_LOOPSEARCH_H
#define SYSYCOMPILER_LOOPSEARCH_H 
#include "Pass.h"
#include "stack"
class Loop;
class LoopBlock;
class LoopSearch : public Pass{
private:
    const std::string name = "LoopSearch";
    std::map<Function *, std::set<Loop *>> fun_loop;
    std::vector<Loop *> work_list;
    std::map<BasicBlock *, int> DFN;
    std::map<BasicBlock *, int> Low;
    std::stack<BasicBlock *> loop_stack;
    std::set<BasicBlock *> in_stack;
    std::set<BasicBlock *> visited;
    std::set<std::pair<BasicBlock *, BasicBlock *>> edges;
    std::map<Loop *, std::set<Loop *>> child_loop;
    std::map<Function *, std::map<BasicBlock *, Loop *>> _nested_loop;
    int index = 0;
    Function *cur_fun;
    Loop *get_inner_loop(const std::set<Loop *>& l, BasicBlock *bb);
    void build_pre_succ_relation(Loop *loop);
    void build_map();
public:
    LoopSearch(Module *m) : Pass(m){}
    ~LoopSearch(){};
    const std::string get_name() const override {return name;}
    std::set<Loop *> get_child_loop(Loop *loop) { return child_loop[loop]; }
    std::set<Loop *> get_loop(Function *fun) { return fun_loop[fun]; }
    Loop *get_outer_loop(Loop *loop) {
        Loop *processing = nullptr;
        for (const auto& parent: child_loop) {
            auto &set = parent.second;
            if (set.find(loop) != set.end()) {
                if (set.find(processing) == set.end()) {
                    processing = parent.first;
                }
            }
        }
        return processing;
    }
    Loop *get_smallest_loop(Function *f, BasicBlock *bb) { return _nested_loop[f][bb]; }
    Loop *get_smallest_loop(BasicBlock *bb) { return _nested_loop[bb->parent_][bb]; }
    void Tarjan(BasicBlock *bb, std::set<BasicBlock *> blocks);
    void execute() override;
};
class Loop{
private:
    BasicBlock *entry_block;
    std::map<BasicBlock *, std::set<BasicBlock *>> loop_pre;
    std::map<BasicBlock *, std::set<BasicBlock *>> loop_succ;
    std::set<BasicBlock *> loop_block;
public:
    std::set<BasicBlock *> get_loop() const { return loop_block; };
    void add_loop_block(BasicBlock *bb) { loop_block.insert(bb); }
    std::set<BasicBlock *> get_loop_bb_pre(BasicBlock *bb) { return loop_pre[bb]; }
    void add_loop_bb_pre(BasicBlock *bb, BasicBlock *pre) { loop_pre[bb].insert(pre); }
    void add_loop_bb_succ(BasicBlock *bb, BasicBlock *succ) { loop_succ[bb].insert(succ); }
    std::set<BasicBlock *> get_loop_bb_succ(BasicBlock *bb) { return loop_succ[bb]; }
    BasicBlock *get_loop_entry() const { return entry_block; }
    void set_entry_block(BasicBlock *entry) { this->entry_block = entry; }
    bool contain_bb(BasicBlock *bb) const { return loop_block.find(bb) != loop_block.end(); }
    BasicBlock *getLoopLatch() const {
        BasicBlock *header = get_loop_entry();
        BasicBlock *bb = nullptr;
        for (auto pre: header->pre_bbs_) {
            if (contain_bb(pre)) {
                if (bb != nullptr) {
                    return nullptr;
                }
                bb = pre;
            }
        }
        return bb;
    }
    void getExitingBB(std::vector<BasicBlock *> &ret) const {
        for (auto bb: get_loop()) {
            for (auto succ: bb->succ_bbs_) {
                if (!contain_bb(succ)) {
                    ret.push_back(bb);
                    break;
                }
            }
        }
    }
    BasicBlock *getExitingBB() const {
        std::vector<BasicBlock *> ret;
        getExitingBB(ret);
        if (ret.size() != 1) {
            return nullptr;
        }
        return ret[0];
    }
};
#endif
