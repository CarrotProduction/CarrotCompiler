// Currently a dummy file
#ifndef OPTH
#define OPTH

#include "ir.h"

class Optimization {
public:
  Module *m;
  explicit Optimization(Module *m_) : m(m_) {}
  virtual void execute() = 0;
};
#endif // !OPTH