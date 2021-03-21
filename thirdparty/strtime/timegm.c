/*
 * Copyright (C) 2019--2021 Empirical Software Solutions, LLC
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

#define SECS_PER_DAY    86400
#define SECS_PER_HOUR    3600
#define SECS_PER_MINUTE    60

/* assumes a Julian calendar, so works from 1901 to 2099 */

time_t fast_timegm(struct tm *timeptr) {
  static const int month_days[] =
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

  return (total_days * SECS_PER_DAY) +
         (timeptr->tm_hour * SECS_PER_HOUR) +
         (timeptr->tm_min * SECS_PER_MINUTE) +
          timeptr->tm_sec;
}

/* division that rounds down for negatives */
static time_t div(time_t a, time_t b) {
  return (a / b - (a % b < 0));
}

static time_t leaps_passed(time_t year) {
  return div(year, 4) - div(year, 100) + div(year, 400);
}

static int is_leap(time_t year) {
  return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

void fast_gmtime(const time_t *clock, struct tm *timeptr) {
  static const time_t month_days[2][13] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
  };

  time_t time = *clock;

  time_t total_days = time / SECS_PER_DAY;
  time_t secs_in_day = time % SECS_PER_DAY;
  while (secs_in_day < 0) {
    secs_in_day += SECS_PER_DAY;
    total_days--;
  }

  timeptr->tm_hour = secs_in_day / SECS_PER_HOUR;
  time_t secs_in_hour = secs_in_day % SECS_PER_HOUR;

  timeptr->tm_min = secs_in_hour / SECS_PER_MINUTE;
  time_t secs_in_minute = secs_in_hour % SECS_PER_MINUTE;

  timeptr->tm_sec = secs_in_minute;

  /* iteratively guess the year and adjust */
  time_t year = 1970;
  time_t days_in_year = total_days;
  while (days_in_year < 0 || days_in_year >= (is_leap(year) ? 366 : 365)) {
    time_t guess = year + div(days_in_year, 365);
    days_in_year -= ((guess - year) * 365
                  + leaps_passed(guess - 1)
                  - leaps_passed(year - 1));
    year = guess;
  }
  timeptr->tm_year = year - 1900;

  /* determine month and day */
  const time_t *md = month_days[is_leap(year)];
  time_t month;
  for (month = 0; days_in_year >= md[month+1]; month++)
    ;
  timeptr->tm_mon = month;
  timeptr->tm_mday = 1 + days_in_year - md[month];
}

