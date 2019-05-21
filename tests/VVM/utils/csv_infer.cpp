/*
 * Tests for CSV inference
 *
 * Copyright (C) 2019 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#include "test.hpp"

#include <VVM/utils/csv_infer.hpp>

int main() {
  main_ret = 0;

  TEST(VVM::infer_table_from_file("../../sample_csv/prices.csv"),
       "symbol: String, date: Date, open: Float64, high: Float64, low: Float64, close: Float64, volume: Int64")

  TEST(VVM::infer_table_from_file("../../sample_csv/listings.csv"),
       "symbol: String, exch: String")

  TEST(VVM::infer_table_from_file("../../sample_csv/malformed.csv"),
       "date: Date, quant_equity: Float64, model: String, live_backtest: String, unnamed_4: String, unnamed_5: String, date_1: Date, quant_macro: Float64")

  return main_ret;
}

