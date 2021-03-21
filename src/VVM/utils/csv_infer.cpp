/*
 * CSV Infer -- determine type from a file
 *
 * Copyright (C) 2019--2021 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#include <cctype>
#include <algorithm>
#include <string>
#include <exception>
#include <unordered_set>

#include <VVM/utils/timestamp.hpp>
#include <VVM/utils/conversion.hpp>
#include <VVM/utils/csv_infer.hpp>

#include <csvmonkey/csvmonkey.hpp>

namespace VVM {
// check whether all elements are blank
bool is_all_empty(const std::vector<std::string>& xs) {
  for (auto& x: xs) {
    if (!x.empty()) {
      return false;
    }
  }
  return true;
}

// check whether every element can be converted to an int64
bool is_int64(const std::vector<std::string>& xs) {
  for (auto& x: xs) {
    if (!x.empty()) {
      if (is_nil(from_string<int64_t>(x))) {
        return false;
      }
    }
  }
  return true;
}

// check whether every element can be converted to a double
bool is_float64(const std::vector<std::string>& xs) {
  for (auto& x: xs) {
    if (!x.empty()) {
      if (is_nil(from_string<double>(x))) {
        return false;
      }
    }
  }
  return true;
}

// check whether every element can be converted to a bool
bool is_bool(const std::vector<std::string>& xs) {
  for (auto& x: xs) {
    if (x != "true" && x != "false") {
      return false;
    }
  }
  return true;
}

// return strtime formats for all strings
std::vector<std::string> infer_all_strtime_formats(
  const std::vector<std::string>& xs) {
  std::vector<std::string> results;
  for (auto& x: xs) {
    if (!x.empty()) {
      results.push_back(infer_strtime_format(x));
    }
  }
  return results;
}

// check whether every format can represent a time
bool is_time(const std::vector<std::string>& formats) {
  for (auto& f: formats) {
    if (!is_inferred_time(f)) {
      return false;
    }
  }
  return true;
}

// check whether every format can represent a date
bool is_date(const std::vector<std::string>& formats) {
  for (auto& f: formats) {
    if (!is_inferred_date(f)) {
      return false;
    }
  }
  return true;
}

// check whether every format can represent a timestamp
bool is_timestamp(const std::vector<std::string>& formats) {
  for (auto& f: formats) {
    if (!is_inferred_timestamp(f)) {
      return false;
    }
  }
  return true;
}

// whether a character is invalid for a header
bool is_invalid_header_char(char c) {
  return (!std::isalnum(c) && c != '_');
}

// fix a header to be Empirical friendly
std::string fix_header(const std::string& header, size_t position,
                       std::unordered_set<std::string>& seen) {
  std::string h = header;

  // header must not be empty
  if (h.empty()) {
    std::ostringstream oss;
    oss << "unnamed_" << position;
    h = oss.str();
  }

  // only valid header names
  std::replace_if(h.begin(), h.end(), is_invalid_header_char, '_');
  std::transform(h.begin(), h.end(), h.begin(), ::tolower);

  // ensure the name is unique
  if (seen.find(h) != seen.end()) {
    size_t counter = 1;
    std::string attempt;
    do {
      std::ostringstream oss;
      oss << h << '_' << counter++;
      attempt = oss.str();
    } while (seen.find(attempt) != seen.end());
    h = attempt;
  }

  seen.insert(h);
  return h;
}

// return a string of the column's name and type
std::string infer_col(const std::string& header,
                      const std::vector<std::string>& xs,
                      size_t position,
                      std::unordered_set<std::string>& seen) {
  // Empirical identifiers are very particular
  std::string new_header = fix_header(header, position, seen);
  std::string ret = new_header + ": ";

  // try each converter to see what works
  if (is_all_empty(xs)) {
    ret += "String";
  } else if (is_int64(xs)) {
    ret += "Int64";
  } else if (is_float64(xs)) {
    ret += "Float64";
  } else if (is_bool(xs)) {
    ret += "Bool";
  } else {
    auto formats = infer_all_strtime_formats(xs);
    if (is_time(formats)) {
      ret += "Time";
    } else if (is_date(formats)) {
      ret += "Date";
    } else if (is_timestamp(formats)) {
      ret += "Timestamp";
    } else {
      ret += "String";
    }
  }

  return ret;
}

// return a string of the table's type definition
std::string infer_table_from_file(const std::string& filename) {
  // prepare reader
  csvmonkey::MappedFileCursor cursor;
  try {
    cursor.open(filename.c_str());
  }
  catch (csvmonkey::Error& e) {
    throw std::logic_error(e.what());
  }
  csvmonkey::CsvReader reader(cursor);

  // read and transpose table
  std::vector<std::string> headers;
  std::vector<std::vector<std::string>> columns;
  bool is_header = true;
  const size_t max_rows = 10;
  size_t nrows = 0;
  while (reader.read_row() && (nrows++ < max_rows)) {
    auto& row = reader.row();
    const size_t count = row.count;
    if (columns.size() < count) {
      headers.resize(count);
      columns.resize(count);
    }
    size_t col = 0;
    while (col < count) {
      if (is_header) {
        headers[col] = row.cells[col].as_str();
      } else {
        columns[col].push_back(row.cells[col].as_str());
      }
      col++;
    }
    is_header = false;
  }

  // infer each column
  std::unordered_set<std::string> seen;
  std::string ret = infer_col(headers[0], columns[0], 0, seen);
  for (size_t i = 1; i < columns.size(); i++) {
    ret += ", " + infer_col(headers[i], columns[i], i, seen);
  }

  return ret;
}
}  // namespace VVM

