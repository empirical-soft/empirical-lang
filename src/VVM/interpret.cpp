/*
 * Interpret -- execute VVM bytecode
 *
 * Copyright (C) 2019 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#include <cmath>
#include <vector>
#include <numeric>
#include <iostream>
#include <unordered_map>

#include <VVM/vvm.hpp>
#include <VVM/utils/timestamp.hpp>
#include <VVM/utils/conversion.hpp>
#include <VVM/utils/terminal.hpp>

#include <csvmonkey/csvmonkey.hpp>

#ifdef max
#undef max
#endif

namespace VVM {
/*
 * The interpreter executes instructions and maintains registers.
 *
 * The C++ functions that interface with the dispatch all end in a suffix that
 * indicates whether the input parameter is a scalar ("_s") or vector ("_v").
 * These may be combined for multiple inputs (eg. "_vs" for a vector parameter
 * followed by a scalar parameter). There is also an output parameter, but
 * this is not in the suffix (except for casts) because it does not alter which
 * function is used.
 *
 * The dispatch will specialize the C++ template for the appropriate
 * underlying type. For example, VVM's opcode:
 *
 *   add_i64v_i64v
 *
 * will invoke the C++ function with the included return type:
 *
 *   add_vv<int64_t, int64_t, int64_t>()
 *
 * Common logic is encapsulated in a macro. For example, BINOP will expand to
 * all permutations of a C++ function representing a binary operator.
 */
class Interpreter {
  // a single register value is a pointer to some object
  typedef void* Value;

  // user-defined types
  defined_types_t types_;

  // register banks
  typedef std::vector<Value> register_bank_t;
  register_bank_t global_registers_;
  register_bank_t local_registers_;

  // index into the register bank
  typedef operand_t index_t;

  // instruction pointer (aka program counter)
  size_t ip_;

  // the operand to return when inside a function call
  operand_t ret_op_;

  // get register from operand as a pointer to the location in the bank
  template<class T>
  T** get_register(operand_t op) {
    // get type of operand
    OpMask mask = OpMask(op & 3);
    if (mask == OpMask::kImmediate) {
      std::ostringstream oss;
      oss << "Was expecting a register, but got immediate value " << (op >> 2);
      throw std::logic_error(oss.str());
    }
    if (mask == OpMask::kType) {
      std::ostringstream oss;
      oss << "Was expecting a register, but got type " << decode_type(op >> 2);
      throw std::logic_error(oss.str());
    }

    // decode operand's info
    register_bank_t& bank = (mask == OpMask::kLocal) ? local_registers_
                                                     : global_registers_;
    index_t idx = op >> 2;

    // ensure that index is valid for particular register bank
    if (idx >= bank.size()) {
      bank.resize(idx + 1, nullptr);
    }

    // return the location in the register bank
    return reinterpret_cast<T**>(&bank[idx]);
  }

  // get a reference to a register's value
  template<class T>
  T& get_reference(operand_t op) {
    T*& ptr = *get_register<T>(op);
    if (ptr == nullptr) {
      ptr = new T;
    }
    return *ptr;
  }

  // get scalar value, either from register or from immediate
  template<class T>
  typename std::enable_if<std::is_integral<T>::value, T>::type
  get_value(operand_t op) {
    OpMask mask = OpMask(op & 3);
    if (mask == OpMask::kImmediate) {
      return op >> 2;
    }
    return get_reference<T>(op);
  }

  // get scalar value from register
  template<class T>
  typename std::enable_if<!std::is_integral<T>::value, T>::type
  get_value(operand_t op) {
    return get_reference<T>(op);
  }

  // a Dataframe is just an array of columns whose type is defined separately
  typedef std::vector<Value> Dataframe;

  // Empirical's eval() wants a string of the user's last expression
  std::string saved_string_;

  /*** MATH ***/

#define BINOP_SS(NAME, OP)  template<class T, class U, class V>\
  void NAME##_ss(operand_t left, operand_t right, operand_t result) {\
    T x = get_value<T>(left);\
    U y = get_value<U>(right);\
    V& z = get_reference<V>(result);\
    z = (is_int_nil(x) || is_int_nil(y)) ? nil_value<V>() : x OP y;\
  }\

#define BINOP_SV(NAME, OP)  template<class T, class U, class V>\
  void NAME##_sv(operand_t left, operand_t right, operand_t result) {\
    T x = get_value<T>(left);\
    std::vector<U>& ys = get_reference<std::vector<U>>(right);\
    std::vector<V>& zs = get_reference<std::vector<V>>(result);\
    zs.resize(ys.size());\
    for (size_t i = 0; i < ys.size(); i++) {\
      zs[i] = (is_int_nil(x) || is_int_nil(ys[i])) ? nil_value<V>() : x OP ys[i];\
    }\
  }\

#define BINOP_VS(NAME, OP)  template<class T, class U, class V>\
  void NAME##_vs(operand_t left, operand_t right, operand_t result) {\
    std::vector<T>& xs = get_reference<std::vector<T>>(left);\
    U y = get_value<U>(right);\
    std::vector<V>& zs = get_reference<std::vector<V>>(result);\
    zs.resize(xs.size());\
    for (size_t i = 0; i < xs.size(); i++) {\
      zs[i] = (is_int_nil(xs[i]) || is_int_nil(y)) ? nil_value<V>() : xs[i] OP y;\
    }\
  }\

#define BINOP_VV(NAME, OP)  template<class T, class U, class V>\
  void NAME##_vv(operand_t left, operand_t right, operand_t result) {\
    std::vector<T>& xs = get_reference<std::vector<T>>(left);\
    std::vector<U>& ys = get_reference<std::vector<U>>(right);\
    if (xs.size() != ys.size()) {\
      throw std::runtime_error("Mismatch array lengths");\
    }\
    std::vector<V>& zs = get_reference<std::vector<V>>(result);\
    zs.resize(xs.size());\
    for (size_t i = 0; i < xs.size(); i++) {\
      zs[i] = (is_int_nil(xs[i]) || is_int_nil(ys[i])) ? nil_value<V>() : xs[i] OP ys[i];\
    }\
  }\

#define BINOP(NAME, OP) BINOP_SS(NAME, OP) BINOP_SV(NAME, OP)\
                        BINOP_VS(NAME, OP) BINOP_VV(NAME, OP)

BINOP(add,+)
BINOP(sub,-)
BINOP(mul,*)
BINOP(div,/)
BINOP(mod,%)
BINOP(lt,<)
BINOP(gt,>)
BINOP(eq,==)
BINOP(ne,!=)
BINOP(lte,<=)
BINOP(gte,>=)
BINOP(and,&&)
BINOP(or,||)
BINOP(bitand,&)
BINOP(bitor,|)
BINOP(lshift,<<)
BINOP(rshift,>>)

#undef BINOP
#undef BINOP_VV
#undef BINOP_VS
#undef BINOP_SV
#undef BINOP_SS

template<class T, class U>
T bar(T x, U y) {
  return (x / y) * y;
}

#define BINFUNC_SS(NAME, F)  template<class T, class U, class V>\
  void NAME##_ss(operand_t left, operand_t right, operand_t result) {\
    T x = get_value<T>(left);\
    U y = get_value<U>(right);\
    V& z = get_reference<V>(result);\
    z = (is_int_nil(x) || is_int_nil(y)) ? nil_value<V>() : F(x, y);\
  }\

#define BINFUNC_SV(NAME, F)  template<class T, class U, class V>\
  void NAME##_sv(operand_t left, operand_t right, operand_t result) {\
    T x = get_value<T>(left);\
    std::vector<U>& ys = get_reference<std::vector<U>>(right);\
    std::vector<V>& zs = get_reference<std::vector<V>>(result);\
    zs.resize(ys.size());\
    for (size_t i = 0; i < ys.size(); i++) {\
      zs[i] = (is_int_nil(x) || is_int_nil(ys[i])) ? nil_value<V>() : F(x, ys[i]);\
    }\
  }\

#define BINFUNC_VS(NAME, F)  template<class T, class U, class V>\
  void NAME##_vs(operand_t left, operand_t right, operand_t result) {\
    std::vector<T>& xs = get_reference<std::vector<T>>(left);\
    U y = get_value<U>(right);\
    std::vector<V>& zs = get_reference<std::vector<V>>(result);\
    zs.resize(xs.size());\
    for (size_t i = 0; i < xs.size(); i++) {\
      zs[i] = (is_int_nil(xs[i]) || is_int_nil(y)) ? nil_value<V>() : F(xs[i], y);\
    }\
  }\

#define BINFUNC_VV(NAME, F)  template<class T, class U, class V>\
  void NAME##_vv(operand_t left, operand_t right, operand_t result) {\
    std::vector<T>& xs = get_reference<std::vector<T>>(left);\
    std::vector<U>& ys = get_reference<std::vector<U>>(right);\
    if (xs.size() != ys.size()) {\
      throw std::runtime_error("Mismatch array lengths");\
    }\
    std::vector<V>& zs = get_reference<std::vector<V>>(result);\
    zs.resize(xs.size());\
    for (size_t i = 0; i < xs.size(); i++) {\
      zs[i] = (is_int_nil(xs[i]) || is_int_nil(ys[i])) ? nil_value<V>() : F(xs[i], ys[i]);\
    }\
  }\

#define BINFUNC(NAME, F) BINFUNC_SS(NAME, F) BINFUNC_SV(NAME, F)\
                         BINFUNC_VS(NAME, F) BINFUNC_VV(NAME, F)

BINFUNC(bar,bar)

#undef BINFUNC
#undef BINFUNC_VV
#undef BINFUNC_VS
#undef BINFUNC_SV
#undef BINFUNC_SS

#define UNOP_S(NAME, OP)  template<class T, class U>\
  void NAME##_s(operand_t left, operand_t result) {\
    T x = get_value<T>(left);\
    U& y = get_reference<T>(result);\
    y = is_int_nil(x) ? nil_value<U>() : OP(x);\
  }\

#define UNOP_V(NAME, OP)  template<class T, class U>\
  void NAME##_v(operand_t left, operand_t result) {\
    std::vector<T>& xs = get_reference<std::vector<T>>(left);\
    std::vector<U>& ys = get_reference<std::vector<U>>(result);\
    ys.resize(xs.size());\
    for (size_t i = 0; i < xs.size(); i++) {\
      ys[i] = is_int_nil(xs[i]) ? nil_value<U>() : OP(xs[i]);\
    }\
  }\

#define UNOP(NAME, OP) UNOP_S(NAME, OP) UNOP_V(NAME, OP)

UNOP(neg,-)
UNOP(pos,+)
UNOP(not,!)
UNOP(sin,std::sin)
UNOP(cos,std::cos)
UNOP(tan,std::tan)
UNOP(asin,std::asin)
UNOP(acos,std::acos)
UNOP(atan,std::atan)
UNOP(sinh,std::sinh)
UNOP(cosh,std::cosh)
UNOP(tanh,std::tanh)
UNOP(asinh,std::asinh)
UNOP(acosh,std::acosh)
UNOP(atanh,std::atanh)

#undef UNOP
#undef UNOP_V
#undef UNOP_S

  // ordinarily just use the initial value...
  template<class T>
  typename std::enable_if<!std::is_same<T, std::string>::value, T>::type
  init_agg(size_t value) {
    return static_cast<T>(value);
  }

  // ...but constructor for std::string is undefined for nullptr input
  template<class T>
  typename std::enable_if<std::is_same<T, std::string>::value, T>::type
  init_agg(size_t value) {
    return std::string();
  }

#define REDUCE(NAME, OP, INIT)  template<class T, class U>\
  void NAME##_v(operand_t left, operand_t result) {\
    std::vector<T>& xs = get_reference<std::vector<T>>(left);\
    U& y = get_reference<U>(result);\
    y = init_agg<U>(INIT);\
    for (auto x: xs) {\
      if (!is_nil(x)) {\
        y = y OP x;\
      }\
    }\
  }

REDUCE(sum,+,0)
REDUCE(prod,*,1)

#undef REDUCE

  // now operation
  template<class T>
  void now_s(operand_t op) {
    T& value = get_reference<T>(op);
    value = T(now_nanos());
  }

  // unit operations (for suffixes)
#define UNIT(NAME, MULT) template<class T, class U>\
  void unit_##NAME##_s(operand_t value, operand_t result) {\
    T x = get_value<T>(value);\
    U& y = get_reference<U>(result);\
    y = U(x * MULT);\
  }

UNIT(ns,1)
UNIT(us,1000)
UNIT(ms,1000000)
UNIT(s,1000000000)
UNIT(m,60000000000)
UNIT(h,3600000000000)
UNIT(d,86400000000000)

#undef UNIT

  // iota
  template<class T>
  std::vector<T> internal_range(T n) {
    std::vector<T> xs(n);
    std::iota(xs.begin(), xs.end(), 0);
    return xs;
  }

  // total number of elements
  template<class T>
  int64_t internal_len(const std::vector<T>& xs) {
    return xs.size();
  }

  // number of non-nil elements
  template<class T>
  int64_t internal_count(const std::vector<T>& xs) {
    int64_t result = 0;
    for (size_t i = 0; i < xs.size(); i++) {
      if (!is_nil(xs[i])) {
        result++;
      }
    }
    return result;
  }

#define WRAPPER_S_V(FUNC) template<class T, class U>\
void FUNC##_s(operand_t left, operand_t result) {\
  T x = get_value<T>(left);\
  std::vector<T>& ys = get_reference<std::vector<T>>(result);\
  ys = internal_##FUNC(x);\
}

#define WRAPPER_V_S(FUNC) template<class T, class U>\
void FUNC##_v(operand_t left, operand_t result) {\
  std::vector<T>& xs = get_reference<std::vector<T>>(left);\
  U& y = get_reference<U>(result);\
  y = internal_##FUNC(xs);\
}

WRAPPER_S_V(range)
WRAPPER_V_S(len)
WRAPPER_V_S(count)

#undef WRAPPER_S_V
#undef WRAPPER_V_S

  /*** REPR ***/

  // scalar representation logic
  template<class T>
  std::string represent_s(operand_t src) {
    T x = get_value<T>(src);
    return to_repr(x);
  }

  // vector representation logic
  template<class T>
  std::string represent_v(operand_t src) {
    std::vector<T>& xs = get_reference<std::vector<T>>(src);
    std::string ys;
    const size_t max_items = 25;
    size_t length = std::min(xs.size(), max_items);
    ys = "[";
    if (!xs.empty()) {
      ys += to_repr(xs[0]);
    }
    for (size_t i = 1; i < length; i++) {
      ys += ", " + to_repr(xs[i]);
    }
    if (!xs.empty() && length < xs.size()) {
      ys += ", ...";
    }
    ys += "]";
    return ys;
  }

  // scalar stringify logic
  template<class T>
  std::string stringify_s(Value v) {
    T& x = *static_cast<T*>(v);
    return to_string(x);
  }

  // vector stringify logic
  template<class T>
  std::vector<std::string> stringify_v(Value v, std::string& name,
                                       size_t max_items) {
    std::vector<T>& xs = *static_cast<std::vector<T>*>(v);
    size_t length = std::min(xs.size(), max_items);
    std::vector<std::string> ys(length);
    for (size_t i = 0; i < length; i++) {
      ys[i] = to_string(xs[i]);
    }
    trim_trailing_zeros<T>(ys);
    ys.insert(ys.begin(), name);
    return ys;
  }

  // wrap scalar string into a vector
  template<class T>
  std::vector<std::string> stringify_wrap(Value v, std::string& name,
                                          size_t max_items) {
    std::vector<std::string> ys(1, stringify_s<T>(v));
    trim_trailing_zeros<T>(ys);
    ys.insert(ys.begin(), name);
    return ys;
  }

#include <VVM/repr.h>

  // pad a string with spaces to fit the desired length
  std::string pad(const std::string& s, size_t length,
                  bool right_justify = true) {
    if (s.size() >= length) {
      return s;
    }

    const size_t extra = length - s.size();
    std::string padding;
    for (size_t i = 0; i < extra; i++) {
      padding += ' ';
    }

    return right_justify ? (padding + s) : (s + padding);
  }

  // string representation of data
  std::string represent(operand_t src, type_t typee) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        return represent_builtin(static_cast<vvm_types>(num), src);
      }
      case TypeMask::kUserDefined: {
        auto members = get_type_members(typee, types_);
        Dataframe& cols = get_reference<Dataframe>(src);

        // get max dimensions if we have to truncate
        size_t max_console_rows, max_console_cols;
        get_terminal_size(max_console_rows, max_console_cols);

        // leave space for the header, clearance, and top & bottom prompt
        const size_t max_df_rows = max_console_rows - 4;

        // determine the number of rows we're allowed to display
        const size_t total_df_rows = len_df(cols, members);
        const size_t permitted_df_rows = std::min(max_df_rows, total_df_rows);

        // avoid dotting if table is exactly the max length
        const size_t dotted_row =
          max_df_rows + (max_df_rows == total_df_rows ? 1 : 0);

        // convert each column to padded strings and tranpose to rows
        std::vector<std::string> rows(permitted_df_rows + 1);
        for (size_t col = 0; col < cols.size(); col++) {
          // get string of each member
          vvm_types vvm_typee =
            static_cast<vvm_types>(members[col].typee >> 1);
          auto results = stringify(vvm_typee, cols[col], members[col].name,
                                   permitted_df_rows);

          // get size of largest string member
          size_t max_length = 0;
          for (auto& r: results) {
            max_length = std::max(max_length, r.size());
          }
          max_length++;

          // append padded version of string members to corresponding row
          for (size_t row = 0; row < rows.size(); row++) {
            std::string input_str = results[row];
            if (row == dotted_row) {
              input_str = (max_length > 3) ? "..." : "..";
            }
            rows[row] += pad(input_str, max_length);
          }

          // correct if we've exceeded the max width
          if (rows[0].size() > max_console_cols) {
            size_t permitted_cols = max_console_cols - 3;
            for (auto& r: rows) {
              r = r.substr(0, permitted_cols) + "...";
            }
            break;
          }
        }

        // join rows by a carriage return
        std::string result = rows[0];
        for (size_t row = 1; row < rows.size(); row++) {
          result += '\n' + rows[row];
        }
        return result;
      }
    }
  }

  // repr operation
  void repr(operand_t src, operand_t typee, operand_t dst) {
    verify_is_type(typee);
    std::string& y = get_reference<std::string>(dst);
    y = represent(src, typee >> 2);
  }

  /*** LOAD ***/

  // parse array of text into a given type
  template<class T>
  void parse_array(const std::vector<std::string>& text, Value arr) {
    std::vector<T>& ys = *reinterpret_cast<std::vector<T>*>(arr);
    ys.resize(text.size());
    for (size_t i = 0; i < text.size(); i++) {
      ys[i] = from_string<T>(text[i]);
    }
  }

#include <VVM/parse.h>

  // load and parse file contents
  Dataframe loader(operand_t src, type_t typee, size_t max_rows = -1) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        std::ostringstream oss;
        oss << "Cannot load a file into builtin type $" << num;
        throw std::logic_error(oss.str());
      }
      case TypeMask::kUserDefined: {
        auto members = get_type_members(typee, types_);
        Dataframe& df = *reinterpret_cast<Dataframe*>(allocate(typee));

        // prepare reader
        std::string filename = get_value<std::string>(src);
        csvmonkey::MappedFileCursor cursor;
        try {
          cursor.open(filename.c_str());
        }
        catch (csvmonkey::Error& e) {
          throw std::logic_error(e.what());
        }
        csvmonkey::CsvReader reader(cursor);

        // read and transpose table
        std::vector<std::vector<std::string>> columns(df.size());
        bool is_header = true;
        size_t nrows = 0;
        while (reader.read_row() && (nrows++ < max_rows)) {
          if (!is_header) {
            auto& row = reader.row();
            for (size_t col = 0; col < columns.size(); col++) {
              columns[col].push_back(
                col < row.count ? row.cells[col].as_str() : "");
            }
          }
          is_header = false;
        }

        // parse each column
        for (size_t col = 0; col < columns.size(); col++) {
          vvm_types vvm_typee =
            static_cast<vvm_types>(members[col].typee >> 1);
          parse_array(vvm_typee, columns[col], df[col]);
        }

        return df;
      }
    }
  }

  // load operation
  void load(operand_t src, operand_t typee, operand_t dst) {
    verify_is_type(typee);
    Dataframe& y = get_reference<Dataframe>(dst);
    y = loader(src, typee >> 2);
  }

  /*** STORE ***/

  // store data to a file
  void storer(type_t typee, operand_t src, std::string filename) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        std::ostringstream oss;
        oss << "Cannot store to a file from builtin type $" << num;
        throw std::logic_error(oss.str());
      }
      case TypeMask::kUserDefined: {
        auto members = get_type_members(typee, types_);
        Dataframe& cols = get_reference<Dataframe>(src);
        const size_t total_df_rows = len_df(cols, members);

        // convert each column to strings and tranpose to rows
        std::vector<std::string> rows(total_df_rows + 1);
        for (size_t col = 0; col < cols.size(); col++) {
          // get string of each member
          // TODO will need to escape the separator
          vvm_types vvm_typee =
            static_cast<vvm_types>(members[col].typee >> 1);
          auto results = stringify(vvm_typee, cols[col], members[col].name,
                                   total_df_rows);

          // append string members to corresponding row
          for (size_t row = 0; row < rows.size(); row++) {
            if (col > 0) {
              rows[row] += ',';
            }
            rows[row] += results[row];
          }
        }

        // write rows to file
        std::ofstream out(filename);
        for (auto& row: rows) {
          out << row << '\n';
        }
        out.close();
      }
    }
  }

  // store operation (always returns zero)
  void store(operand_t typee, operand_t src, operand_t fn, operand_t res) {
    verify_is_type(typee);
    std::string filename = get_value<std::string>(fn);
    int64_t& z = get_reference<int64_t>(res);
    z = 0;
    storer(typee >> 2, src, filename);
  }

  /*** ASSIGN ***/

  // scalar assign (value) logic
  template<class T>
  void assign_value_s(Value src, Value dst) {
    T& x = *static_cast<T*>(src);
    T& y = *static_cast<T*>(dst);
    y = x;
  }

  // vector assign (value) logic
  template<class T>
  void assign_value_v(Value src, Value dst) {
    std::vector<T>& xs = *static_cast<std::vector<T>*>(src);
    std::vector<T>& ys = *static_cast<std::vector<T>*>(dst);
    ys = xs;
  }

  // scalar assign (builtin) logic
  template<class T>
  void assign_builtin_s(operand_t src, operand_t dst) {
    T x = get_value<T>(src);
    T& y = get_reference<T>(dst);
    y = x;
  }

  // vector assign (builtin) logic
  template<class T>
  void assign_builtin_v(operand_t src, operand_t dst) {
    std::vector<T>& xs = get_reference<std::vector<T>>(src);
    std::vector<T>& ys = get_reference<std::vector<T>>(dst);
    ys = xs;
  }

#include <VVM/assign.h>

  // assign item
  void assigner(operand_t src, type_t typee, operand_t dst) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        return assign_builtin(static_cast<vvm_types>(num), src, dst);
      }
      case TypeMask::kUserDefined: {
        auto members = get_type_members(typee, types_);
        Dataframe& src_cols = get_reference<Dataframe>(src);
        Dataframe& dst_cols = get_reference<Dataframe>(dst);

        // assign each column
        for (size_t col = 0; col < src_cols.size(); col++) {
          vvm_types vvm_typee =
            static_cast<vvm_types>(members[col].typee >> 1);
          assign_value(vvm_typee, src_cols[col], dst_cols[col]);
        }
      }
    }
  }

  // assign operation
  void assign(operand_t src, operand_t typee, operand_t dst) {
    verify_is_type(typee);
    assigner(src, typee >> 2, dst);
  }

  /*** APPEND ***/

  // append logic
  template<class T>
  void append_s(operand_t left, operand_t right) {
    T x = get_value<T>(left);
    std::vector<T>& ys = get_reference<std::vector<T>>(right);
    ys.push_back(x);
  }

#include <VVM/append.h>

  // append item
  void appender(operand_t src, type_t typee, operand_t dst) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        return append_builtin(static_cast<vvm_types>(num), src, dst);
      }
      case TypeMask::kUserDefined: {
        // TODO probably want to convert List(UDT) to be a Dataframe
        throw std::logic_error("Cannot build a list from user-defined types");
      }
    }
  }

  // append operation
  void append(operand_t src, operand_t typee, operand_t dst) {
    verify_is_type(typee);
    appender(src, typee >> 2, dst);
  }

  /*** CAST ***/

  // scalar cast
  template<class T, class U>
  void cast_s(operand_t src, operand_t dst) {
    T x = get_value<T>(src);
    U& y = get_reference<U>(dst);
    y = super_cast<T, U>(x);
  }

  // vector cast
  template<class T, class U>
  void cast_v(operand_t src, operand_t dst) {
    std::vector<T>& xs = get_reference<std::vector<T>>(src);
    std::vector<U>& ys = get_reference<std::vector<U>>(dst);
    ys.resize(xs.size());
    for (size_t i = 0; i < xs.size(); i++) {
      ys[i] = super_cast<T, U>(xs[i]);
    }
  }

  /*** WHERE ***/

  // narrow vector according to where the elements are true
  template<class T, class U>
  typename std::enable_if<std::is_same<U, bool>::value, void>::type
  where_elem(Value src, const std::vector<U>& tr, Value dst) {
    std::vector<T>& xs = *reinterpret_cast<std::vector<T>*>(src);
    std::vector<T>& ys = *reinterpret_cast<std::vector<T>*>(dst);
    if (xs.size() != tr.size()) {
      throw std::runtime_error("Mismatch array lengths");
    }
    size_t count = 0;
    for (size_t i = 0; i < xs.size(); i++) {
      if (tr[i]) {
        count++;
      }
    }
    ys.resize(count);
    count = 0;
    for (size_t i = 0; i < xs.size(); i++) {
      if (tr[i]) {
        ys[count++] = xs[i];
      }
    }
  }

  // narrow vector according to multiple indices
  template<class T, class U>
  typename std::enable_if<!std::is_same<U, bool>::value, void>::type
  where_elem(Value src, const std::vector<U>& idxs, Value dst) {
    std::vector<T>& xs = *reinterpret_cast<std::vector<T>*>(src);
    std::vector<T>& ys = *reinterpret_cast<std::vector<T>*>(dst);
    ys.resize(idxs.size());
    for (size_t i = 0; i < idxs.size(); i++) {
      ys[i] = idxs[i] == -1 ? nil_value<T>() : xs[idxs[i]];
    }
  }

#include <VVM/where.h>

  // narrow Dataframe according to either indices or where the rows are true
  template<class T>
  Dataframe where_rows(operand_t src, const std::vector<T>& values,
                       type_t typee) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        std::ostringstream oss;
        oss << "Cannot narrow a Dataframe of builtin type $" << num;
        throw std::logic_error(oss.str());
      }
      case TypeMask::kUserDefined: {
        auto members = get_type_members(typee, types_);

        // for each column, copy only the desired elements
        Dataframe& table = get_reference<Dataframe>(src);
        Dataframe& columns = *reinterpret_cast<Dataframe*>(allocate(typee));

        for (size_t col = 0; col < columns.size(); col++) {
          vvm_types vvm_typee =
            static_cast<vvm_types>(members[col].typee >> 1);
          where_elem(vvm_typee, table[col], values, columns[col]);
        }

        return columns;
      }
    }
  }

  // where operation
  void where(operand_t src, operand_t truths, operand_t typee, operand_t dst) {
    verify_is_type(typee);
    Dataframe& y = get_reference<Dataframe>(dst);
    std::vector<bool>& tr = get_reference<std::vector<bool>>(truths);
    y = where_rows(src, tr, typee >> 2);
  }

  // multidx operation
  void multidx(operand_t src, operand_t indices, operand_t typee,
               operand_t dst) {
    verify_is_type(typee);
    Dataframe& y = get_reference<Dataframe>(dst);
    std::vector<int64_t>& idxs = get_reference<std::vector<int64_t>>(indices);
    y = where_rows(src, idxs, typee >> 2);
  }

  /*** MISC ***/

  // write operation
  void write(operand_t op) {
    std::string& x = get_reference<std::string>(op);
    std::cout << x << std::endl;
  }

  // print operation (always returns zero)
  template<class T, class U>
  void print_s(operand_t op1, operand_t op2) {
    T x = get_value<T>(op1);
    std::string s = super_cast<T, std::string>(x);
    std::cout << s << std::endl;
    U& y = get_reference<U>(op2);
    y = U(0);
  }

  template<class T, class U>
  void print_v(operand_t op1, operand_t op2) {
    std::vector<T>& xs = get_reference<std::vector<T>>(op1);
    std::string s = "[";
    if (!xs.empty()) {
      s += to_repr(xs[0]);
    }
    for (size_t i = 1; i < xs.size(); i++) {
      s += ", " + to_repr(xs[i]);
    }
    s += "]";
    std::cout << s << std::endl;
    U& y = get_reference<U>(op2);
    y = U(0);
  }

  // save operation
  void save(operand_t op) {
    std::string& x = get_reference<std::string>(op);
    saved_string_ = x;
  }

  // idx operation -- specialized for bools (read-only)
  template<class T, class U, class V>
  typename std::enable_if<std::is_same<T, bool>::value, void>::type
  idx_vs(operand_t value, operand_t index, operand_t result) {
    std::vector<T>& xs = get_reference<std::vector<T>>(value);
    U y = get_value<U>(index);
    if (y >= xs.size()) {
       throw std::runtime_error("Index out of bounds");
    }
    V& z = get_reference<V>(result);
    z = xs[y];
  }

  // idx operation
  template<class T, class U, class V>
  typename std::enable_if<!std::is_same<T, bool>::value, void>::type
  idx_vs(operand_t value, operand_t index, operand_t result) {
    std::vector<T>& xs = get_reference<std::vector<T>>(value);
    U y = get_value<U>(index);
    if (y >= xs.size()) {
       throw std::runtime_error("Index out of bounds");
    }
    V*& ptr = *get_register<V>(result);
    // C++'s std::vector<bool> prohibits pointers to elements
    ptr = &xs[y];
  }

  // scalar del operation
  template<class T>
  void del_s(operand_t tgt) {
    T*& ptr = *get_register<T>(tgt);
    delete ptr;
    ptr = nullptr;
  }

  // vector del operation
  template<class T>
  void del_v(operand_t tgt) {
    std::vector<T>*& ptr = *get_register<std::vector<T>>(tgt);
    delete ptr;
    ptr = nullptr;
  }

#include <VVM/allocate.h>

  // allocate according to a type
  Value allocate(type_t typee) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        return allocate_builtin(static_cast<vvm_types>(num));
      }
      case TypeMask::kUserDefined: {
        auto members = get_type_members(typee, types_);

        // iteratively allocate each member type via recursion
        Dataframe* fields = new Dataframe(members.size());
        for (size_t i = 0; i < members.size(); i++) {
          (*fields)[i] = allocate(members[i].typee);
        }
        return reinterpret_cast<void*>(fields);
      }
    }
  }

  // alloc operation
  void alloc(operand_t typee, operand_t dst) {
    verify_is_type(typee);
    Value& reg = *get_register<void>(dst);
    reg = allocate(typee >> 2);
  }

  // member operation
  void member(operand_t value, operand_t index, operand_t result) {
    Dataframe& xs = get_reference<Dataframe>(value);
    int64_t y = get_value<int64_t>(index);
    if (y >= xs.size()) {
       throw std::runtime_error("Member index out of bounds");
    }
    Value& ptr = *get_register<void>(result);
    ptr = xs[y];
  }

  // branch operation
  void br(operand_t dst) {
    size_t loc = get_value<size_t>(dst);
    ip_ = loc;
  }

  // branch-true operation
  void btrue(operand_t value, operand_t dst) {
    bool truth = get_value<bool>(value);
    size_t loc = get_value<size_t>(dst);
    if (truth) {
      ip_ = loc;
    }
  }

  // branch-false operation
  void bfalse(operand_t value, operand_t dst) {
    bool truth = get_value<bool>(value);
    size_t loc = get_value<size_t>(dst);
    if (!truth) {
      ip_ = loc;
    }
  }

  /*** FUNCTIONS ***/

  // ret operation
  void ret(operand_t value, const instructions_t& bytecode) {
    // save the value and jump to halt instruction
    ret_op_ = value;
    ip_ = bytecode.size() - 1;
  }

#include <VVM/wrap_immediate.h>

  // guarantee a pointer from the operand; wrap immediate values
  void* get_ptr(type_t typee, operand_t op) {
    TypeMask type_mask = TypeMask(typee & 1);
    OpMask op_mask = OpMask(op & 3);
    if (type_mask == TypeMask::kUserDefined || op_mask != OpMask::kImmediate) {
      return *get_register<void>(op);
    }
    return wrap_immediate(vvm_types(typee >> 1), op);
  }

  // call operation
  void call(operand_t func, operand_t num_params, const instructions_t& bytecode) {
    FunctionDef& fd = get_reference<FunctionDef>(func);
    int64_t np = get_value<int64_t>(num_params) - 1;

    // ensure that argument count is what we expect
    if (np < 0) {
      std::string msg = "Function call to " + fd.name +
                        " requires location of return value";
      throw std::runtime_error(msg);
    }
    if (np != fd.args.size()) {
      std::ostringstream oss;
      oss << "Calling " << fd.name << " with wrong number of arguments: "
          << np << " vs " << fd.args.size()
          << " (must include location of return value)";
      throw std::runtime_error(oss.str());
    }

    // copy pointers from operands to a new register bank
    auto bc = &bytecode[ip_];
    register_bank_t new_registers(np);
    for (size_t i = 0; i < new_registers.size(); i++) {
      operand_t op = bc[i];
      new_registers[i] = get_ptr(fd.args[i].typee, op);
    }

    // move IP to end of operands
    ip_ += (np + 1);

    // save frame information
    register_bank_t saved_registers = std::move(local_registers_);
    size_t saved_ip = ip_;

    // dispatch the new bytecode; this is mutually recursive
    local_registers_ = std::move(new_registers);
    dispatch(fd.body);

    // having returned from the user's function call, save result
    Value ret_value = get_ptr(fd.rettype, ret_op_);

    // restore frame information
    local_registers_ = std::move(saved_registers);
    ip_ = saved_ip;

    // save the returned result now that we have our registers back
    Value& result = *get_register<void>(bc[np]);
    result = ret_value;
  }

  /*** LEN ***/

  template<class T>
  int64_t len(Value src) {
    std::vector<T>& xs = *reinterpret_cast<std::vector<T>*>(src);
    return xs.size();
  }

#include <VVM/len.h>

  int64_t len_df(const Dataframe& table, const type_definition_t& members,
                 size_t which_member = 0) {
    vvm_types first_typee =
      static_cast<vvm_types>(members[which_member].typee >> 1);
    return len(first_typee, table[0]);
  }

  /*** SORT ***/

  // sort array by index
  template<class T>
  void isort_elem(Value src, std::vector<int64_t>& indices) {
    std::vector<T>& xs = *reinterpret_cast<std::vector<T>*>(src);
    std::stable_sort(std::begin(indices), std::end(indices),
                     [&](size_t a, size_t b) {return xs[a] < xs[b];});
  }

#include <VVM/isort.h>

  std::vector<int64_t> isort_cols(operand_t src, type_t typee) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        std::ostringstream oss;
        oss << "Cannot sort a builtin type $" << num;
        throw std::logic_error(oss.str());
      }
      case TypeMask::kUserDefined: {
        auto members = get_type_members(typee, types_);

        // pre-arrange the indices as 0..n-1
        Dataframe& table = get_reference<Dataframe>(src);
        int64_t n = len_df(table, members);
        std::vector<int64_t> indices(n);
        std::iota(std::begin(indices), std::end(indices), 0);

        // for each column, determine order of indices; must go in reverse
        for (int64_t col = table.size() - 1; col >= 0; col--) {
          vvm_types vvm_typee =
            static_cast<vvm_types>(members[col].typee >> 1);
          isort_elem(vvm_typee, table[col], indices);
        }

        return indices;
      }
    }
  }

  // isort operation
  void isort(operand_t src, operand_t typee, operand_t dst) {
    verify_is_type(typee);
    std::vector<int64_t>& y = get_reference<std::vector<int64_t>>(dst);
    y = isort_cols(src, typee >> 2);
  }

  /*** CATEGORIZE ***/

  // These functions enumerate the unique tuple values of a Dataframe. They
  // are not exposed as opcodes in VVM; instead, they are used by the group
  // and join operations.

  template<class T>
  int64_t categorize(const std::vector<T>& keys, std::vector<int64_t>& labs,
                     int64_t stride) {
    std::unordered_map<T, int64_t> m(keys.size());
    int64_t count = 0;

    // get labels
    for (size_t i = 0; i < keys.size(); i++) {
      int64_t v;
      T k = keys[i];
      auto iter = m.find(k);
      if (iter == m.end()) {
        v = m[k] = count++;
      }
      else {
        v = iter->second;
      }
      labs[i] += v * stride;
    }

    if (stride != 1) {
      // re-categorize to prevent labels from overflowing
      std::vector<int64_t> new_labs(labs.size(), 0);
      count = categorize<int64_t>(labs, new_labs, 1);
      labs = std::move(new_labs);
    }

    return count;
  }

  template<class T>
  int64_t categorize2(const std::vector<T>& lkeys, const std::vector<T>& rkeys,
                      std::vector<int64_t>& llabs, std::vector<int64_t>& rlabs,
                      int64_t stride) {
    std::unordered_map<T, int64_t> m(lkeys.size() + rkeys.size());
    int64_t count = 0;

    // get left labels
    for (size_t i = 0; i < lkeys.size(); i++) {
      int64_t v;
      T k = lkeys[i];
      auto iter = m.find(k);
      if (iter == m.end()) {
        v = m[k] = count++;
      }
      else {
        v = iter->second;
      }
      llabs[i] += v * stride;
    }

    // also get right labels
    for (size_t i = 0; i < rkeys.size(); i++) {
      int64_t v;
      T k = rkeys[i];
      auto iter = m.find(k);
      if (iter == m.end()) {
        v = m[k] = count++;
      }
      else {
        v = iter->second;
      }
      rlabs[i] += v * stride;
    }

    if (stride != 1) {
      // re-categorize to prevent labels from overflowing
      std::vector<int64_t> new_llabs(llabs.size(), 0);
      std::vector<int64_t> new_rlabs(rlabs.size(), 0);
      count = categorize2<int64_t>(llabs, rlabs, new_llabs, new_rlabs, 1);
      llabs = std::move(new_llabs);
      rlabs = std::move(new_rlabs);
    }

    return count;
  }

  template<class T>
  int64_t categorize(Value keys, std::vector<int64_t>& labs, int64_t stride) {
    const std::vector<T>& xs = *reinterpret_cast<std::vector<T>*>(keys);
    return categorize(xs, labs, stride);
  }

  template<class T>
  int64_t categorize2(Value lkeys, Value rkeys,
                    std::vector<int64_t>& llabs, std::vector<int64_t>& rlabs,
                    int64_t stride) {
    const std::vector<T>& xs = *reinterpret_cast<std::vector<T>*>(lkeys);
    const std::vector<T>& ys = *reinterpret_cast<std::vector<T>*>(rkeys);
    return categorize2(xs, ys, llabs, rlabs, stride);
  }

#include <VVM/categorize.h>

  // categorize a Dataframe; return number of unique labels
  int64_t categorize_df(type_t typee, operand_t df,
                        std::vector<int64_t>& labs) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        std::ostringstream oss;
        oss << "Cannot categorize a builtin type $" << num;
        throw std::logic_error(oss.str());
      }
      case TypeMask::kUserDefined: {
        auto members = get_type_members(typee, types_);

        // preset labels as all zeros
        Dataframe& table = get_reference<Dataframe>(df);
        int64_t length = len_df(table, members);
        labs.resize(length, 0);
        int64_t stride = 1;

        // for each column, collect labels offset by stride
        for (int64_t col = 0; col < table.size(); col++) {
          vvm_types vvm_typee =
            static_cast<vvm_types>(members[col].typee >> 1);
          stride = categorize(vvm_typee, table[col], labs, stride);
        }
        return stride;
      }
    }
  }

  // categorize two Dataframes; return number of unique labels
  int64_t categorize_df2(type_t typee, operand_t left_df, operand_t right_df,
                         std::vector<int64_t>& llabs,
                         std::vector<int64_t>& rlabs) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        std::ostringstream oss;
        oss << "Cannot categorize a builtin type $" << num;
        throw std::logic_error(oss.str());
      }
      case TypeMask::kUserDefined: {
        auto members = get_type_members(typee, types_);

        // preset labels as all zeros
        Dataframe& left_table = get_reference<Dataframe>(left_df);
        Dataframe& right_table = get_reference<Dataframe>(right_df);
        int64_t left_length = len_df(left_table, members);
        int64_t right_length = len_df(right_table, members);
        llabs.resize(left_length, 0);
        rlabs.resize(right_length, 0);
        int64_t stride = 1;

        // for each column, collect labels offset by stride
        for (int64_t col = 0; col < left_table.size(); col++) {
          vvm_types vvm_typee =
            static_cast<vvm_types>(members[col].typee >> 1);
          stride = categorize2(vvm_typee, left_table[col], right_table[col],
                               llabs, rlabs, stride);
        }
        return stride;
      }
    }
  }

  /*** GROUP ***/

  // split a column from one Dataframe across many
  template<class T>
  void split_col(size_t col, const std::vector<std::vector<int64_t>>& igroup,
                 const Dataframe& df, std::vector<Dataframe*>& tgt_dfs) {
    std::vector<T>& df_col = *reinterpret_cast<std::vector<T>*>(df[col]);

    // allocate a column and copy elements for each target DF
    for (size_t row_group = 0; row_group < igroup.size(); row_group++) {
      const std::vector<int64_t>& row_indices = igroup[row_group];
      auto new_col = new std::vector<T>(row_indices.size());
      for (size_t i = 0; i < row_indices.size(); i++) {
        (*new_col)[i] = df_col[row_indices[i]];
      }
      (*tgt_dfs[row_group])[col] = reinterpret_cast<void*>(new_col);
    }
  }

#include <VVM/split.h>

  // group a Dataframe according to keys
  void group_df(type_t df_type, operand_t df,
                type_t key_type, operand_t keys,
                type_t ret_type, Dataframe& init_df,
                std::vector<Dataframe*>& df_vec, int64_t& length) {
    // check tag
    TypeMask mask = TypeMask(key_type & 1);
    type_t num = key_type >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        std::ostringstream oss;
        oss << "Cannot group a builtin type $" << num;
        throw std::logic_error(oss.str());
      }
      case TypeMask::kUserDefined: {
        auto members = get_type_members(df_type, types_);

        // get labels from keys
        std::vector<int64_t> labs;
        length = categorize_df(key_type, keys, labs);

        // group the label indices
        // (std::vector::push_back() is really slow, hence indirect logic)
        std::vector<std::vector<int64_t>> igroup(length);
        std::vector<int64_t> ig_count(length, 0);
        for (int64_t i = 0; i < labs.size(); i++) {
          ig_count[labs[i]]++;
        }
        for (int64_t i = 0; i < length; i++) {
          igroup[i].resize(ig_count[i]);
          ig_count[i] = 0;
        }
        for (int64_t i = 0; i < labs.size(); i++) {
          int64_t j = labs[i];
          igroup[j][ig_count[j]++] = i;
        }

        // preallocate the target Dataframes
        const Dataframe& table = get_reference<Dataframe>(df);
        df_vec.resize(length);
        for (size_t i = 0; i < length; i++) {
          df_vec[i] = new Dataframe(table.size());
        }

        // split each column across target Dataframes
        for (size_t col = 0; col < table.size(); col++) {
          vvm_types vvm_typee =
            static_cast<vvm_types>(members[col].typee >> 1);
          split_col(vvm_typee, col, igroup, table, df_vec);
        }

        // determine initial output Dataframe with columns from keys
        init_df = std::move(
          *reinterpret_cast<Dataframe*>(allocate(ret_type)));
        std::vector<int64_t> first_rows(length);
        for (size_t i = 0; i < length; i++) {
          first_rows[i] = igroup[i][0];
        }
        Dataframe key_rows = where_rows(keys, first_rows, key_type);
        for (size_t i = 0; i < key_rows.size(); i++) {
          init_df[i] = key_rows[i];
        }
      }
    }
  }

  // group operation
  void group(operand_t df_type, operand_t df,
             operand_t key_type, operand_t keys,
             operand_t ret_type, operand_t init_df,
             operand_t df_vec, operand_t length) {
    verify_is_type(df_type);
    verify_is_type(key_type);
    verify_is_type(ret_type);

    Dataframe& x = get_reference<Dataframe>(init_df);
    std::vector<Dataframe*>& y =
      get_reference<std::vector<Dataframe*>>(df_vec);
    int64_t& z = get_reference<int64_t>(length);

    group_df(df_type >> 2, df, key_type >> 2, keys, ret_type >> 2, x, y, z);
  }

  /*** JOIN ***/

  // match two Dataframes by equal keys
  void eqmatch_df(type_t typee, operand_t left_df, operand_t right_df,
                  std::vector<int64_t>& left_indices,
                  std::vector<int64_t>& right_indices) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        std::ostringstream oss;
        oss << "Cannot eqmatch a builtin type $" << num;
        throw std::logic_error(oss.str());
      }
      case TypeMask::kUserDefined: {
        // get labels
        std::vector<int64_t> llabs;
        std::vector<int64_t> rlabs;
        categorize_df2(typee, left_df, right_df, llabs, rlabs);

        // set right labels as map
        std::unordered_map<int64_t, int64_t> m(rlabs.size());
        for (int64_t i = 0; i < rlabs.size(); i++) {
          int64_t k = rlabs[i];
          auto iter = m.find(k);
          if (iter != m.end()) {
            std::ostringstream oss;
            oss << "Duplicate keys in right table at index "
                << iter->second << " and " << i;
            throw std::runtime_error(oss.str());
          }
          m[k] = i;
        }

        // look-up left labels in map
        left_indices.resize(llabs.size());
        right_indices.resize(llabs.size());
        for (int64_t i = 0; i < llabs.size(); i++) {
          auto iter = m.find(llabs[i]);
          left_indices[i] = i;
          right_indices[i] = (iter == m.end()) ? -1 : iter->second;
        }
      }
    }
  }

  // eqmatch operation
  void eqmatch(operand_t typee, operand_t left_df, operand_t right_df,
               operand_t left_indices, operand_t right_indices) {
    verify_is_type(typee);

    std::vector<int64_t>& left =
      get_reference<std::vector<int64_t>>(left_indices);
    std::vector<int64_t>& right =
      get_reference<std::vector<int64_t>>(right_indices);

    eqmatch_df(typee >> 2, left_df, right_df, left, right);
  }

  // The asof functions below are separated for types that have subtraction
  // defined versus those that don't. We can order std::string, for example,
  // but we can't compute a distance. Therefore, functions that match the
  // nearest or within a tolerance must be distinct from the regular match.

  // match two arrays asof ordering (not nearest)
  template <class T>
  void asofmatch_arr(operand_t left, operand_t right, bool strict,
                     AsofDirection direction,
                     std::vector<int64_t>& left_indices,
                     std::vector<int64_t>& right_indices) {
    std::vector<T>& left_values = get_reference<std::vector<T>>(left);
    std::vector<T>& right_values = get_reference<std::vector<T>>(right);

    left_indices.resize(left_values.size());
    std::iota(std::begin(left_indices), std::end(left_indices), 0);
    right_indices.resize(left_values.size(), -1);

    if (direction == AsofDirection::kBackward) {
      int64_t right_pos = 0;
      for (int64_t left_pos = 0; left_pos < left_values.size();
           left_pos++) {
        // find last position in right whose value is less than left's
        if (!strict) {
          while (right_pos < right_values.size() &&
                 right_values[right_pos] <= left_values[left_pos]) {
            right_pos++;
          }
        }
        else {
          while (right_pos < right_values.size() &&
                 right_values[right_pos] < left_values[left_pos]) {
            right_pos++;
          }
        }

        // save position as the desired index
        right_indices[left_pos] = right_pos - 1;
      }
    }
    if (direction == AsofDirection::kForward) {
      int64_t left_pos = 0;
      for (int64_t right_pos = 0; right_pos < right_values.size();
           right_pos++) {
        // find last position in left whose value is less than right's
        // and save positions as the desired index along the way
        if (!strict) {
          while (left_pos < left_values.size() &&
                 left_values[left_pos] <= right_values[right_pos]) {
            right_indices[left_pos] = right_pos;
            left_pos++;
          }
        }
        else {
          while (left_pos < left_values.size() &&
                 left_values[left_pos] < right_values[right_pos]) {
            right_indices[left_pos] = right_pos;
            left_pos++;
          }
        }
      }
    }
    if (direction == AsofDirection::kNearest) {
      throw std::logic_error("'nearest' direction requires asofnear");
    }
  }

  // match two arrays asof ordering (nearest)
  template <class T>
  void asofnear_arr(operand_t left, operand_t right, bool strict,
                    AsofDirection direction,
                    std::vector<int64_t>& left_indices,
                    std::vector<int64_t>& right_indices) {
    std::vector<T>& left_values = get_reference<std::vector<T>>(left);
    std::vector<T>& right_values = get_reference<std::vector<T>>(right);
    using DiffType = decltype(left_values[0] - right_values[0]);

    left_indices.resize(left_values.size());
    std::iota(std::begin(left_indices), std::end(left_indices), 0);
    right_indices.resize(left_values.size(), -1);

    if (direction != AsofDirection::kNearest) {
      throw std::logic_error("asofnear requires 'nearest' direction");
    }

    int64_t right_pos = 0, left_pos = 0;
    while (left_pos < left_values.size()) {
      // find first position in right whose value is greater than left's
      while (right_pos < right_values.size() &&
             right_values[right_pos] <= left_values[left_pos]) {
        right_pos++;
      }

      // set backward and forward positions
      int64_t prev_pos = right_pos - 1;
      int64_t next_pos = right_pos;

      if (right_pos < right_values.size()) {
        // find last position in left whose value is less than right's
        // and compare positions for the desired index along the way
        while (left_pos < left_values.size() &&
               left_values[left_pos] <= right_values[right_pos]) {
          if (prev_pos != -1) {
            DiffType p = left_values[left_pos] - right_values[prev_pos];
            DiffType n = right_values[next_pos] - left_values[left_pos];
            right_indices[left_pos] = (p <= n) ? prev_pos : next_pos;
          }
          else {
            right_indices[left_pos] = next_pos;
          }
          left_pos++;
        }
      }
      else {
        // save position as the desired index
        right_indices[left_pos] = prev_pos;
        left_pos++;
      }
    }
  }

  // match two arrays asof ordering within a tolerance
  template <class T>
  void asofwithin_arr(operand_t left, operand_t right, bool strict,
                      AsofDirection direction, operand_t within_value,
                      std::vector<int64_t>& left_indices,
                      std::vector<int64_t>& right_indices) {
    std::vector<T>& left_values = get_reference<std::vector<T>>(left);
    std::vector<T>& right_values = get_reference<std::vector<T>>(right);
    using DiffType = decltype(left_values[0] - right_values[0]);
    DiffType within = get_value<DiffType>(within_value);

    left_indices.resize(left_values.size());
    std::iota(std::begin(left_indices), std::end(left_indices), 0);
    right_indices.resize(left_values.size(), -1);

    if (direction == AsofDirection::kBackward) {
      int64_t right_pos = 0;
      for (int64_t left_pos = 0; left_pos < left_values.size();
           left_pos++) {
        // find last position in right whose value is less than left's
        if (!strict) {
          while (right_pos < right_values.size() &&
                 right_values[right_pos] <= left_values[left_pos]) {
            right_pos++;
          }
        }
        else {
          while (right_pos < right_values.size() &&
                 right_values[right_pos] < left_values[left_pos]) {
            right_pos++;
          }
        }

        // save position as the desired index if 'within' is met
        if (right_pos != 0) {
          int64_t pos = right_pos - 1;
          DiffType diff = left_values[left_pos] - right_values[pos];
          if (diff <= within) {
            right_indices[left_pos] = pos;
          }
        }
      }
    }
    if (direction == AsofDirection::kForward) {
      int64_t left_pos = 0;
      for (int64_t right_pos = 0; right_pos < right_values.size();
           right_pos++) {
        // find last position in left whose value is less than right's
        // and save positions as the desired index if 'within' is met
        if (!strict) {
          while (left_pos < left_values.size() &&
                 left_values[left_pos] <= right_values[right_pos]) {
            DiffType diff = right_values[right_pos] - left_values[left_pos];
            if (diff <= within) {
              right_indices[left_pos] = right_pos;
            }
            left_pos++;
          }
        }
        else {
          while (left_pos < left_values.size() &&
                 left_values[left_pos] < right_values[right_pos]) {
            DiffType diff = right_values[right_pos] - left_values[left_pos];
            if (diff <= within) {
              right_indices[left_pos] = right_pos;
            }
            left_pos++;
          }
        }
      }
    }
    if (direction == AsofDirection::kNearest) {
      int64_t right_pos = 0, left_pos = 0;
      while (left_pos < left_values.size()) {
        // find first position in right whose value is greater than left's
        while (right_pos < right_values.size() &&
               right_values[right_pos] <= left_values[left_pos]) {
          right_pos++;
        }

        // set backward and forward positions
        int64_t prev_pos = right_pos - 1;
        int64_t next_pos = right_pos;

        if (right_pos < right_values.size()) {
          // find last position in left whose value is less than right's
          // and compare positions, checking that 'within' is met
          while (left_pos < left_values.size() &&
                 left_values[left_pos] <= right_values[right_pos]) {
            if (prev_pos != -1) {
              DiffType p = left_values[left_pos] - right_values[prev_pos];
              DiffType n = right_values[next_pos] - left_values[left_pos];
              if (p <= n) {
                if (p <= within) {
                  right_indices[left_pos] = prev_pos;
                }
              }
              else {
                if (n <= within) {
                  right_indices[left_pos] = next_pos;
                }
              }
            }
            else {
              DiffType diff = right_values[next_pos] - left_values[left_pos];
              if (diff <= within) {
                right_indices[left_pos] = next_pos;
              }
            }
            left_pos++;
          }
        }
        else {
          if (prev_pos != -1) {
            DiffType diff = left_values[left_pos] - right_values[prev_pos];
            if (diff <= within) {
              right_indices[left_pos] = prev_pos;
            }
          }
          left_pos++;
        }
      }
    }
  }

  // match two Dataframes and two arrays (not nearest)
  template <class T>
  void eqasofmatch_df(type_t typee, operand_t left_df, operand_t right_df,
                      operand_t left_arr, operand_t right_arr, bool strict,
                      AsofDirection direction,
                      std::vector<int64_t>& left_indices,
                      std::vector<int64_t>& right_indices) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        std::ostringstream oss;
        oss << "Cannot eqasofmatch a builtin type $" << num;
        throw std::logic_error(oss.str());
      }
      case TypeMask::kUserDefined: {
        // get labels
        std::vector<int64_t> llabs;
        std::vector<int64_t> rlabs;
        categorize_df2(typee, left_df, right_df, llabs, rlabs);

        std::vector<T>& left_values = get_reference<std::vector<T>>(left_arr);
        std::vector<T>& right_values = get_reference<std::vector<T>>(right_arr);

        left_indices.resize(left_values.size());
        std::iota(std::begin(left_indices), std::end(left_indices), 0);
        right_indices.resize(left_values.size(), -1);

        if (direction == AsofDirection::kBackward) {
          std::unordered_map<int64_t, int64_t> m(rlabs.size());
          int64_t right_pos = 0;
          for (int64_t left_pos = 0; left_pos < left_values.size();
               left_pos++) {
            // find last position in right whose value is less than left's
            // and store last-seen position for each label
            if (!strict) {
              while (right_pos < right_values.size() &&
                     right_values[right_pos] <= left_values[left_pos]) {
                m[rlabs[right_pos]] = right_pos;
                right_pos++;
              }
            }
            else {
              while (right_pos < right_values.size() &&
                     right_values[right_pos] < left_values[left_pos]) {
                m[rlabs[right_pos]] = right_pos;
                right_pos++;
              }
            }

            // save last-seen position as the desired index
            auto iter = m.find(llabs[left_pos]);
            if (iter != m.end()) {
              right_indices[left_pos] = iter->second;
            }
          }
        }
        if (direction == AsofDirection::kForward) {
          std::unordered_map<int64_t, std::vector<int64_t>> m(llabs.size());
          int64_t left_pos = 0;
          for (int64_t right_pos = 0; right_pos < right_values.size();
               right_pos++) {
            // find last position in left whose value is less than right's
            // and store dependencies for each label along the way
            if (!strict) {
              while (left_pos < left_values.size() &&
                     left_values[left_pos] <= right_values[right_pos]) {
                m[llabs[left_pos]].push_back(left_pos);
                left_pos++;
              }
            }
            else {
              while (left_pos < left_values.size() &&
                     left_values[left_pos] < right_values[right_pos]) {
                m[llabs[left_pos]].push_back(left_pos);
                left_pos++;
              }
            }

            // restore position for dependencies of this label
            auto iter = m.find(rlabs[right_pos]);
            if (iter != m.end()) {
              for (int64_t pos: iter->second) {
                right_indices[pos] = right_pos;
              }
              m.erase(iter);
            }
          }
        }
        if (direction == AsofDirection::kNearest) {
          throw std::logic_error("'nearest' direction requires eqasofnear");
        }
      }
    }
  }

  // match two Dataframes and two arrays (nearest)
  template <class T>
  void eqasofnear_df(type_t typee, operand_t left_df, operand_t right_df,
                     operand_t left_arr, operand_t right_arr, bool strict,
                     AsofDirection direction,
                     std::vector<int64_t>& left_indices,
                     std::vector<int64_t>& right_indices) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        std::ostringstream oss;
        oss << "Cannot eqasofnear a builtin type $" << num;
        throw std::logic_error(oss.str());
      }
      case TypeMask::kUserDefined: {
        // get labels
        std::vector<int64_t> llabs;
        std::vector<int64_t> rlabs;
        categorize_df2(typee, left_df, right_df, llabs, rlabs);

        std::vector<T>& left_values = get_reference<std::vector<T>>(left_arr);
        std::vector<T>& right_values = get_reference<std::vector<T>>(right_arr);
        using DiffType = decltype(left_values[0] - right_values[0]);

        left_indices.resize(left_values.size());
        std::iota(std::begin(left_indices), std::end(left_indices), 0);
        right_indices.resize(left_values.size(), -1);

        if (direction != AsofDirection::kNearest) {
          throw std::logic_error("eqasofnear requires 'nearest' direction");
        }

        std::unordered_map<int64_t, int64_t> mr(rlabs.size());
        std::unordered_map<int64_t, std::vector<int64_t>> ml(llabs.size());
        int64_t right_pos = 0, left_pos = 0;
        while (left_pos < left_values.size()) {
          // find first position in right whose value is greater than left's
          // and store last-seen position for each label
          while (right_pos < right_values.size() &&
                 right_values[right_pos] <= left_values[left_pos]) {
            mr[rlabs[right_pos]] = right_pos;
            right_pos++;
          }

          if (right_pos < right_values.size()) {
            // find first position in left whose value is greater than right's
            // and store dependencies for each label along the way
            // and pre-fill last-seen value in case we don't see it again
            while (left_pos < left_values.size() &&
                   left_values[left_pos] <= right_values[right_pos]) {
              ml[llabs[left_pos]].push_back(left_pos);
              auto iter = mr.find(llabs[left_pos]);
              if (iter != mr.end()) {
                right_indices[left_pos] = iter->second;
              }
              left_pos++;
            }

            // compare positions and restore for dependencies of this label
            auto next_iter = ml.find(rlabs[right_pos]);
            if (next_iter != ml.end()) {
              int64_t next_pos = right_pos;
              auto prev_iter = mr.find(rlabs[right_pos]);
              if (prev_iter != mr.end()) {
                int64_t prev_pos = prev_iter->second;
                for (int64_t pos: next_iter->second) {
                  DiffType p = left_values[pos] - right_values[prev_pos];
                  DiffType n = right_values[next_pos] - left_values[pos];
                  right_indices[pos] = (p <= n) ? prev_pos : next_pos;
                }
              }
              else {
                for (int64_t pos: next_iter->second) {
                  right_indices[pos] = next_pos;
                }
              }
              ml.erase(next_iter);
            }
          }
          else {
            // save last-seen position as the desired index
            auto iter = mr.find(llabs[left_pos]);
            if (iter != mr.end()) {
              right_indices[left_pos] = iter->second;
            }
            left_pos++;
          }
        }
      }
    }
  }

  // match two Dataframes and two arrays within a tolerance
  template <class T>
  void eqasofwithin_df(type_t typee, operand_t left_df, operand_t right_df,
                       operand_t left_arr, operand_t right_arr, bool strict,
                       AsofDirection direction, operand_t within_value,
                       std::vector<int64_t>& left_indices,
                       std::vector<int64_t>& right_indices) {
    // check tag
    TypeMask mask = TypeMask(typee & 1);
    type_t num = typee >> 1;
    switch (mask) {
      case TypeMask::kBuiltIn: {
        std::ostringstream oss;
        oss << "Cannot eqasofwithin a builtin type $" << num;
        throw std::logic_error(oss.str());
      }
      case TypeMask::kUserDefined: {
        // get labels
        std::vector<int64_t> llabs;
        std::vector<int64_t> rlabs;
        categorize_df2(typee, left_df, right_df, llabs, rlabs);

        std::vector<T>& left_values = get_reference<std::vector<T>>(left_arr);
        std::vector<T>& right_values = get_reference<std::vector<T>>(right_arr);
        using DiffType = decltype(left_values[0] - right_values[0]);
        DiffType within = get_value<DiffType>(within_value);

        left_indices.resize(left_values.size());
        std::iota(std::begin(left_indices), std::end(left_indices), 0);
        right_indices.resize(left_values.size(), -1);

        if (direction == AsofDirection::kBackward) {
          std::unordered_map<int64_t, int64_t> m(rlabs.size());
          int64_t right_pos = 0;
          for (int64_t left_pos = 0; left_pos < left_values.size();
               left_pos++) {
            // find last position in right whose value is less than left's
            // and store last-seen position for each label
            if (!strict) {
              while (right_pos < right_values.size() &&
                     right_values[right_pos] <= left_values[left_pos]) {
                m[rlabs[right_pos]] = right_pos;
                right_pos++;
              }
            }
            else {
              while (right_pos < right_values.size() &&
                     right_values[right_pos] < left_values[left_pos]) {
                m[rlabs[right_pos]] = right_pos;
                right_pos++;
              }
            }

            // save last-seen position as the desired index if 'within' is met
            auto iter = m.find(llabs[left_pos]);
            if (iter != m.end()) {
              int64_t pos = iter->second;
              DiffType diff = left_values[left_pos] - right_values[pos];
              if (diff <= within) {
                right_indices[left_pos] = pos;
              }
            }
          }
        }
        if (direction == AsofDirection::kForward) {
          std::unordered_map<int64_t, std::vector<int64_t>> m(llabs.size());
          int64_t left_pos = 0;
          for (int64_t right_pos = 0; right_pos < right_values.size();
               right_pos++) {
            // find last position in left whose value is less than right's
            // and store dependencies for each label along the way
            if (!strict) {
              while (left_pos < left_values.size() &&
                     left_values[left_pos] <= right_values[right_pos]) {
                m[llabs[left_pos]].push_back(left_pos);
                left_pos++;
              }
            }
            else {
              while (left_pos < left_values.size() &&
                     left_values[left_pos] < right_values[right_pos]) {
                m[llabs[left_pos]].push_back(left_pos);
                left_pos++;
              }
            }

            // restore position for this label if 'within' is met
            auto iter = m.find(rlabs[right_pos]);
            if (iter != m.end()) {
              for (int64_t pos: iter->second) {
                DiffType diff = right_values[right_pos] - left_values[pos];
                if (diff <= within) {
                  right_indices[pos] = right_pos;
                }
              }
              m.erase(iter);
            }
          }
        }
        if (direction == AsofDirection::kNearest) {
          std::unordered_map<int64_t, int64_t> mr(rlabs.size());
          std::unordered_map<int64_t, std::vector<int64_t>> ml(llabs.size());
          int64_t right_pos = 0, left_pos = 0;
          while (left_pos < left_values.size()) {
            // find first position in right whose value is greater than left's
            // and store last-seen position for each label
            while (right_pos < right_values.size() &&
                   right_values[right_pos] <= left_values[left_pos]) {
              mr[rlabs[right_pos]] = right_pos;
              right_pos++;
            }

            if (right_pos < right_values.size()) {
              // find first position in left whose value is greater than right's
              // and store dependencies for each label along the way
              // and pre-fill last-seen value in case we don't see it again
              while (left_pos < left_values.size() &&
                     left_values[left_pos] <= right_values[right_pos]) {
                ml[llabs[left_pos]].push_back(left_pos);
                auto iter = mr.find(llabs[left_pos]);
                if (iter != mr.end()) {
                  int64_t pos = iter->second;
                  DiffType diff = left_values[left_pos] - right_values[pos];
                  if (diff <= within) {
                    right_indices[left_pos] = pos;
                  }
                }
                left_pos++;
              }

              // compare positions and restore for this label if 'within' is met
              auto next_iter = ml.find(rlabs[right_pos]);
              if (next_iter != ml.end()) {
                int64_t next_pos = right_pos;
                auto prev_iter = mr.find(rlabs[right_pos]);
                if (prev_iter != mr.end()) {
                  int64_t prev_pos = prev_iter->second;
                  for (int64_t pos: next_iter->second) {
                    DiffType p = left_values[pos] - right_values[prev_pos];
                    DiffType n = right_values[next_pos] - left_values[pos];
                    if (p <= n) {
                      if (p <= within) {
                        right_indices[pos] = prev_pos;
                      }
                    }
                    else {
                      if (n <= within) {
                        right_indices[pos] = next_pos;
                      }
                    }
                  }
                }
                else {
                  for (int64_t pos: next_iter->second) {
                    DiffType diff = right_values[next_pos] - left_values[pos];
                    if (diff <= within) {
                      right_indices[pos] = next_pos;
                    }
                  }
                }
                ml.erase(next_iter);
              }
            }
            else {
              // save last-seen position as the desired index if 'within' is met
              auto iter = mr.find(llabs[left_pos]);
              if (iter != mr.end()) {
                int64_t pos = iter->second;
                DiffType diff = left_values[left_pos] - right_values[pos];
                if (diff <= within) {
                  right_indices[left_pos] = pos;
                }
              }
              left_pos++;
            }
          }
        }
      }
    }
  }

#include <VVM/asof.h>

  // asofmatch operation
  void asofmatch(operand_t typee, operand_t left, operand_t right,
                 operand_t strictness, operand_t direct,
                 operand_t left_result, operand_t right_result) {
    verify_is_type(typee);
    type_t type_code = typee >> 2;
    vvm_types vvm_typee = static_cast<vvm_types>(type_code >> 1);

    bool strict = get_value<bool>(strictness);
    AsofDirection direction = AsofDirection(get_value<int64_t>(direct));
    std::vector<int64_t>& left_indices =
      get_reference<std::vector<int64_t>>(left_result);
    std::vector<int64_t>& right_indices =
      get_reference<std::vector<int64_t>>(right_result);

    asofmatch_arr(vvm_typee, left, right, strict, direction, left_indices,
                  right_indices);
  }

  // asofnear operation
  void asofnear(operand_t typee, operand_t left, operand_t right,
                operand_t strictness, operand_t direct,
                operand_t left_result, operand_t right_result) {
    verify_is_type(typee);
    type_t type_code = typee >> 2;
    vvm_types vvm_typee = static_cast<vvm_types>(type_code >> 1);

    bool strict = get_value<bool>(strictness);
    AsofDirection direction = AsofDirection(get_value<int64_t>(direct));
    std::vector<int64_t>& left_indices =
      get_reference<std::vector<int64_t>>(left_result);
    std::vector<int64_t>& right_indices =
      get_reference<std::vector<int64_t>>(right_result);

    asofnear_arr(vvm_typee, left, right, strict, direction, left_indices,
                 right_indices);
  }

  // asofwithin operation
  void asofwithin(operand_t typee, operand_t left, operand_t right,
               operand_t strictness, operand_t direct, operand_t within,
               operand_t left_result, operand_t right_result) {
    verify_is_type(typee);
    type_t type_code = typee >> 2;
    vvm_types vvm_typee = static_cast<vvm_types>(type_code >> 1);

    bool strict = get_value<bool>(strictness);
    AsofDirection direction = AsofDirection(get_value<int64_t>(direct));
    std::vector<int64_t>& left_indices =
      get_reference<std::vector<int64_t>>(left_result);
    std::vector<int64_t>& right_indices =
      get_reference<std::vector<int64_t>>(right_result);

    asofwithin_arr(vvm_typee, left, right, strict, direction, within,
                   left_indices, right_indices);
  }

  // eqasofmatch operation
  void eqasofmatch(operand_t df_typee, operand_t left_df, operand_t right_df,
                   operand_t arr_typee, operand_t left_arr, operand_t right_arr,
                   operand_t strictness, operand_t direct,
                   operand_t left_result, operand_t right_result) {
    verify_is_type(df_typee);
    verify_is_type(arr_typee);

    type_t type_code = arr_typee >> 2;
    vvm_types vvm_typee = static_cast<vvm_types>(type_code >> 1);

    bool strict = get_value<bool>(strictness);
    AsofDirection direction = AsofDirection(get_value<int64_t>(direct));
    std::vector<int64_t>& left_indices =
      get_reference<std::vector<int64_t>>(left_result);
    std::vector<int64_t>& right_indices =
      get_reference<std::vector<int64_t>>(right_result);

    eqasofmatch_df(vvm_typee, df_typee >> 2, left_df, right_df, left_arr,
                   right_arr, strict, direction, left_indices, right_indices);
  }

  // eqasofnear operation
  void eqasofnear(operand_t df_typee, operand_t left_df, operand_t right_df,
                  operand_t arr_typee, operand_t left_arr, operand_t right_arr,
                  operand_t strictness, operand_t direct,
                  operand_t left_result, operand_t right_result) {
    verify_is_type(df_typee);
    verify_is_type(arr_typee);

    type_t type_code = arr_typee >> 2;
    vvm_types vvm_typee = static_cast<vvm_types>(type_code >> 1);

    bool strict = get_value<bool>(strictness);
    AsofDirection direction = AsofDirection(get_value<int64_t>(direct));
    std::vector<int64_t>& left_indices =
      get_reference<std::vector<int64_t>>(left_result);
    std::vector<int64_t>& right_indices =
      get_reference<std::vector<int64_t>>(right_result);

    eqasofnear_df(vvm_typee, df_typee >> 2, left_df, right_df, left_arr,
                   right_arr, strict, direction, left_indices, right_indices);
  }

  // eqasofwithin operation
  void eqasofwithin(operand_t df_typee, operand_t left_df, operand_t right_df,
                    operand_t arr_typee, operand_t left_arr,
                    operand_t right_arr, operand_t strictness, operand_t direct,
                    operand_t within, operand_t left_result,
                    operand_t right_result) {
    verify_is_type(df_typee);
    verify_is_type(arr_typee);

    type_t type_code = arr_typee >> 2;
    vvm_types vvm_typee = static_cast<vvm_types>(type_code >> 1);

    bool strict = get_value<bool>(strictness);
    AsofDirection direction = AsofDirection(get_value<int64_t>(direct));
    std::vector<int64_t>& left_indices =
      get_reference<std::vector<int64_t>>(left_result);
    std::vector<int64_t>& right_indices =
      get_reference<std::vector<int64_t>>(right_result);

    eqasofwithin_df(vvm_typee, df_typee >> 2, left_df, right_df, left_arr,
                    right_arr, strict, direction, within, left_indices,
                    right_indices);
  }

  // take columns from a Dataframe according to the new type
  void take_df(type_t old_type, type_t new_type, Dataframe& xs,
               Dataframe& ys) {
    verify_user_defined(old_type);
    verify_user_defined(new_type);

    auto xs_members = get_type_members(old_type, types_);
    auto ys_members = get_type_members(new_type, types_);

    // index xs' member names
    std::unordered_map<std::string, size_t> m(xs.size());
    for (size_t i = 0; i < xs.size(); i++) {
      m[xs_members[i].name] = i;
    }

    // save columns from xs into ys by member name
    for (size_t i = 0; i < ys.size(); i++) {
      auto iter = m.find(ys_members[i].name);
      if (iter == m.end()) {
        std::string msg = "Unkown target column " + ys_members[i].name;
        throw std::logic_error(msg);
      }
      size_t j = iter->second;
      ys[i] = xs[j];
    }
  }

  // take operation
  void take(operand_t old_type, operand_t new_type, operand_t src,
            operand_t dst) {
    verify_is_type(old_type);
    verify_is_type(new_type);

    alloc(new_type, dst);
    Dataframe& xs = get_reference<Dataframe>(src);
    Dataframe& ys = get_reference<Dataframe>(dst);

    take_df(old_type >> 2, new_type >> 2, xs, ys);
  }

  // merge two Dataframes together
  void concat_df(type_t result_type, Dataframe& left, Dataframe& right,
                 Dataframe& result) {
    verify_user_defined(result_type);
    auto members = get_type_members(result_type, types_);

    // verify that left and right are the same length
    int64_t left_length = len_df(left, members);
    int64_t right_length = len_df(right, members, left.size());
    if (left_length != right_length) {
      throw std::runtime_error("Mismatch dataframe lengths");
    }

    // copy left and then right
    size_t i = 0;
    for (; i < left.size(); i++) {
      result[i] = left[i];
    }
    for (size_t j = 0; j < right.size(); i++, j++) {
      result[i] = right[j];
    }
  }

  // concat operation
  void concat(operand_t result_type, operand_t left, operand_t right,
              operand_t result) {
    verify_is_type(result_type);

    alloc(result_type, result);
    Dataframe& xs = get_reference<Dataframe>(left);
    Dataframe& ys = get_reference<Dataframe>(right);
    Dataframe& zs = get_reference<Dataframe>(result);

    concat_df(result_type >> 2, xs, ys, zs);
  }

 public:

  // run interpreter; results will be in a saved string
  void interpret(const Program& program) {
    // clear the saved string before running interpreter
    saved_string_.clear();

    // append user-defined types to the list of known types
    for (auto& kv: program.types) {
      types_[kv.first >> 1] = kv.second;
    }

    // append a const pool to register bank
    for (auto& kv: program.constants) {
      Value& key = *get_register<void>(kv.first);
      Value value = remove_tag(kv.second);
      key = value;
    }

    // run everything
    dispatch(program.instructions);
  }

  // get the saved string after running interpreter
  std::string get_saved_string() {
    return saved_string_;
  }

#include <VVM/dispatch.h>
} interpreter;


// interpret bytecode and return any saved string
std::string interpret(const Program& program) {
  // run interpreter
  try {
    interpreter.interpret(program);
  }
  catch (std::exception& e) {
    std::string err_msg = e.what();
    throw std::runtime_error("Error: " + err_msg + '\n');
  }

  // return result
  return interpreter.get_saved_string();
}
}  // namespace VVM

