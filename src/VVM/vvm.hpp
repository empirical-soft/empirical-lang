/*
 * VVM -- core logic for the Vector Virtual Machine
 *
 * Copyright (C) 2019--2021 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#pragma once

#include <map>
#include <cctype>
#include <vector>
#include <string>
#include <sstream>
#include <exception>
#include <unordered_map>

#include <VVM/types.h>
#include <VVM/opcodes.h>

/*
 * Instructions (instr) in VVM are an opcode and any number of operands. These
 * are all numerical values. The opcode is dispatched to a function in the
 * interpreter. The operands may represent a register (local, global, or
 * state), an immediate value, or a type parameter. Registers can hold scalars,
 * vectors, or Dataframes; they can be of any type, but immediates must be
 * small scalar integers.
 *
 * Directives in VVM can pre-set a global register (to define the constant
 * pool) or declare type definitions. Note that the constant pool can only
 * take scalars, though they may be of any builtin type.
 *
 * The logic in this file contains the bytecode handling (encode/decode logic)
 * for both instructions and directives.
 */
namespace VVM {
typedef std::vector<size_t> instructions_t;

/*** defined types ***/

// a type is tagged as either built-in or user-defined
typedef size_t type_t;

// a type definition is a collection of types
struct named_type_t {
  type_t      typee;
  std::string name;
};
typedef std::vector<named_type_t> type_definition_t;

// the type definitions map a new type to the underlying definition
typedef std::map<type_t, type_definition_t> defined_types_t;

// tag a type with where it's defined
enum class TypeMask: type_t {
  kBuiltIn     = 0,
  kUserDefined = 1
};

/*** function definitions ***/

struct FunctionDef {
  std::string       name;
  type_definition_t args;
  type_t            rettype;
  instructions_t    body;
};

/*** operands ***/

// an operand is a tagged integer; it can represent an immediate value, a
// value stored in a local or global register, or a type parameter
typedef type_t operand_t;

// tag an operands's least-significant bits to indicate what it represents
enum class OpMask : operand_t {
  kImmediate = 0,
  kLocal     = 1,
  kGlobal    = 2,
  kState     = 3,
  kType      = 4
};

/*** constant pool ***/

// a tagged pointer includes type information useful for disassembly
typedef void* tagged_ptr_t;

// the pool maps a register (operand) to a tagged pointer
typedef std::map<operand_t, tagged_ptr_t> const_pool_t;

// tag a pointer with its type
enum class PtrMask: size_t {
  kInt     = 0,
  kFloat   = 1,
  kStr     = 2,
  kFuncDef = 3
};

/*** whole programs ***/

// a program is the instructions and directives (constants and types)
struct Program {
  instructions_t  instructions;
  const_pool_t    constants;
  defined_types_t types;
};

/*** miscellaneous ***/

// which direction to perform asof join
enum class AsofDirection: int64_t {
  kBackward = 0, kForward = 1, kNearest = 2
};

// whether an invocation of VVM is for runtime or comptime
enum class Mode: size_t {
  kRuntime,
  kComptime
};

/*** exceptions ***/

struct ExitException {
  int64_t n;
  explicit ExitException(int64_t n_): n(n_) {}
};

/*** forward declarations (most defined in bytecode.cpp) ***/

type_definition_t get_type_members(type_t typee, const defined_types_t& types);
bool is_small_int(type_t n, size_t bits);
type_t encode_vvm_type(const std::string& s);
type_t encode_type(const std::string& s);
type_t encode_type(size_t s, TypeMask mask);
std::string decode_type(type_t typee);
std::string decode_named_type(const named_type_t& nt);
std::string decode_types(const type_definition_t& td);
std::string disassemble(const defined_types_t& dt);
void verify_user_defined(type_t typee);

OpMask get_operand_mask(operand_t op);
size_t get_operand_value(operand_t op);
operand_t encode_operand(const std::string& s);
operand_t encode_operand(size_t s, OpMask mask);
std::string decode_operand(operand_t op);
void verify_is_type(operand_t typee);

tagged_ptr_t encode_ptr(int64_t* ptr);
tagged_ptr_t encode_ptr(double* ptr);
tagged_ptr_t encode_ptr(std::string* ptr);
tagged_ptr_t encode_ptr(FunctionDef* ptr);
void* remove_tag(tagged_ptr_t ptr);
std::string decode_ptr(tagged_ptr_t ptr);
std::string disassemble(const const_pool_t& cp);

size_t encode_opcode(const std::string& op);
std::string to_string(const Program& program);

// defined in disassembler.h
std::string disassemble(const instructions_t& code,
                        const std::string& padding);

// defined in interpret.cpp
std::string interpret(const Program& program, Mode mode);

// defined in assemble.cpp
Program assemble(const std::string& text, bool dump_vvm = false);

/*** labels ***/

// collects and resolves labels for branching
template<class Label=size_t>
class Labeler {
  struct LabelInfo {
    std::vector<size_t> dependents;
    size_t resolved = -1;
  };

  typedef std::unordered_map<Label, LabelInfo> LabelMap;
  LabelMap label_map_;

 public:

  // append dependency for a given label
  void append_dep(Label label, size_t loc) {
    label_map_[label].dependents.push_back(loc);
  }

  // set resolved location for a label
  void set_location(Label label, size_t loc) {
    label_map_[label].resolved = loc;
  }

   // update bytecode to resolve labels
   void resolve(instructions_t& code) {
     for (auto& kv: label_map_) {
       auto resolved = kv.second.resolved;
       if (resolved == size_t(-1)) {
         std::ostringstream oss;
         oss << "Unknown label " << kv.first;
         throw std::logic_error(oss.str());
       }
       for (auto& dep: kv.second.dependents) {
         code[dep] = encode_operand(resolved, OpMask::kImmediate);
       }
     }
   }

   // reset map
   void clear() {
     label_map_.clear();
   }
};
}  // namespace VVM

