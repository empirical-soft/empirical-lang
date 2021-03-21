/*
 * Tests for timestamp and related types
 *
 * Copyright (C) 2019--2021 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#include "test.hpp"

#include <VVM/utils/timestamp.hpp>
#include <VVM/utils/conversion.hpp>

int main() {
  main_ret = 0;

  TEST(P(VVM::super_cast<std::string, VVM::Timestamp>)("2019-03-28 22:16:33.441076"),
       VVM::Timestamp(1553811393441076000))

  TEST(P(VVM::super_cast<std::string, VVM::Timestamp>)("2019-03-28 22:16:33"),
       VVM::Timestamp(1553811393000000000))

  TEST(P(VVM::super_cast<std::string, VVM::Timestamp>)("2019-03-28"),
       VVM::Timestamp(1553731200000000000))

  TEST_NIL(P(VVM::super_cast<std::string, VVM::Timestamp>)("err"))

  TEST(P(VVM::super_cast<VVM::Timestamp, std::string>)(VVM::Timestamp(1553811393441076000)),
       "2019-03-28 22:16:33.441076")


  TEST(P(VVM::super_cast<std::string, VVM::Timedelta>)("-00:01"),
       VVM::Timedelta(-60000000000))

  TEST(P(VVM::super_cast<std::string, VVM::Timedelta>)("00:01"),
       VVM::Timedelta(60000000000))

  TEST(P(VVM::super_cast<std::string, VVM::Timedelta>)("00:02:00"),
       VVM::Timedelta(120000000000))

  TEST(P(VVM::super_cast<std::string, VVM::Timedelta>)("00:02:00.005"),
       VVM::Timedelta(120005000000))

  TEST(P(VVM::super_cast<std::string, VVM::Timedelta>)("1 days"),
       VVM::Timedelta(86400000000000))

  TEST(P(VVM::super_cast<std::string, VVM::Timedelta>)("1 days 01:00"),
       VVM::Timedelta(90000000000000))

  TEST(P(VVM::super_cast<std::string, VVM::Timedelta>)("1 days 01:00:05"),
       VVM::Timedelta(90005000000000))

  TEST(P(VVM::super_cast<std::string, VVM::Timedelta>)("1 days 01:00:05.000020"),
       VVM::Timedelta(90005000020000))

  TEST_NIL(P(VVM::super_cast<std::string, VVM::Timedelta>)("err"))

  TEST(P(VVM::super_cast<VVM::Timedelta, std::string>)(VVM::Timedelta(60000000000)),
       "00:01:00")

  TEST(P(VVM::super_cast<VVM::Timedelta, std::string>)(VVM::Timedelta(86400000000000)),
       "1 days")

  TEST(P(VVM::super_cast<VVM::Timedelta, std::string>)(VVM::Timedelta(90000000000000)),
       "1 days 01:00:00")


  TEST(P(VVM::super_cast<std::string, VVM::Date>)("2019-03-28"),
       VVM::Date(1553731200000000000))

  TEST(P(VVM::super_cast<VVM::Date, std::string>)(VVM::Date(1553731200000000000)),
       "2019-03-28")


  TEST(P(VVM::super_cast<std::string, VVM::Time>)("22:16:33.441076"),
       VVM::Time(80193441076000))

  TEST(P(VVM::super_cast<VVM::Time, std::string>)(VVM::Time(80193441076000)),
       "22:16:33.441076")


  TEST(VVM::Date(1553731200000000000) + VVM::Time(80193441076000), VVM::Timestamp(1553811393441076000))


  TEST(VVM::Timestamp(7) - VVM::Timestamp(3), VVM::Timedelta(4))

  TEST(VVM::Timestamp(3) + VVM::Timedelta(4), VVM::Timestamp(7))

  return main_ret;
}

