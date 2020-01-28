/*
 * Tests for text conversion
 *
 * Copyright (C) 2019--2020 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#include "test.hpp"

#include <VVM/utils/conversion.hpp>

int main() {
  main_ret = 0;

  TEST(P(VVM::super_cast<std::string, int64_t>)("5"), 5)
  TEST(P(VVM::super_cast<std::string, int64_t>)("-5"), -5)
  TEST(P(VVM::super_cast<std::string, int64_t>)("0"), 0)
  TEST_NIL(P(VVM::super_cast<std::string, int64_t>)("x"))

  TEST(P(VVM::super_cast<std::string, double>)("5.5"), 5.5)
  TEST(P(VVM::super_cast<std::string, double>)("-5.5"), -5.5)
  TEST(P(VVM::super_cast<std::string, double>)("0.0"), 0.0)
  TEST_NIL(P(VVM::super_cast<std::string, double>)("x"))

  TEST(P(VVM::super_cast<std::string, bool>)("true"), true)
  TEST(P(VVM::super_cast<std::string, bool>)("false"), false)
  TEST(P(VVM::super_cast<std::string, bool>)("x"), false)

  TEST(P(VVM::super_cast<std::string, char>)("c"), 'c')
  TEST_NIL(P(VVM::super_cast<std::string, char>)(""))

  TEST(P(VVM::super_cast<int64_t, std::string>)(5), "5")
  TEST(P(VVM::super_cast<int64_t, std::string>)(-5), "-5")
  TEST(P(VVM::super_cast<int64_t, std::string>)(0), "0")
  TEST(P(VVM::super_cast<int64_t, std::string>(VVM::nil_value<int64_t>())), "")

  TEST(P(VVM::super_cast<double, std::string>(5.5)), "5.5")
  TEST(P(VVM::super_cast<double, std::string>(-5.5)), "-5.5")
  TEST(P(VVM::super_cast<double, std::string>(0.0)), "0.0")
  TEST(P(VVM::super_cast<double, std::string>(VVM::nil_value<double>())), "")

  TEST(P(VVM::super_cast<bool, std::string>(true)), "true")
  TEST(P(VVM::super_cast<bool, std::string>(false)), "false")
  TEST(P(VVM::super_cast<bool, std::string>(VVM::nil_value<bool>())), "false")

  TEST(P(VVM::super_cast<char, std::string>('c')), "c")
  TEST(P(VVM::super_cast<char, std::string>(VVM::nil_value<char>())), "")

  TEST(P(VVM::super_cast<std::string, std::string>)("Hello"), "Hello")
  TEST(P(VVM::super_cast<std::string, std::string>)(""), "")

  TEST(P(VVM::super_cast<int64_t, double>)(5), 5.0)
  TEST(P(VVM::super_cast<double, int64_t>)(5.0), 5)

  TEST(VVM::to_repr(5), "5")
  TEST(VVM::to_repr(VVM::nil_value<int64_t>()), "nil")

  TEST(VVM::to_repr(5.5), "5.5")
  TEST(VVM::to_repr(VVM::nil_value<double>()), "nan")

  TEST(VVM::to_repr(true), "true")
  TEST(VVM::to_repr(VVM::nil_value<bool>()), "false")

  TEST(VVM::to_repr('c'), "'c'")
  TEST(VVM::to_repr(VVM::nil_value<char>()), "''")

  return main_ret;
}

