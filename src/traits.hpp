/*
 * Traits -- tracks the kind of computation
 *
 * Copyright (C) 2019--2021 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#pragma once

// traits are intended for a bitmap
enum class SingleTrait: size_t {
  kNone = 0,
  kPure = 1,
  kTransform = 2,
  kLinear = 4,
  kAutostream = 8
};

// by convention, a set of Traits is always of type size_t
typedef size_t Traits;

// common collection of Traits
const Traits empty_traits = Traits(SingleTrait::kNone);
const Traits all_traits = Traits(SingleTrait::kPure)
                        | Traits(SingleTrait::kTransform)
                        | Traits(SingleTrait::kLinear);
const Traits io_traits = Traits(SingleTrait::kTransform)
                       | Traits(SingleTrait::kLinear);
const Traits ca_traits = Traits(SingleTrait::kPure)
                       | Traits(SingleTrait::kLinear);
const Traits ra_traits = Traits(SingleTrait::kPure)
                       | Traits(SingleTrait::kTransform);

