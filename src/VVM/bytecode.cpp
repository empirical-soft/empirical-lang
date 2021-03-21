/*
 * Bytecode -- routines for handling VVM bytecode
 *
 * Copyright (C) 2019--2021 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#include <VVM/vvm.hpp>

namespace VVM {

// return the definition for a user-defined type
type_definition_t get_type_members(type_t typee,
                                   const defined_types_t& types) {
  type_t num = typee >> 1;
  auto iter = types.find(num);
  if (iter == types.end()) {
    std::ostringstream oss;
    oss << "Unknown user-defined type $" << num;
    throw std::logic_error(oss.str());
  }
  return iter->second;
}

// check whether number can be tagged for a bit length
bool is_small_int(type_t n, size_t bits) {
  return ((n << bits) >> bits) == n;
}

// convert a builtin type string to its numerical form
type_t encode_vvm_type(const std::string& s) {
  // cache the encoder map if it hasn't been created yet
  static std::unordered_map<std::string, size_t> encoder;
  if (encoder.empty()) {
    size_t count = 0;
    for (auto s: type_strings) {
      encoder[s] = count++;
    }
  }

  // find the type if possible
  auto iter = encoder.find(s);
  if (iter == encoder.end()) {
    std::string msg = "Unknown type: " + s;
    throw std::logic_error(msg);
  }
  return iter->second;
}

// convert a type string to its numerical form
type_t encode_type(const std::string& s) {
  // set mask and generate numerical version of string
  type_t result;
  TypeMask mask;
  if (s[0] == '$') {
    result = std::stol(&s[1]);
    mask = TypeMask::kUserDefined;
  } else {
    result = encode_vvm_type(s);
    mask = TypeMask::kBuiltIn;
  }
  // make sure the resulting number can handle tagging
  if (!is_small_int(result, 1)) {
    std::ostringstream oss;
    oss << "Type " <<  result << " is too large to be respresented with "
        << sizeof(type_t) << " bytes";
    throw std::logic_error(oss.str());
  }
  // tag the number
  return (result << 1) | type_t(mask);
}

// convert a stand-alone number to a proper type
type_t encode_type(size_t s, TypeMask mask) {
  type_t op = s;
  return (op << 1) | type_t(mask);
}

// string-ify a type
std::string decode_type(type_t typee) {
  TypeMask mask = TypeMask(typee & 1);
  type_t num = typee >> 1;
  switch (mask) {
    case TypeMask::kBuiltIn:
      return type_strings[num];
    case TypeMask::kUserDefined:
      return "$" + std::to_string(num);
  }
}

// string-ify a named type
std::string decode_named_type(const named_type_t& nt) {
  std::string result;
  if (!nt.name.empty()) {
    result += "\"" + nt.name + "\": ";
  }
  result += decode_type(nt.typee);
  return result;
}

// string-ify a type definition
std::string decode_types(const type_definition_t& td) {
  std::string results;
  if (!td.empty()) {
    results = decode_named_type(td[0]);
    for (size_t i = 1; i < td.size(); i++) {
      results += ", " + decode_named_type(td[i]);
    }
  }
  return results;
}

// disassemble user-defined types into directives
std::string disassemble(const defined_types_t& dt) {
  std::string results;
  for (auto& kv: dt) {
    results += decode_type(kv.first) + " = {";
    results += decode_types(kv.second);
    results += "}\n";
  }
  if (!results.empty()) {
    results += '\n';
  }
  return results;
}

// helper to ensure that a type is user defined
void verify_user_defined(type_t typee) {
  TypeMask mask = TypeMask(typee & 1);
  if (mask != TypeMask::kUserDefined) {
    std::string msg = "Was expecting user-defined type but got " +
      decode_type(typee);
    throw std::logic_error(msg);
  }
}

// extract the mask from an opereand
OpMask get_operand_mask(operand_t op) {
  return OpMask(op & 7);
}

// get the standalone number from an operand
size_t get_operand_value(operand_t op) {
  return op >> 3;
}

// convert an operand string into its numerical form
operand_t encode_operand(const std::string& s) {
  // set mask and convert string to number
  operand_t result;
  OpMask mask;
  if (std::isdigit(s[0])) {
    result = std::stol(s);
    mask = OpMask::kImmediate;
  } else if (s[0] == '%') {
    result = std::stol(&s[1]);
    mask = OpMask::kLocal;
  } else if (s[0] == '@') {
    result = std::stol(&s[1]);
    mask = OpMask::kGlobal;
  } else if (s[0] == '*') {
    result = std::stol(&s[1]);
    mask = OpMask::kState;
  } else {
    result = encode_type(s);
    mask = OpMask::kType;
  }
  // make sure the resulting number can handle tagging
  if (!is_small_int(result, 3)) {
    std::ostringstream oss;
    oss << "Operand " <<  result << " is too large to be respresented with "
        << sizeof(operand_t) << " bytes";
    throw std::logic_error(oss.str());
  }
  // tag the number
  return (result << 3) | operand_t(mask);
}

// convert a stand-alone number to a proper operand
operand_t encode_operand(size_t s, OpMask mask) {
  operand_t op = s;
  return (op << 3) | operand_t(mask);
}

// string-ify an operand
std::string decode_operand(operand_t op) {
  OpMask mask = get_operand_mask(op);
  size_t num = get_operand_value(op);
  switch (mask) {
    case OpMask::kImmediate:
      return std::to_string(num);
    case OpMask::kLocal:
      return "%" + std::to_string(num);
    case OpMask::kGlobal:
      return "@" + std::to_string(num);
    case OpMask::kState:
      return "*" + std::to_string(num);
    case OpMask::kType:
      return decode_type(type_t(num));
  }
}

// helper to ensure that an operand is actually a type
void verify_is_type(operand_t typee) {
  OpMask mask = get_operand_mask(typee);
  if (mask != OpMask::kType) {
    std::string msg = "Was expecting type but got " + decode_operand(typee);
    throw std::logic_error(msg);
  }
  return;
}

tagged_ptr_t encode_ptr(int64_t* ptr) {
  return (void*)(size_t(ptr) | size_t(PtrMask::kInt));
}

tagged_ptr_t encode_ptr(double* ptr) {
  return (void*)(size_t(ptr) | size_t(PtrMask::kFloat));
}

tagged_ptr_t encode_ptr(std::string* ptr) {
  return (void*)(size_t(ptr) | size_t(PtrMask::kStr));
}

tagged_ptr_t encode_ptr(FunctionDef* ptr) {
  return (void*)(size_t(ptr) | size_t(PtrMask::kFuncDef));
}

// remove a pointer's tag
void* remove_tag(tagged_ptr_t ptr) {
  return (void*)(size_t(ptr) & (~7));
}

// string-ify a tagged pointer
std::string decode_ptr(tagged_ptr_t ptr) {
  PtrMask mask = PtrMask(size_t(ptr) & 7);
  void* p = remove_tag(ptr);
  switch (mask) {
    case PtrMask::kInt:
      return std::to_string(*static_cast<int64_t*>(p));
    case PtrMask::kFloat:
      return std::to_string(*static_cast<double*>(p));
    case PtrMask::kStr:
      return "\"" + *static_cast<std::string*>(p) + "\"";
    case PtrMask::kFuncDef: {
      FunctionDef* fd = static_cast<FunctionDef*>(p);
      return "def " + fd->name + "(" + decode_types(fd->args) + ") " +
        decode_type(fd->rettype) + ":\n" + disassemble(fd->body, "  ") +
        "end\n";
    }
  }
}

// disassemble a constant pool into directives
std::string disassemble(const const_pool_t& cp) {
  std::string results;
  for (auto& kv: cp) {
    results += decode_operand(kv.first) + " = " + decode_ptr(kv.second)
            + '\n';
  }
  if (!results.empty()) {
    results += '\n';
  }
  return results;
}

// convert string version of opcode into numerical form
size_t encode_opcode(const std::string& op) {
  // map of known codes
  static std::unordered_map<std::string, size_t> encoder_;
  if (encoder_.empty()) {
    // construct map from string-ified opcode to underlying number
    size_t count = 0;
    for (auto s: opcode_strings) {
      encoder_[s] = count++;
    }
  }

  // look-up the code
  auto iter = encoder_.find(op);
  if (iter == encoder_.end()) {
    std::string msg = "Unknown opcode " + op;
    throw std::logic_error(msg);
  }
  return iter->second;
}

// disassemble a program
std::string to_string(const Program& program) {
  std::string result;
  result += disassemble(program.types);
  result += disassemble(program.constants);
  result += disassemble(program.instructions, "");
  return result;
}

}  // namespace VVM

#include <VVM/disassembler.h>

