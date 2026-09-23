// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#include <BSDL/config.h>

using BSDLConfig = bsdl::BSDLDefaultConfig;
#define BSDL_CONFIG

#include "bsdfs.h"
#include "parse_bsdf.h"
#include "png.h"
#include "tracer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

namespace
{

void print_usage(const char* program, const BsdfRegistry& registry)
{
    std::fprintf(
        stderr,
        "Usage: %s render --bsdf '[(R,G,B)] NAME(ARGUMENT, ...)'\n"
        "       [--bsdf ...] ... [options]\n"
        "       %s diff <reference.png> <result.png> [--threshold N] [--scale N] [-o "
        "FILE]\n"
        "Options: -o FILE, --output FILE --resolution N --samples N --depth N --seed N\n"
        "         --threads N\n"
        "         --exposure STOPS --noshadow --ground COLOR1 COLOR2 SCALE\n"
        "         -L X,Y,Z R,G,B ANGLE_DEGREES INTENSITY\n"
        "Available BSDFs:\n",
        program,
        program);
    std::vector<std::string> names;
    names.reserve(registry.size());
    for (const auto& [name, description] : registry)
        names.push_back(name);
    std::sort(names.begin(), names.end());
    for (const std::string& name : names)
        std::fprintf(stderr, "  %s\n", name.c_str());
}

bool parse_positive(std::string_view text, int& value)
{
    return parse_int(text, value) && value > 0;
}

bool parse_nonnegative(std::string_view text, int& value)
{
    return parse_int(text, value) && value >= 0;
}

bool parse_positive_float(std::string_view text, float& value)
{
    return parse_float(text, value) && value > 0;
}

bool parse_nonnegative_float(std::string_view text, float& value)
{
    return parse_float(text, value) && value >= 0;
}

// Convert a light specification (angle in degrees, integrated intensity)
// into the renderer's representation (unit direction, radians, intensity
// per unit solid angle). Both default lights and -L use this boundary.
Scene::Light prepare_light(Scene::Light light)
{
    constexpr float degrees_to_radians = 0.01745329251994329577f;
    light.angle *= degrees_to_radians;
    light.direction.normalize();
    light.intensity *= cone_pdf(light.angle);
    return light;
}

void add_default_lights(SimpleSphere& scene)
{
    scene.lights = { prepare_light({ { -1, 0.5f, 0 }, 45.0f, { 1, 0.7f, 0.4f }, 1.0f }),
                     prepare_light({ { 1, 1, -0.6f }, 5.0f, { 1, 0.98f, 0.92f }, 1.0f }),
                     prepare_light({ { 0, 1, 0 }, 90.0f, { 0.5f, 0.8f, 0.92f }, 0.7f }) };
}

} // namespace

int render_main(int argc, char* argv[])
{
    const auto registry = make_registry<BsdfLobes>();
    if (argc < 3 || std::string_view(argv[2]) == "--help") {
        print_usage(argv[0], registry);
        return argc < 3 ? 2 : 0;
    }

    std::string  output     = "bsdf.png";
    int          resolution = 512, samples = 64, depth = 1, seed = 1, threads = 0;
    float        exposure = 1;
    SimpleSphere scene;
    // Each --bsdf appends a layer below the previous ones. render() combines
    // the list with a GroupBsdf, weighting each layer by the product of the
    // filter_o() of all the layers on top of it.
    std::vector<BsdfInstance> instances;
    for (int i = 2; i < argc;) {
        const std::string_view option = argv[i++];
        if (option == "--bsdf" && i < argc) {
            if (instances.size() >= GROUP_BSDF_MAX) {
                std::fprintf(stderr,
                             "Invalid --bsdf definition: too many layers (max %d)\n",
                             GROUP_BSDF_MAX);
                return 2;
            }
            std::string error;
            if (auto instance = parse_bsdf(argv[i++], registry, error))
                instances.push_back(std::move(*instance));
            else {
                std::fprintf(stderr, "Invalid --bsdf definition: %s\n", error.c_str());
                return 2;
            }
        } else if ((option == "-o" || option == "--output") && i < argc) {
            output = argv[i++];
            continue;
        } else if (option == "--resolution" && i < argc &&
                   parse_positive(argv[i++], resolution))
            continue;
        else if (option == "--samples" && i < argc && parse_positive(argv[i++], samples))
            continue;
        else if (option == "--depth" && i < argc && parse_nonnegative(argv[i++], depth))
            continue;
        else if (option == "--seed" && i < argc && parse_positive(argv[i++], seed))
            continue;
        else if (option == "--threads" && i < argc && parse_positive(argv[i++], threads))
            continue;
        else if (option == "--exposure" && i < argc && parse_float(argv[i++], exposure))
            continue;
        else if (option == "--noshadow")
            scene.shadows = false;
        else if (option == "--ground" && i + 2 < argc) {
            if (!parse_vector(argv[i++], scene.ground_color1) ||
                !parse_vector(argv[i++], scene.ground_color2) ||
                !parse_positive_float(argv[i++], scene.ground_scale)) {
                std::fprintf(stderr, "Invalid --ground specification\n");
                return 2;
            }
            scene.ground = true;
        } else if (option == "-L" && i + 3 < argc) {
            SimpleSphere::Light light;
            if (!parse_vector(argv[i++], light.direction) ||
                !parse_vector(argv[i++], light.color) ||
                !parse_float(argv[i++], light.angle) ||
                !parse_float(argv[i++], light.intensity) ||
                light.direction.length2() == 0 || light.angle <= 0 || light.angle > 180 ||
                light.intensity < 0) {
                std::fprintf(stderr, "Invalid -L light specification\n");
                return 2;
            }
            scene.lights.push_back(prepare_light(light));
        } else {
            std::fprintf(stderr, "Invalid render option: %s\n", option.data());
            return 2;
        }
    }
    if (instances.empty()) {
        std::fprintf(stderr, "Missing --bsdf definition\n");
        return 2;
    }
    if (scene.lights.empty())
        add_default_lights(scene);

    std::vector<Imath::C3f> image;
    render(scene,
           instances,
           image,
           resolution,
           samples,
           depth,
           static_cast<unsigned>(seed),
           static_cast<unsigned>(threads));
    if (!write_png(output, image, resolution, resolution, exposure)) {
        std::fprintf(stderr, "Could not write PNG: %s\n", output.c_str());
        return 2;
    }
    return 0;
}

namespace
{

void print_diff_usage(const char* program)
{
    std::fprintf(stderr,
                 "Usage: %s diff <reference.png> <result.png> [--threshold N] "
                 "[--scale N] [-o FILE]\n",
                 program);
}

} // namespace

int diff_main(int argc, char* argv[])
{
    if (argc == 3 && std::string_view(argv[2]) == "--help") {
        print_diff_usage(argv[0]);
        return 0;
    }
    if (argc < 4) {
        print_diff_usage(argv[0]);
        return 2;
    }

    std::string output;
    float       threshold = 1e-2f;
    float       scale     = 10.0f;
    for (int i = 4; i < argc;) {
        const std::string_view option = argv[i++];
        if ((option == "-o" || option == "--output") && i < argc)
            output = argv[i++];
        else if (option == "--threshold" && i < argc &&
                 parse_nonnegative_float(argv[i++], threshold))
            continue;
        else if (option == "--scale" && i < argc &&
                 parse_positive_float(argv[i++], scale))
            continue;
        else {
            std::fprintf(stderr, "Invalid diff option: %s\n", option.data());
            return 2;
        }
    }

    std::vector<unsigned char> reference, result;
    int reference_width, reference_height, result_width, result_height;
    if (!read_png_rgb(argv[2], reference, reference_width, reference_height)) {
        std::fprintf(stderr, "Could not read reference PNG: %s\n", argv[2]);
        return 2;
    }
    if (!read_png_rgb(argv[3], result, result_width, result_height)) {
        std::fprintf(stderr, "Could not read result PNG: %s\n", argv[3]);
        return 2;
    }
    if (reference_width != result_width || reference_height != result_height) {
        std::fprintf(stderr,
                     "Image dimensions differ: %dx%d vs %dx%d\n",
                     reference_width,
                     reference_height,
                     result_width,
                     result_height);
        return 1;
    }

    double                     squared_error = 0;
    std::vector<unsigned char> difference;
    if (!output.empty())
        difference.resize(reference.size());
    for (std::size_t i = 0; i < reference.size(); ++i) {
        const float error = (float(result[i]) - float(reference[i])) / 255.0f;
        squared_error += error * error;
        if (!output.empty())
            difference[i] = static_cast<unsigned char>(
                std::lround(std::min(1.0f, std::abs(error) * scale) * 255.0f));
    }
    const float rmse = static_cast<float>(std::sqrt(squared_error / reference.size()));
    std::printf("RMSE: %.8f (threshold %.8f)\n", rmse, threshold);
    if (!output.empty() &&
        !write_png_rgb(output, difference, reference_width, reference_height)) {
        std::fprintf(stderr, "Could not write difference PNG: %s\n", output.c_str());
        return 2;
    }
    return rmse > threshold ? 1 : 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <render|diff> ...\n", argv[0]);
        return 2;
    }
    const std::string_view command = argv[1];
    try {
        if (command == "render")
            return render_main(argc, argv);
        if (command == "diff")
            return diff_main(argc, argv);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 2;
    }
    std::fprintf(stderr, "Unknown command: %s\n", argv[1]);
    return 2;
}
