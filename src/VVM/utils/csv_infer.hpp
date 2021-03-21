/*
 * CSV Infer header -- declares routine to determine type from a file
 *
 * Copyright (C) 2019--2021 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#pragma once

#include <string>

namespace VVM {

std::string infer_table_from_file(const std::string& filename);

}  // namespace VVM

