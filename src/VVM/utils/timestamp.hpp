/*
 * Timestamp header -- defines timestamp and related types
 *
 * Copyright (C) 2019 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#pragma once

#include <iomanip>
#include <sstream>
#include <limits>
#include <vector>
#include <type_traits>

#ifdef max
#undef max
#endif

namespace VVM {

/*** forward declarations (defined in timestamp.cpp) ***/

int64_t now_nanos();

bool is_inferred_timestamp(const std::string& format);
bool is_inferred_date(const std::string& format);
bool is_inferred_time(const std::string& format);
std::string infer_strtime_format(const std::string& str);

std::string nanos_to_string(int64_t value, const std::string& format);
std::string nanos_to_string(int64_t nanos);
int64_t nanos_from_string(const std::string& str, std::string format);
int64_t nanos_from_string(const std::string& str);
std::string delta_to_string(int64_t delta);
int64_t delta_from_string(const std::string& str);

/*** strongly typed container of integer ***/

// the class just wraps the integer that represents nanoseconds
#define CLASS(TYPE) class TYPE {\
 protected:\
  int64_t value_;\
 public:\
  TYPE(): value_(0) {}\
  explicit TYPE(int64_t v): value_(v) {}\
  constexpr explicit operator int64_t() {return value_;}\
};

CLASS(Timestamp)
CLASS(Timedelta)

#undef CLASS

// like above, but also converts between Timestamp and self
#define CLASS_CONV(TYPE,CONV) class TYPE {\
 protected:\
  int64_t value_;\
 public:\
  TYPE(): value_(0) {}\
  explicit TYPE(int64_t v): value_(v) {}\
  explicit TYPE(Timestamp t) {\
    static const int64_t ns_per_day = 86400000000000;\
    value_ = CONV;\
  }\
  constexpr explicit operator int64_t() {return value_;}\
  explicit operator Timestamp() {return Timestamp(value_);}\
};

CLASS_CONV(Date,(int64_t(t) / ns_per_day) * ns_per_day)
CLASS_CONV(Time,int64_t(t) % ns_per_day)

#undef CLASS_CONV

// type trait to include all above containers
template<class T> struct is_datetime_     : public std::false_type {};
template<> struct is_datetime_<Timestamp> : public std::true_type {};
template<> struct is_datetime_<Timedelta> : public std::true_type {};
template<> struct is_datetime_<Date>      : public std::true_type {};
template<> struct is_datetime_<Time>      : public std::true_type {};
template<class T> struct is_datetime :
  public is_datetime_<std::remove_cv_t<T>> {};

/*** operators ***/

// comparison operators exist for all types
#define CMPOP(TYPE,OP) inline bool operator OP(TYPE lhs, TYPE rhs) {\
  return static_cast<int64_t>(lhs) OP static_cast<int64_t>(rhs);\
}

#define MK_CMPOP(TYPE)\
CMPOP(TYPE,==)\
CMPOP(TYPE,!=)\
CMPOP(TYPE,<)\
CMPOP(TYPE,<=)\
CMPOP(TYPE,>)\
CMPOP(TYPE,>=)

MK_CMPOP(Timestamp)
MK_CMPOP(Timedelta)
MK_CMPOP(Date)
MK_CMPOP(Time)

#undef CMPOP
#undef MK_CMPOP

// adding a Date and Time yields a Timestamp
inline Timestamp operator+(Date lhs, Time rhs) {
  return Timestamp(static_cast<int64_t>(lhs) + static_cast<int64_t>(rhs));
}

// all operators permitted between pair of Timedeltas
#define BINOP(OP) inline Timedelta operator OP(Timedelta lhs, Timedelta rhs) {\
  return Timedelta(static_cast<int64_t>(lhs) OP static_cast<int64_t>(rhs));\
}

BINOP(+)
BINOP(-)
BINOP(*)
BINOP(/)

#undef BINOP

// operators with a Timedelta
#define BINOP(OP,T1,T2,RT) inline RT operator OP(T1 lhs, T2 rhs) {\
  return RT(static_cast<int64_t>(lhs) OP static_cast<int64_t>(rhs));\
}

#define MK_BINOP(TYPE) \
BINOP(-,TYPE,TYPE,Timedelta)\
BINOP(+,TYPE,Timedelta,TYPE)\
BINOP(+,Timedelta,TYPE,TYPE)\
BINOP(-,TYPE,Timedelta,TYPE)\
BINOP(*,TYPE,Timedelta,TYPE)\
BINOP(*,Timedelta,TYPE,TYPE)\
BINOP(/,TYPE,Timedelta,TYPE)

MK_BINOP(Timestamp)
MK_BINOP(Date)
MK_BINOP(Time)

#undef MK_BINOP
#undef BINOP

/*** nil and string conversion ***/

// nil value routines
#define NIL(TYPE) template<class T> constexpr \
typename std::enable_if<std::is_same<T, TYPE>::value, T>::type \
nil_value() {\
  return TYPE(std::numeric_limits<int64_t>::max());\
}\
inline bool is_nil(TYPE x) {\
  return x == nil_value<TYPE>();\
}\
inline bool is_int_nil(TYPE x) {\
  return x == nil_value<TYPE>();\
}

NIL(Timestamp)
NIL(Timedelta)
NIL(Date)
NIL(Time)

#undef NIL

// remove excess zeros
template<class T>
inline typename std::enable_if<is_datetime<T>::value, std::string>::type
trim_trailing_zeros(const std::string& x) {
  std::string y = x;
  while (y.size() >= 4 && y.substr(y.size() - 3) == "000") {
    y.pop_back();
    y.pop_back();
    y.pop_back();
  }
  if (!y.empty() && y.back() == '.') {
    y.pop_back();
  }
  return y;
}

template<class T>
inline typename std::enable_if<is_datetime<T>::value, void>::type
trim_trailing_zeros(std::vector<std::string>& xs) {
  // ensure this array is worth looking at
  if (xs.empty()) {
    return;
  }
  bool all_empty = true;
  for (const auto& x: xs) {
    if (!x.empty()) {
      all_empty = false;
    }
  }
  if (all_empty) {
    return;
  }

  // if all elements end in three zeros, remove them
  bool all_zeros = true;
  while (all_zeros) {
    for (const auto& x: xs) {
      if (!x.empty() && (x.size() < 4 || x.substr(x.size() - 3) != "000")) {
        all_zeros = false;
      }
    }
    if (all_zeros) {
      for (auto& x: xs) {
        x.pop_back();
        x.pop_back();
        x.pop_back();
      }
    }
  }
  for (auto& x: xs) {
    if (!x.empty() && x.back() == '.') {
      x.pop_back();
    }
  }
}

// generate string for console
inline std::string to_repr(Timestamp t) {
  if (is_nil(t)) {
    return "Timestamp(nil)";
  }
  auto s = trim_trailing_zeros<Timestamp>(nanos_to_string(int64_t(t)));
  return "Timestamp(\"" + s + "\")";
}

inline std::string to_repr(Timedelta t) {
  if (is_nil(t)) {
    return "Timedelta(nil)";
  }
  auto s = trim_trailing_zeros<Timedelta>(delta_to_string(int64_t(t)));
  return "Timedelta(\"" + s + "\")";
}

inline std::string to_repr(Date t) {
  if (is_nil(t)) {
    return "Date(nil)";
  }
  return "Date(\"" + nanos_to_string(int64_t(t), "%Y-%m-%d") + "\")";
}

inline std::string to_repr(Time t) {
  if (is_nil(t)) {
    return "Time(nil)";
  }
  auto s =
    trim_trailing_zeros<Time>(nanos_to_string(int64_t(t), "%H:%M:%S.%f"));
  return "Time(\"" + s + "\")";
}

// generate string for internal use
inline std::string to_string(Timestamp t) {
  if (is_nil(t)) {
    return std::string();
  }
  return nanos_to_string(int64_t(t));
}

inline std::string to_string(Timedelta t) {
  if (is_nil(t)) {
    return std::string();
  }
  return delta_to_string(int64_t(t));
}

inline std::string to_string(Date t) {
  if (is_nil(t)) {
    return std::string();
  }
  return nanos_to_string(int64_t(t), "%Y-%m-%d");
}

inline std::string to_string(Time t) {
  if (is_nil(t)) {
    return std::string();
  }
  return nanos_to_string(int64_t(t), "%H:%M:%S.%f");
}

// parse string
template<class T>
inline typename std::enable_if<std::is_same<T, Timestamp>::value, T>::type
from_string(const std::string& text) {
  return Timestamp(nanos_from_string(text));
}

template<class T>
inline typename std::enable_if<std::is_same<T, Timedelta>::value, T>::type
from_string(const std::string& text) {
  return Timedelta(delta_from_string(text));
}

template<class T>
inline typename std::enable_if<std::is_same<T, Date>::value, T>::type
from_string(const std::string& text) {
  return Date(nanos_from_string(text));
}

template<class T>
inline typename std::enable_if<std::is_same<T, Time>::value, T>::type
from_string(const std::string& text) {
  return Time(nanos_from_string(text));
}
}  // namespace VVM


/*** hash function for std::unordered_map ***/

namespace std {
#define HASH(TYPE) template<>\
struct hash<VVM::TYPE> {\
  size_t operator()(VVM::TYPE t) const noexcept {\
    std::hash<int64_t> h;\
    return h(int64_t(t));\
  }\
};

HASH(Timestamp)
HASH(Timedelta)
HASH(Date)
HASH(Time)

#undef HASH
} // namespace std


/*** stream operators ***/

#define STREAM(TYPE) inline std::ostream&\
operator<<(std::ostream& os, VVM::TYPE t) {\
  os << VVM::to_string(t);\
  return os;\
}

STREAM(Timestamp)
STREAM(Timedelta)
STREAM(Date)
STREAM(Time)

#undef STREAM

