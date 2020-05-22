/*
 * Copyright (C) 2019--2020 Empirical Software Solutions, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <time.h>

/* assumes a Julian calendar, so works from 1901 to 2099 */

time_t fast_timegm(struct tm *timeptr) {
  static const int month_days [] =
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  int years = timeptr->tm_year - 70;
  int days_to_year = years * 365;

  int days_to_month = 0;
  for (int i = 0; i < timeptr->tm_mon; i++) {
    days_to_month += month_days[i];
  }

  int days_in_month = timeptr->tm_mday - 1;

  int leap_days = 0;
  if (years >= 0) {
    leap_days = (years + 2) / 4;
    /* if this is a leap year, but we haven't hit March yet */
    if (((years + 2) % 4) == 0 && (timeptr->tm_mon <= 1)) {
      leap_days--;
    }
  }
  else {
    leap_days = (years - 2) / 4;
    /* if this is a leap year, but we have passed March */
    if (((years - 2) % 4) == 0 && (timeptr->tm_mon > 1)) {
      leap_days++;
    }
  }

  int total_days = days_to_year + days_to_month + days_in_month + leap_days;

  return (total_days * 86400) + (timeptr->tm_hour * 3600) +
         (timeptr->tm_min * 60) + timeptr->tm_sec;
}

