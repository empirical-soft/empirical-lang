/*
 * Timestamp -- routines for handling timestamp and related types
 *
 * Copyright (C) 2019--2020 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN
#include <windows.h>
#else  // WIN32
#include <sys/time.h>
#endif  // WIN32

#include <ctime>
#include <cstring>

#include <VVM/utils/timestamp.hpp>

#include <strtime/strtime.h>

namespace VVM {

/*** helper routines ***/

// get current time in nanoseconds
int64_t now_nanos() {
#ifdef WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);  // or GetSystemTimePreciseAsFileTime()
  __int64 win_time = (static_cast<__int64>(ft.dwHighDateTime) << 32) |
                      static_cast<__int64>(ft.dwLowDateTime);
  // Windows is 100ns ticks since 1601-01-01
  int64_t unix_time = (win_time - 116444736000000000) * 100;
#else  // WIN32
  timeval tv;
  gettimeofday(&tv, nullptr);  // or clock_gettime()
  int64_t unix_time = ((tv.tv_sec * 1000000) + tv.tv_usec) * 1000;
#endif  // WIN32
  return unix_time;
}

/*** formatted string conversion ***/

// returns whether format string represents a valid timestamp
bool is_inferred_timestamp(const std::string& format) {
  return (format == "%Y-%m-%d" ||
          format == "%Y/%m/%d" ||
          format == "%H:%M" ||
          format == "%H:%M:%S" ||
          format == "%H:%M:%S.%f" ||
          format == "%Y-%m-%d %H:%M:%S" ||
          format == "%Y-%m-%d %H:%M:%S.%f" ||
          format == "%Y/%m/%d %H:%M:%S" ||
          format == "%Y/%m/%d %H:%M:%S.%f");
}

// returns whether format string represents a valid date
bool is_inferred_date(const std::string& format) {
  return (format == "%Y-%m-%d" ||
          format == "%Y/%m/%d");
}

// returns whether format string represents a valid time
bool is_inferred_time(const std::string& format) {
  return (format == "%H:%M" ||
          format == "%H:%M:%S" ||
          format == "%H:%M:%S.%f");
}

// returns the inferred format string
std::string infer_strtime_format(const std::string& str) {
  char buffer[80];
  ::istrtime(str.c_str(), buffer, sizeof(buffer));
  return buffer;
}

// returns string according to format
std::string nanos_to_string(int64_t value, const std::string& format) {
  static const int64_t ns_per_sec = 1000000000;
  time_t clock = value / ns_per_sec;
  int nanos = value % ns_per_sec;
  tm time;
  memset(&time, 0, sizeof(tm));
  fast_gmtime(&clock, &time);
  char buffer[80];
  ::strftime_ns(buffer, sizeof(buffer), format.c_str(), &time, nanos);
  return buffer;
}

// returns string formatted to nanoseconds
std::string nanos_to_string(int64_t nanos) {
  return nanos_to_string(nanos, "%Y-%m-%d %H:%M:%S.%f");
}

// returns integer according to format
int64_t nanos_from_string(const std::string& str, std::string format) {
  static const int64_t ns_per_sec = 1000000000;
  tm time;
  memset(&time, 0, sizeof(tm));
  int nanos = 0;
  char* ret = ::strptime_ns(str.c_str(), format.c_str(), &time, &nanos);
  if (ret == nullptr) {
    return std::numeric_limits<int64_t>::max();
  }
  if (time.tm_year == 0) {
    time.tm_year = 70;
  }
  if (time.tm_mday == 0) {
    time.tm_mday = 1;
  }
  time_t clock = fast_timegm(&time);
  int64_t unix_time = (clock * ns_per_sec) + int64_t(nanos);
  return unix_time;
}

// returns integer given a string to nanoseconds
int64_t nanos_from_string(const std::string& str) {
  std::string format = infer_strtime_format(str);
  if (!is_inferred_timestamp(format)) {
    return std::numeric_limits<int64_t>::max();
  }
  return nanos_from_string(str, format);
}

// returns delta string formatted to nanoseconds
std::string delta_to_string(int64_t delta) {
  static const int64_t ns_per_day = 86400000000000;
  int64_t sign = delta < 0 ? -1 : 1;
  std::string str = sign == -1 ? "-" : "";
  int64_t full_days = (sign * delta) / ns_per_day;
  if (full_days != 0) {
    str += std::to_string(full_days) + " days";
  }
  int64_t sub_days = (sign * delta) % ns_per_day;
  if (sub_days != 0) {
    if (full_days != 0) {
      str += ' ';
    }
    str += nanos_to_string(sub_days, "%H:%M:%S.%f");
  }
  return str;
}

// returns delta integer given a string to nanoseconds
int64_t delta_from_string(const std::string& str) {
  static const int64_t ns_per_day = 86400000000000;
  int64_t num = 0;
  int64_t sign = 1;
  size_t start = 0;
  if (str[0] == '-') {
    sign = -1;
    start = 1;
  }
  std::string day_str = " day";
  size_t day_start = str.find(day_str, start);
  if (day_start != std::string::npos) {
    num = std::stol(str.substr(start, day_start)) * ns_per_day;
    start = day_start + day_str.size();
    if (start < str.size() && str[start] == 's') {
      start++;
    }
  }
  while (start < str.size() && str[start] == ' ') {
    start++;
  }
  if (start < str.size()) {
    std::string sub_day = str.substr(start);
    std::string format = infer_strtime_format(sub_day);
    if (!is_inferred_time(format)) {
      return std::numeric_limits<int64_t>::max();
    }
    int64_t sub_day_value = nanos_from_string(sub_day, format);
    num += sub_day_value;
  }
  return sign * num;
}

}  // namespace VVM

