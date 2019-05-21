/*
 * Conversion -- convert between types, particularly to and from strings
 *
 * Copyright (C) 2019 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#pragma once

#include <vector>

#include <VVM/utils/nil.hpp>

/*
 * Convert a C++ value to and from a string. This is lexical casting with a
 * few specifics for Empirical, like nil handling and zero trimming.
 *
 * This file introduces these functions:
 *   1. to_repr(T x)           : generates a string intended for a console
 *   2. to_string(T x)         : generates a string for internal use
 *   3. from_string<T>(x)      : parses a string into a value
 *   4. trim_trailing_zeros(x) : removes excess zeros from a converted float
 *
 * The above functions are predefined for standard C++ types. Any new type
 * must redefine all four.
 *
 * This extra function does not need to be redefined:
 *   5. super_cast<T, U>(T x) : all-in-one cast for lexical and static
 */
namespace VVM {
// remove excess zeros from converted floating points
template<class T>
typename std::enable_if<std::is_integral<T>::value ||
                        std::is_same<T, std::string>::value, void>::type
trim_trailing_zeros(std::vector<std::string>& xs) {
  ;
}

template<class T>
typename std::enable_if<std::is_floating_point<T>::value, void>::type
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

  // if all elements end in zero, remove it
  bool all_zeros = true;
  while (all_zeros) {
    for (const auto& x: xs) {
      if (!x.empty() && x.back() != '0') {
        all_zeros = false;
      }
    }
    if (all_zeros) {
      for (auto& x: xs) {
        x.pop_back();
      }
    }
  }
  for (auto& x: xs) {
    if (!x.empty() && x.back() == '.') {
      x.push_back('0');
    }
  }
}

template<class T>
typename std::enable_if<std::is_integral<T>::value ||
                        std::is_same<T, std::string>::value, std::string>::type
trim_trailing_zeros(const std::string& x) {
  return x;
}

template<class T>
typename std::enable_if<std::is_floating_point<T>::value, std::string>::type
trim_trailing_zeros(const std::string& x) {
  std::string y = x;
  while (!y.empty() && y.back() == '0') {
    y.pop_back();
  }
  if (!y.empty() && y.back() == '.') {
    y.push_back('0');
  }
  return y;
}

// generate a string for display in console
template<class T>
typename std::enable_if<is_int<T>::value, std::string>::type
to_repr(T x) {
  if (is_nil(x)) {
    return "nil";
  }
  return std::to_string(x);
}

template<class T>
typename std::enable_if<std::is_floating_point<T>::value, std::string>::type
to_repr(T x) {
  if (is_nil(x)) {
    return "nan";
  }
  return trim_trailing_zeros<T>(std::to_string(x));
}

inline std::string to_repr(bool b) {
  return b ? "true" : "false";
}

inline std::string to_repr(const std::string& s) {
  // TODO should escape the string
  return '"' + s + '"';
}

inline std::string to_repr(char c) {
  if (is_nil(c)) {
    return "''";
  }
  // TODO should escape the character
  return "'" + std::string(1, c) + "'";
}

// generate a string for internal use
template<class T>
typename std::enable_if<is_int<T>::value, std::string>::type
to_string(T x) {
  if (is_nil(x)) {
    return std::string();
  }
  return std::to_string(x);
}

template<class T>
typename std::enable_if<std::is_floating_point<T>::value, std::string>::type
to_string(T x) {
  if (is_nil(x)) {
    return std::string();
  }
  return std::to_string(x);
}

inline std::string to_string(bool b) {
  return b ? "true" : "false";
}

inline std::string to_string(const std::string& s) {
  return s;
}

inline std::string to_string(char c) {
  if (is_nil(c)) {
    return std::string();
  }
  return std::string(1, c);
}

// parse a string into a value
template<class T>
typename std::enable_if<std::is_same<T, int64_t>::value, T>::type
from_string(const std::string& text) {
  try {
    size_t pos = 0;
    int64_t result = std::stol(text, &pos);
    if (pos != text.size()) {
      return nil_value<int64_t>();
    }
    return result;
  }
  catch (std::invalid_argument) {
    return nil_value<int64_t>();
  }
}

template<class T>
typename std::enable_if<std::is_same<T, double>::value, T>::type
from_string(const std::string& text) {
  try {
    size_t pos = 0;
    double result = std::stod(text, &pos);
    if (pos != text.size()) {
      return nil_value<double>();
    }
    return result;
  }
  catch (std::invalid_argument) {
    return nil_value<double>();
  }
}

template<class T>
typename std::enable_if<std::is_same<T, std::string>::value, T>::type
from_string(const std::string& text) {
  return text;
}

template<class T>
typename std::enable_if<std::is_same<T, bool>::value, T>::type
from_string(const std::string& text) {
  return text == "true";
}

template<class T>
typename std::enable_if<std::is_same<T, char>::value, T>::type
from_string(const std::string& text) {
  if (text.size() == 1) {
    return text[0];
  }
  return nil_value<char>();
}

// an all-in-one cast that handles lexical and static varities
template<class T, class U>
typename std::enable_if<std::is_same<T, std::string>::value &&
                        std::is_same<U, std::string>::value, U>::type
super_cast(T x) {
  return x;
}

template<class T, class U>
typename std::enable_if<std::is_same<T, std::string>::value &&
                        !std::is_same<U, std::string>::value, U>::type
super_cast(T x) {
  return from_string<U>(x);
}

template<class T, class U>
typename std::enable_if<!std::is_same<T, std::string>::value &&
                        std::is_same<U, std::string>::value, U>::type
super_cast(T x) {
  return trim_trailing_zeros<T>(to_string(x));
}

template<class T, class U>
typename std::enable_if<!std::is_same<T, std::string>::value &&
                        !std::is_same<U, std::string>::value, U>::type
super_cast(T x) {
  return is_nil(x) ? nil_value<U>() : static_cast<U>(x);
}
}  // namespace VVM

