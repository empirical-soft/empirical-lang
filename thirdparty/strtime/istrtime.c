/*
 * Copyright (C) 2019 Empirical Software Solutions, LLC
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

#include <ctype.h>
#include <stdlib.h>

enum TokenType {
  kWord,
  kNumber,
  kSpace,
  kOther
};

struct Token {
  enum TokenType token_type;
  /* where token occurs as indices in string [start, stop) */
  size_t start;
  size_t stop;
};


// tokenize a string; return number of tokens or 0 if over limit
static size_t _tokenize(const char *str, struct Token *tokens, size_t max_tok) {
  const char *s = str;
  size_t ti = 0;
  enum TokenType token_type;

  while (*s != '\0' && ti < max_tok) {
    size_t start = s - str;
    if (isalpha(*s)) {
      while (isalpha(*s))
        s++;
      token_type = kWord;
    }
    else if (isdigit(*s)) {
      while (isdigit(*s))
        s++;
      token_type = kNumber;
    }
    else if (isspace(*s)) {
      while (isspace(*s))
        s++;
      token_type = kSpace;
    }
    else {
      s++;
      token_type = kOther;
    }
    tokens[ti++] = (struct Token) {token_type, start, s - str};
  }

  if (ti == max_tok) {
    return 0;
  }

  return ti;
}


// copy a string, starting at the pointer and up to a limit
static char *_emit(const char *str, char *pt, const char *ptlim) {
  while (pt < ptlim && (*pt = *str++) != '\0') {
    ++pt;
  }
  return pt;
}


// append string segment, starting at the point and up to a limit
static char *_add(const char *str, struct Token *tok, char *pt,
                  const char* ptlim) {
  const char *s = &str[tok->start], *end = &str[tok->stop];
  while (pt < ptlim && s < end) {
    *pt++ = *s++;
  }
  return pt;
}


// infer the format string
char* istrtime(const char *str, char *format_buffer, size_t maxlen) {
  char *fb = format_buffer, *fb_limit = format_buffer + maxlen;
  int found_year = 0, found_hour = 0;
  size_t ti, num_tokens;
  struct Token tokens[80];
  char emitted[80];

  num_tokens = _tokenize(str, tokens, 80);
  if (num_tokens == 0) {
    return NULL;
  }

  ti = 0;
  while (ti < num_tokens) {
    struct Token *tok = &tokens[ti];
    int len = tok->stop - tok->start;
    switch (tok->token_type) {
      case kWord: {
        fb = _add(str, tok, fb, fb_limit);
        emitted[ti] = '\0';
        break;
      }
      case kNumber: {
        long num = strtol(&str[tok->start], NULL, 10);
        emitted[ti] = '\0';
        if (len == 4 && !found_year) {
          if (num >= 1900 && num <= 2099) {                /* year */
            fb = _emit("%Y", fb, fb_limit);
            emitted[ti] = 'Y';
            found_year = 1;
          }
        }
        if (len == 2) {
          if (ti >= 2 &&
              (emitted[ti-1] == '-' || emitted[ti-1] == '/') &&
              emitted[ti-2] == 'Y') {
            if (num >= 1 && num <= 12) {                  /* month */
              fb = _emit("%m", fb, fb_limit);
              emitted[ti] = 'm';
            }
          }
          else if (ti >= 3 &&
                   emitted[ti-1] == emitted[ti-3] &&
                   emitted[ti-2] == 'm') {
            if (num >= 1 && num <= 31) {                  /* day */
              fb = _emit("%d", fb, fb_limit);
              emitted[ti] = 'd';
            }
          }
          else if ((ti == 0 || emitted[ti-1] != ':') &&
                   !found_hour) {
            if (num >= 0 && num <= 23) {                  /* hour */
              fb = _emit("%H", fb, fb_limit);
              emitted[ti] = 'H';
              found_hour = 1;
            }
          }
          else if (ti >= 2 && emitted[ti-1] == ':' &&
                   emitted[ti-2] == 'H') {
            if (num >= 0 && num <= 59) {                  /* minute */
              fb = _emit("%M", fb, fb_limit);
              emitted[ti] = 'M';
            }
          }
          else if (ti >= 2 && emitted[ti-1] == ':' &&
                   emitted[ti-2] == 'M') {
            if (num >= 0 && num <= 60) {                  /* second */
              fb = _emit("%S", fb, fb_limit);
              emitted[ti] = 'S';
            }
          }
        }
        if (emitted[ti] == '\0' && len <= 9) {
          if (ti >= 2 && emitted[ti-1] == '.' &&
              emitted[ti-2] == 'S') {                     /* subsecond */
            fb = _emit("%f", fb, fb_limit);
            emitted[ti] = 'f';
          }
        }
        if (emitted[ti] == '\0') {
          fb = _add(str, tok, fb, fb_limit);
        }
        break;
      }
      case kSpace: {
        fb = _add(str, tok, fb, fb_limit);
        emitted[ti] = ' ';
        break;
      }
      case kOther: {
        fb = _add(str, tok, fb, fb_limit);
        emitted[ti] = str[tok->start];
        break;
      }
    }

    if (fb == fb_limit) {
      return NULL;
    }

    ti++;
  }
  
  *fb = '\0';
  return format_buffer;
}

