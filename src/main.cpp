/*
 * Main -- driver for Empirical
 *
 * Copyright (C) 2019 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#include <cstdlib>
#include <iostream>
#include <unordered_map>

#include <empirical.hpp>

#include <sysconfig.hpp>
#include <linenoise.hpp>
#include <string_helpers.hpp>

#include <VVM/utils/timer.hpp>

#include <docopt/docopt.h>

// evaluate Empirical code
std::string eval(const std::string& text, bool interactive = true,
                 bool dump_ast = false, bool dump_hir = false,
                 bool dump_vvm = false) {
  // compile to bytecode
  AST::mod_t ast = parse(text, interactive, dump_ast);
  HIR::mod_t hir = sema(ast, interactive, dump_hir);
  VVM::Program program = codegen(hir, interactive, dump_vvm);

  // interpret bytecode
  return VVM::interpret(program);
}

// evaluate VVM assembly code
std::string eval_asm(const std::string& text, bool dump_vvm = false) {
  // assemble to bytecode
  VVM::Program program = VVM::assemble(text, dump_vvm);

  // interpret bytecode
  return VVM::interpret(program);
}

// read an entire file's contents
std::string read_file(std::string filename) {
  filename = trim(filename);
  if (starts_with(filename, "~")) {
    filename = std::getenv("HOME") + filename.erase(0, 1);
  }
  std::ifstream file(filename);
  if (!file) {
    throw std::runtime_error("Error: unable to read " + filename);
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// read multiline contents from REPL
std::string read_multiline() {
  std::cout << "# Entering multiline mode (Ctrl-D to exit)" << std::endl;
  Linenoise ln;
  std::string result, line;
  while (ln.get_line(line)) {
    result += line + '\n';
  }
  return result;
}

// global variable needed for certain quirks in regression tests
bool testing_mode;

// main function is the driver for getting user input and calling eval()
int main(int argc, char* argv[]) {
  // docopt argument parsing
  const char usage[] =
R"(Empirical programming language

Usage:
  empirical [--dump-ast] [--dump-hir] [--dump-vvm] [--test-mode] [<file>]
  empirical --verify-markdown <file>
  empirical -v | --version
  empirical -h | --help

Options:
  -h --help                 Show this message
  -v --version              Show version number
  --dump-ast                Print abstract syntax tree
  --dump-hir                Print high-level IR
  --dump-vvm                Print Vector Virtual Machine asm
  --test-mode               Indicates regression tests
  --verify-markdown=<file>  Test code segments in file
)";

  std::map<std::string, docopt::value> args
      = docopt::docopt(usage,
                       {argv + 1, argv + argc},
                       true,                 // show help if requested
                       EMPIRICAL_VERSION);   // version string

  bool dump_ast = args["--dump-ast"].asBool();
  bool dump_hir = args["--dump-hir"].asBool();
  bool dump_vvm = args["--dump-vvm"].asBool();

  std::string md_file = args["--verify-markdown"] ?
                        args["--verify-markdown"].asString() : "";
  std::string filename = args["<file>"] ? args["<file>"].asString() : "";

  testing_mode = args["--test-mode"].asBool();
  int ret_code = 0;
  if (filename.empty() && md_file.empty()) {
    // interactive mode
    std::cout << "Empirical version " << EMPIRICAL_VERSION << std::endl;
    std::cout << "Copyright (C) 2019 Empirical Software Solutions, LLC"
              << std::endl << std::endl;
    Linenoise ln(argv[0]);
    std::string line;
    while (ln.get_line(line)) {
      if (line == "quit" || line == "exit") {
        break;
      }
      bool timer_desired = false;

      // check for 'magic' commands
      if (starts_with(line, "\\")) {
        if (starts_with(line, "t ", 1)) {
          timer_desired = true;
          line = line.substr(3);
        }
        else if (starts_with(line, "time ", 1)) {
          timer_desired = true;
          line = line.substr(6);
        }
        else if (starts_with(line, "l ", 1)) {
          std::string contents;
          try {
            contents = read_file(line.substr(3));
            eval(contents, false, dump_ast, dump_hir, dump_vvm);
          }
          catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
          }
          line.clear();
        }
        else if (starts_with(line, "load ", 1)) {
          std::string contents;
          try {
            contents = read_file(line.substr(6));
            eval(contents, false, dump_ast, dump_hir, dump_vvm);
          }
          catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
          }
          line.clear();
        }
        else if (starts_with(line, "multiline", 1)) {
          line = read_multiline();
        }
        else if (starts_with(line, "help", 1)) {
          std::cout << R"(Magic commands:
  \t <expr>, \time <expr> - time execution of an expression
  \l <file>, \load <file> - load a file into global scope
  \multiline              - enter multiple lines of code
  \help                   - print this message)"
                    << std::endl;
          line.clear();
        }
        else {
          std::cerr << "Error: unrecognized magic command " << line
                    << std::endl;
          line.clear();
        }
      }

      if (line != "") {
        // eval and print
        try {
          VVM::Timer timer;
          std::string res = eval(line, true, dump_ast, dump_hir, dump_vvm);
          std::cout << res << std::endl;
          if (!res.empty()) {
            std::cout << std::endl;
          }
          if (timer_desired) {
            timer.check("", "ms");
          }
        }
        catch (std::exception& e) {
          std::cerr << e.what() << std::endl;
        }
      } else {
        std::cout << std::endl;
      }
    }
  }
  else if (!md_file.empty()) {
    // verify mode
    testing_mode = true;
    Tests tests;
    try {
      std::string contents = read_file(md_file);
      tests = parse_markdown(contents);
    }
    catch (std::exception& e) {
      std::cerr << e.what() << std::endl;
      return 1;
    }

    for (auto& t: tests) {
      std::string result;
      try {
        result = eval(t.in);
        result += result.empty() ? "\n" : "\n\n";
      }
      catch (std::exception& e) {
        result = std::string(e.what()) + '\n';
      }

      if (result != t.out) {
        std::cout << ">>> " << t.in;
        std::cout << t.out;
        std::cout << "----" << std::endl;
        std::cout << result;
        ret_code = 1;
      }
    }
  }
  else {
    // file mode
    try {
      std::string contents = read_file(filename);
      if (ends_with(filename, ".vvm")) {
        eval_asm(contents, dump_vvm);
      }
      else {
        eval(contents, false, dump_ast, dump_hir, dump_vvm);
      }
    }
    catch (std::exception& e) {
      std::cerr << e.what() << std::endl;
      return 1;
    }
  }
  return ret_code;
}

