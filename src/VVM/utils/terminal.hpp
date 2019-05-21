/*
 * Terminal -- get console size
 *
 * Copyright (C) 2019 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#pragma once

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN
#include <windows.h>
#else  // WIN32
#include <cstdlib>
#include <sys/ioctl.h>
#include <libgen.h>
#endif  // WIN32

/**
 * Returns the number of rows and columns in terminal
 */
void get_terminal_size(size_t& rows_out, size_t& cols_out) {
  // default settings
  rows_out = 25;
  cols_out = 80;

#ifdef WIN32
  // attempt to query console screen buffer via standard handles
  for (DWORD h = -10; h >= DWORD(-12); h--) {
    HANDLE handle = GetStdHandle(h);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(handle, &info) != 0) {
      rows_out = info.srWindow.Bottom - info.srWindow.Top + 1;
      cols_out = info.srWindow.Right - info.srWindow.Left + 1;
      return;
    }
  }
#else  // WIN32
  // attempt to query terminal via standard file descriptors
  for (int fd = 0; fd <= 2; fd++) {
    winsize sz;
    if (ioctl(fd, TIOCGWINSZ, &sz) == 0) {
      rows_out = sz.ws_row;
      cols_out = sz.ws_col;
      return;
    }
  }

  // try environment variables instead
  char* lines = getenv("LINES");
  char* columns = getenv("COLUMNS");
  if (lines != nullptr && columns != nullptr) {
    rows_out = atol(lines);
    cols_out = atol(columns);
  }
#endif  // WIN32
}

