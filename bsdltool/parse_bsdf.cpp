// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#include <BSDL/config.h>

using BSDLConfig = bsdl::BSDLDefaultConfig;
#define BSDL_CONFIG

#include "parse_bsdf.h"
#include "tracer.h"

#include <cctype>
#include <vector>

namespace
{

// Defined in bsdfs.h; duplicate here so parse_bsdf.cpp can compile standalone.
std::string_view trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    return text;
}

bool split_arguments(std::string_view               text,
                     std::vector<std::string_view>& arguments,
                     std::string&                   error)
{
    if (trim(text).empty())
        return true;

    int         depth = 0;
    std::size_t begin = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        switch (text[i]) {
            case '(': ++depth; break;
            case ')':
                if (--depth < 0) {
                    error = "unexpected ')' in BSDF arguments";
                    return false;
                }
                break;
            case ',':
                if (depth == 0) {
                    const std::string_view argument = trim(text.substr(begin, i - begin));
                    if (argument.empty()) {
                        error = "empty BSDF argument";
                        return false;
                    }
                    arguments.push_back(argument);
                    begin = i + 1;
                }
                break;
        }
    }
    if (depth != 0) {
        error = "unclosed '(' in BSDF arguments";
        return false;
    }

    const std::string_view argument = trim(text.substr(begin));
    if (argument.empty()) {
        error = "empty BSDF argument";
        return false;
    }
    arguments.push_back(argument);
    return true;
}

bool parse_literal(void* data, const bsdl::LobeParam& parameter, std::string_view text)
{
    text = trim(text);
    if (parameter.type == bsdl::ParamType::VECTOR ||
        parameter.type == bsdl::ParamType::COLOR) {
        if (text.size() < 2 || text.front() != '(' || text.back() != ')')
            return false;
        text = trim(text.substr(1, text.size() - 2));
    }
    return assign_parameter(data, parameter, text);
}

bool parse_global_assignment(std::string_view       text,
                             const BsdfDescription& description,
                             std::size_t            parameter_index,
                             BsdfAssignment&        assignment,
                             std::string&           error)
{
    const std::size_t      open   = text.find('[');
    const std::string_view name   = trim(text.substr(0, open));
    const BsdfGlobal*      global = find_bsdf_global(name);
    if (!global)
        return false;

    const bsdl::LobeParam& parameter =
        description.parameters[parameter_index].lobe_parameter;
    if (parameter.type != global->type || parameter.type_size != global->size) {
        error = "BSDF argument " + std::to_string(parameter_index) +
                " cannot use global '" + std::string(name) + "'";
        return false;
    }

    float lerp_A = 0;
    float lerp_B = 0;
    if (open != std::string_view::npos) {
        if (text.back() != ']') {
            error = "invalid remap range for global '" + std::string(name) + "'";
            return false;
        }
        const std::string_view range = text.substr(open + 1, text.size() - open - 2);
        const std::size_t      colon = range.find(':');
        if (parameter.type != bsdl::ParamType::FLOAT || colon == std::string_view::npos ||
            range.find(':', colon + 1) != std::string_view::npos ||
            !parse_float(trim(range.substr(0, colon)), lerp_A) ||
            !parse_float(trim(range.substr(colon + 1)), lerp_B)) {
            error =
                "global '" + std::string(name) + "' remap must be a float range [A:B]";
            return false;
        }
    }
    assignment = { parameter_index, global->offset, lerp_A, lerp_B };
    return true;
}

} // namespace

std::optional<BsdfInstance>
parse_bsdf(std::string_view text, const BsdfRegistry& registry, std::string& error)
{
    // A fresh instance is zero-initialized with white weight and no
    // assignments; omitted trailing parameters keep their zero value.
    BsdfInstance instance;
    text = trim(text);
    // Optional color weight in front of the name: "(R,G,B) NAME(...)".
    if (!text.empty() && text.front() == '(') {
        int         depth = 0;
        std::size_t close = std::string_view::npos;
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '(')
                ++depth;
            else if (text[i] == ')' && --depth == 0) {
                close = i;
                break;
            }
        }
        if (close == std::string_view::npos) {
            error = "unclosed '(' in BSDF weight";
            return std::nullopt;
        }
        Imath::V3f rgb;
        if (!parse_vector(text.substr(1, close - 1), rgb)) {
            error = "invalid BSDF weight: expected (R,G,B)";
            return std::nullopt;
        }
        instance.weight = Imath::C3f(rgb.x, rgb.y, rgb.z);
        text            = trim(text.substr(close + 1));
    }

    const std::size_t open = text.find('(');
    if (open == std::string_view::npos || text.empty() || text.back() != ')') {
        error = "expected NAME(ARGUMENT, ...)";
        return std::nullopt;
    }

    const std::string_view name  = trim(text.substr(0, open));
    const auto             found = registry.find(std::string(name));
    if (name.empty() || found == registry.end()) {
        error = "unknown BSDF: " + std::string(name);
        return std::nullopt;
    }

    std::vector<std::string_view> arguments;
    if (!split_arguments(text.substr(open + 1, text.size() - open - 2), arguments, error))
        return std::nullopt;

    const BsdfDescription* description = &found->second;
    if (arguments.size() > description->parameters.size()) {
        error = description->name + " expects at most " +
                std::to_string(description->parameters.size()) + " arguments";
        return std::nullopt;
    }

    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const bsdl::LobeParam& parameter = description->parameters[i].lobe_parameter;
        BsdfAssignment         assignment;
        if (parse_global_assignment(arguments[i], *description, i, assignment, error)) {
            instance.assignments.push_back(assignment);
        } else if (!error.empty()) {
            return std::nullopt;
        } else if (!parse_literal(&instance.data, parameter, arguments[i])) {
            error = "invalid literal for BSDF argument " + std::to_string(i) +
                    " (parameter type " +
                    std::to_string(static_cast<int>(parameter.type)) + ")";
            return std::nullopt;
        }
    }
    instance.description = description;
    return instance;
}
