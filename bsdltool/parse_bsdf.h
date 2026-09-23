// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include "bsdfs.h"

#include <optional>
#include <string>
#include <string_view>

struct BsdfInstance;

// Parses ["(R,G,B)"] NAME(ARGUMENT, ...): an optional color weight in front
// of the BSDF name, white when absent. Returns the parsed instance, or
// nullopt with an explanation in 'error'.
std::optional<BsdfInstance>
parse_bsdf(std::string_view text, const BsdfRegistry& registry, std::string& error);
