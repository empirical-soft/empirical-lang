/*
 * Timer -- functions for performance evaluation
 *
 * Copyright (C) 2019--2020 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#pragma once

#include <string>
#include <iostream>
#include <exception>
#include <unordered_map>
#include <chrono>

/*
 * This library contains two mechanisms for accurately timing code:
 *
 * To run once in a given scope:
 *
 *   {
 *     Timer timer;               // starts clock automatically
 *     foo();
 *     timer.check("foo", "ms");  // print how long foo() took in milliseconds
 *     bar();
 *     timer.check("bar", "ms");  // print how long bar() took in milliseconds
 *   }
 *
 * To run repeatedly across scopes:
 *
 *   Profiler profiler;
 *
 *   profiler.start();             // must manually start a clock each time
 *   foo();
 *   profiler.add("foo");          // accumulates the run
 *
 *   profiler.start();
 *   bar();
 *   profiler.add("bar");
 *
 *   profiler.check("foo", "ms");  // print total accumulated runtime
 *   profiler.check("bar", "ms");
 *
 *   profiler.clear();             // remove all timings
 */
namespace VVM {
class Timer {
  std::chrono::high_resolution_clock::time_point start_;

 public:
  Timer(): start_(std::chrono::high_resolution_clock::now()) {}

  void check(const std::string& name, const std::string& unit) {
    auto stop = std::chrono::high_resolution_clock::now();
    std::chrono::nanoseconds diff = stop - start_;

    int64_t result;
    if (unit == "ns") {
      result = diff.count();
    }
    else if (unit == "us") {
      result =
        std::chrono::duration_cast<std::chrono::microseconds>(diff).count();
    }
    else if (unit == "ms") {
      result =
        std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
    }
    else if (unit == "s") {
      result =
        std::chrono::duration_cast<std::chrono::seconds>(diff).count();
    }
    else {
      throw std::logic_error("Unknown unit " + unit);
    }

    std::cout << name << ' ' << result << unit << std::endl;
    start_ = std::chrono::high_resolution_clock::now();
  }
};


class Profiler {
  std::unordered_map<std::string, std::chrono::nanoseconds> times_;
  std::chrono::high_resolution_clock::time_point start_;

 public:
  Profiler() {}

  void start() {
    start_ = std::chrono::high_resolution_clock::now();
  }

  void add(const std::string& name) {
    auto stop = std::chrono::high_resolution_clock::now();
    times_[name] += (stop - start_);
  }

  void check(const std::string& name, const std::string& unit) {
    std::chrono::nanoseconds diff = times_[name];

    int64_t result;
    if (unit == "ns") {
      result = diff.count();
    }
    else if (unit == "us") {
      result =
        std::chrono::duration_cast<std::chrono::microseconds>(diff).count();
    }
    else if (unit == "ms") {
      result =
        std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
    }
    else if (unit == "s") {
      result =
        std::chrono::duration_cast<std::chrono::seconds>(diff).count();
    }
    else {
      throw std::logic_error("Unknown unit " + unit);
    }

    std::cout << name << ' ' << result << unit << std::endl;
    start_ = std::chrono::high_resolution_clock::now();
  }

  void clear() {
    times_.clear();
  }
};

}  // namespace VVM

