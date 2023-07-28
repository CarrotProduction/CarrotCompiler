#include "ast.h"
#include "define.h"
#include "genIR.h"
#include <fstream>
#include <iostream>
#include "backend.h"
#include <ostream>
#include <unistd.h>

extern unique_ptr<CompUnitAST> root;
extern int yyparse();
extern FILE *yyin;

int main(int argc, char **argv) {
  // Assert the number of arguments
  assert(argc >= 2);

  // TODO: advanced argument parser
  char *filename = nullptr;
  int print_ir = false;
  int print_asm = false;

  std::string output = "-";

  int opt;
  while((opt = getopt(argc, argv, "Sco:")) != -1)
  {
    switch (opt) {
      case 'S':
        print_asm = true;
        print_ir = false;
        break;
      case 'c':
        print_ir = true;
        print_asm = false;
        break;
      case 'o':
        output = optarg;
        break;
      default:
        return -1;
    }
  }
  filename = argv[optind];

  yyin = fopen(filename, "r");
  if (yyin == nullptr) {
    std::cout << "yyin open" << filename << "failed" << std::endl;
    return -1;
  }

  // Frontend parser
  yyparse();

  // Generate IR from AST
  GenIR genIR;
  root->accept(genIR);
  std::unique_ptr<Module> m = genIR.getModule();

  // Run IR optimization
  // TODO

  // Open output file
  std::ofstream fout;
  std::ostream *out;
  if (output == "-") {
    out = &std::cout;
  } else {
    fout.open(output);
    out = &fout;
  }

  // Print IR result
  const std::string IR = m->print();
  if (print_ir) {
    *out << IR << std::endl;
  }

  // Generate assembly file
  // TODO
  if (print_asm) {
    auto builder = new RiscvBuilder();
    const std::string RiscvCode = builder->buildRISCV(m.get());
    *out << RiscvCode << std::endl;
  }
  return 0;
}
