/*
 * Routines for dealing with text (outside of ANTLR)
 *
 * Copyright (C) 2019--2020 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#pragma once

#include <stdexcept>
#include <string>
#include <vector>

// see if left string starts with right, starting at left's pos
bool starts_with(const std::string& left, const std::string& right,
                 size_t pos = 0) {
  return left.compare(pos, right.size(), right) == 0;
}

// see if left string ends with right
bool ends_with(const std::string& left, const std::string& right) {
  if (left.size() >= right.size()) {
    size_t pos = left.size() - right.size();
    return left.compare(pos, right.size(), right) == 0;
  }
  return false;
}

std::string trim(std::string str) {
  size_t start = str.find_first_not_of(" \t\n");
  size_t end = str.find_last_not_of(" \t\n");
  return str.substr(start, end - start + 1);
}

// contains pair of testable input and expected output
struct TestPair {
  std::string in;
  std::string out;
};
typedef std::vector<TestPair> Tests;

// parse Markdown file into inputs and expected outputs
Tests parse_markdown(const std::string& contents) {
  Tests tests;

  bool inticks = false;
  size_t line = 1;
  const std::string TICKS = "```";
  const std::string PROMPT = ">>> ";

  for (size_t i = 0; i < contents.size(); ) {
    if (starts_with(contents, TICKS, i)) {
      inticks = !inticks;
      while (i < contents.size() && contents[i++] != '\n')
        ;
      line++;
    }
    if (inticks) {
      if (starts_with(contents, PROMPT, i)) {
        // testable input
        std::string in;
        i += PROMPT.size();
        while (i < contents.size() && contents[i] != '\n') {
          in.push_back(contents[i++]);
        }
        in.push_back('\n');
        i++;
        line++;

        // expected output
        std::string out;
        while (i < contents.size() && !starts_with(contents, PROMPT, i) &&
               !starts_with(contents, TICKS, i)) {
          while (i < contents.size() && contents[i] != '\n') {
            out.push_back(contents[i++]);
          }
          out.push_back('\n');
          i++;
          line++;
        }

        // combined test
        tests.push_back({in, out});
      }
      else {
        std::ostringstream oss;
        oss << "Error: prompt expected on line " << line;
        throw std::logic_error(oss.str());
      }
    }
    else {
      // raw text is ignored
      while (i < contents.size() && !starts_with(contents, TICKS, i)) {
        while (i < contents.size() && contents[i++] != '\n')
          ;
        line++;
      }
    }
  }

  if (inticks) {
    throw std::logic_error("Error: file ended in a code segment");
  }

  return tests;
}

