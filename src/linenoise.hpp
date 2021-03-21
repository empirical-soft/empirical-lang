/*
 * Linenoise -- C++ wrapper for antirez's linenoise
 *
 * Copyright (C) 2019--2021 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#pragma once

#include <string>

#include <sysconfig.hpp>

#ifndef WIN32
#include <libgen.h>
#include <linenoise/linenoise.h>
#endif  // WIN32

// wrapper class for linenoise (disabled on Windows)
class Linenoise {
  std::string history_filename_;
  std::string prompt_;

  bool disable_;

 public:

  // regular constructor doesn't load a history file
  Linenoise() {
#ifndef WIN32
    disable_ = false;
#else  // WIN32
    disable_ = true;
#endif  // WIN32
  }

  // constructor with argv[0] loads history file
  Linenoise(char* argv0) {
#ifndef WIN32
    std::string command_name = basename(argv0);

    std::string homedir = getenv("HOME");
    if (homedir.empty()) {
      homedir = ".";
    }

    history_filename_ = homedir + "/." + command_name + "_history";
    linenoiseHistoryLoad(history_filename_.c_str());

    disable_ = false;
#else  // WIN32
    disable_ = true;
#endif  // WIN32
    prompt_ = ">>> ";
  }

  // destructor saves history if argv[0] was provided
  ~Linenoise() {
#ifndef WIN32
    if (!history_filename_.empty()) {
      linenoiseHistorySave(history_filename_.c_str());
    }
#endif  // WIN32
  }

  // get the user's line and return whether the line is valid
  bool get_line(std::string& line_out) {
    if (disable_) {
      std::cout << prompt_;
      return bool(std::getline(std::cin, line_out));
    }

#ifndef WIN32
    char* raw_line = linenoise(prompt_.c_str());
    if (raw_line == nullptr) {
      return false;
    }

    line_out = raw_line;
    free(raw_line);
    linenoiseHistoryAdd(line_out.c_str());

    return true;
#endif  // WIN32
  }

  // change the prompt
  void set_prompt(const std::string& prompt) {
    prompt_ = prompt;
  }

  // useful for debugging, because LLDB doesn't work with Linenoise
  void disable() {
    disable_ = true;
  }

  // TODO need linenoiseSetCompletionCallback()
};

