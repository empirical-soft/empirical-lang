/*
 * Main -- driver for Empirical
 *
 * Copyright (C) 2019--2020 Empirical Software Solutions, LLC
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

const std::string empirical_routines =
R"(
data CsvProvider{filename: String} = compile(_csv_infer(filename))

func csv_load{T}(filename: String) -> !T => _csv_load(filename, !T)

func load($ filename: String) => csv_load{CsvProvider{filename}}(filename)

func store(df, filename: String) => _csv_store(type_of(df), df, filename)

func String(x) => _repr(x, type_of(x))

func print(x) => _print(String(x))

func len[T](xs: !T) => len(compile("xs." + members_of(xs)[0]))

func reverse[T](df: !T) -> type_of(df) => _reverse(df, type_of(df))
)";

const std::string empirical_help =
R"(
# Types

  Bool        true, false
  Char        'a', '\n'
  String      "Hello", "\nWorld"
  Int64       45, nil
  Float64     4.5, nan
  Date        Date("2020-08-01")
  Time        Time("12:30:00"), Time("12:30:00.000123")
  Timestamp   Timestamp("2020-08-01 12:30:00")
  Timedelta   5m, Timedelta("00:05:00")

# Operators

  comparison:    == != > >= < <=
  arithmetic:    + - * / %
  bitwise:       & | << >>
  boolean:       and or not

# Common Functions

  bar      count    exit     len      load     now
  print    prod     range    reverse  store    sum

# Trigonometry

  acos   acosh  asin   asinh  atan   atanh
  cos    cosh   sin    sinh   tan    tanh

https://www.empirical-soft.com)";

// global variables from command line; extern these as needed
bool kDumpAst = false;
bool kDumpHir = false;
bool kDumpVvm = false;
bool kTestingMode = false;

// evaluate Empirical code
std::string eval(const std::string& text, bool interactive = true) {
  // compile to bytecode
  AST::mod_t ast = parse(text, interactive, kDumpAst);
  HIR::mod_t hir = sema(ast, interactive, kDumpHir);
  VVM::Program program = codegen(hir, VVM::Mode::kRuntime,
                                 interactive, kDumpVvm);

  // interpret bytecode
  return VVM::interpret(program, VVM::Mode::kRuntime);
}

// evaluate VVM assembly code
std::string eval_asm(const std::string& text) {
  // assemble to bytecode
  VVM::Program program = VVM::assemble(text, kDumpVvm);

  // interpret bytecode
  return VVM::interpret(program, VVM::Mode::kRuntime);
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

// main function is the driver for getting user input and calling eval()
int main(int argc, char* argv[]) {
  // setup
  eval(empirical_routines);

  // docopt argument parsing
  const char usage[] =
R"(Empirical programming language

Usage:
  empirical [--dump-ast] [--dump-hir] [--dump-vvm] [--test-mode] [<file> [<args> ...]]
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

  kDumpAst = args["--dump-ast"].asBool();
  kDumpHir = args["--dump-hir"].asBool();
  kDumpVvm = args["--dump-vvm"].asBool();
  kTestingMode = args["--test-mode"].asBool();

  std::string md_file = args["--verify-markdown"] ?
                        args["--verify-markdown"].asString() : "";
  std::string filename = args["<file>"] ? args["<file>"].asString() : "";

  std::vector<std::string> cli_args = args["<args>"].asStringList();
  if (!filename.empty()) {
    cli_args.insert(cli_args.begin(), filename);
  }
  set_argv(cli_args);

  int ret_code = 0;
  if (filename.empty() && md_file.empty()) {
    // interactive mode
    std::cout << "Empirical version " << EMPIRICAL_VERSION << std::endl;
    std::cout << "Copyright (C) 2019--2020 Empirical Software Solutions, LLC"
              << std::endl;
    std::cout << "Type '?' for help. Type '\\help' for magic commands."
              << std::endl << std::endl;
    Linenoise ln(argv[0]);
    // uncomment the below line to work with LLDB
    //ln.disable();
    std::string line;
    while (ln.get_line(line)) {
      if (line == "quit" || line == "exit") {
        break;
      }
      bool timer_desired = false;

      // check for '?'
      if (line == "?") {
        std::cout << empirical_help << std::endl;
        line.clear();
      }

      // check for 'magic' commands
      if (starts_with(line, "\\")) {
        if (starts_with(line, "t ", 1)) {
          timer_desired = true;
          line = line.substr(3);
        } else if (starts_with(line, "time ", 1)) {
          timer_desired = true;
          line = line.substr(6);
        } else if (starts_with(line, "l ", 1)) {
          std::string contents;
          try {
            contents = read_file(line.substr(3));
            eval(contents, false);
          }
          catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
          }
          line.clear();
        } else if (starts_with(line, "load ", 1)) {
          std::string contents;
          try {
            contents = read_file(line.substr(6));
            eval(contents, false);
          }
          catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
          }
          line.clear();
        } else if (starts_with(line, "multiline", 1)) {
          line = read_multiline();
        } else if (starts_with(line, "help", 1)) {
          std::cout << R"(Magic commands:
  \t <expr>, \time <expr> - time execution of an expression
  \l <file>, \load <file> - load a file into global scope
  \multiline              - enter multiple lines of code
  \help                   - print this message)"
                    << std::endl;
          line.clear();
        } else {
          std::cerr << "Error: unrecognized magic command " << line
                    << std::endl;
          line.clear();
        }
      }

      if (line != "") {
        // eval and print
        try {
          VVM::Timer timer;
          std::string res = eval(line);
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
        catch (VVM::ExitException& e) {
          std::cerr << "To exit: use 'exit', 'quit', or Ctrl-D" << std::endl
                    << std::endl;
        }
      } else {
        std::cout << std::endl;
      }
    }
  } else if (!md_file.empty()) {
    // verify mode
    kTestingMode = true;
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
      catch (VVM::ExitException& e) {
        result = "To exit: use 'exit', 'quit', or Ctrl-D\n\n";
      }

      if (result != t.out) {
        std::cout << ">>> " << t.in;
        std::cout << t.out;
        std::cout << "----" << std::endl;
        std::cout << result;
        ret_code = 1;
      }
    }
  } else {
    // file mode
    try {
      std::string contents = read_file(filename);
      if (ends_with(filename, ".vvm")) {
        eval_asm(contents);
      } else {
        eval(contents, false);
      }
    }
    catch (std::exception& e) {
      std::cerr << e.what() << std::endl;
      return 1;
    }
    catch (VVM::ExitException& e) {
      return e.n;
    }
  }
  return ret_code;
}

