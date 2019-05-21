/*
 * Nil -- missing data representation
 *
 * Copyright (C) 2019 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

/*
 * Missing data is represented as "nil", a sentinel value whose definition
 * depends on the specific type. For integers, this is the max value; for
 * floating-point numbers, it's the IEEE NaN.
 *
 * This file introduces three functions:
 *   1. nil_value<T>()  : returns the type-specific sentinel
 *   2. is_nil(T x)     : whether a value represents a sentinel
 *   3. is_int_nil(T x) : like above, but returns false for floats
 *
 * The above functions are predefined for standard C++ types. Any new type
 * must redefine all three.
 */
namespace VVM {
// need our own definition of is_integral since std includes bool and char
template<class T> struct is_int_    : public std::false_type {};
template<> struct is_int_<int16_t>  : public std::true_type {};
template<> struct is_int_<uint16_t> : public std::true_type {};
template<> struct is_int_<int32_t>  : public std::true_type {};
template<> struct is_int_<uint32_t> : public std::true_type {};
template<> struct is_int_<int64_t>  : public std::true_type {};
template<> struct is_int_<uint64_t> : public std::true_type {};
template<class T> struct is_int     : public is_int_<std::remove_cv_t<T>> {};

// return sentinel value for nil
template<class T> constexpr
typename std::enable_if<is_int<T>::value, T>::type
nil_value() {
  return std::numeric_limits<T>::max();
}

template<class T> constexpr
typename std::enable_if<std::is_floating_point<T>::value, T>::type
nil_value() {
  return std::numeric_limits<T>::quiet_NaN();
}

template<class T> constexpr
typename std::enable_if<std::is_same<T, bool>::value, T>::type
nil_value() {
  return false;
}

template<class T>
typename std::enable_if<std::is_same<T, std::string>::value, T>::type
nil_value() {
  return std::string();
}

template<class T> constexpr
typename std::enable_if<std::is_same<T, char>::value, T>::type
nil_value() {
  return std::numeric_limits<char>::max();
}

// return whether the value is nil; doesn't make sense for bool or std::string
template<class T> constexpr
typename std::enable_if<is_int<T>::value, bool>::type
is_nil(T x) {
  return x == nil_value<T>();
}

template<class T>
typename std::enable_if<std::is_floating_point<T>::value, bool>::type
is_nil(T x) {
  return std::isnan(x);
}

constexpr bool is_nil(bool) {
  return false;
}

template<class T> constexpr
typename std::enable_if<std::is_same<T, std::string>::value, bool>::type
is_nil(T x) {
  return false;
}

constexpr bool is_nil(char c) {
  return c == nil_value<char>();
}

// fast path since IEEE NaN propogates in hardware
template<class T> constexpr
typename std::enable_if<is_int<T>::value, bool>::type
is_int_nil(T x) {
  return x == nil_value<T>();
}

template<class T> constexpr
typename std::enable_if<std::is_floating_point<T>::value, bool>::type
is_int_nil(T x) {
  return false;
}

constexpr bool is_int_nil(bool) {
  return false;
}

template<class T> constexpr
typename std::enable_if<std::is_same<T, std::string>::value, bool>::type
is_int_nil(T x) {
  return false;
}

constexpr bool is_int_nil(char c) {
  return c == nil_value<char>();
}
}  // namespace VVM

