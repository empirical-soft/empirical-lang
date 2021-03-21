/*
 * Test helper routines
 *
 * Copyright (C) 2019--2021 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#pragma once

#include <iostream>

int main_ret;

#define TEST(x, y) if (x != y) {\
  std::cout << __FILE__ << '(' << __LINE__ << "): "\
            << #x << " != " << #y << " => (" << x << " != " << y << ')'\
            << std::endl;\
  main_ret = 1;\
}


#define TEST_NIL(x) if (!VVM::is_nil(x)) {\
  std::cout << __FILE__ << '(' << __LINE__ << "): "\
            << #x << " wasn't nil => (" << x << ')'\
            << std::endl;\
  main_ret = 1;\
}

#define P(...) __VA_ARGS__

