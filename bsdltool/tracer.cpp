// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#include "BSDL/tools.h"
#include <BSDL/config.h>

using BSDLConfig = bsdl::BSDLDefaultConfig;
#define BSDL_CONFIG

#include "rng.h"
#include "tracer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace
{

constexpr float PI      = 3.14159265358979323846f;
constexpr float MIN_PDF = 1e-4f;

Imath::V3f sample_cone(const Imath::V3f& direction, float angle, const Imath::V3f& random)
{
    const float       cos_theta = 1.0f - random.x * (1.0f - std::cos(angle));
    const float       sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));
    const float       phi       = 2.0f * PI * random.y;
    const bsdl::Frame frame(direction);
    return frame.world(
        { std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, cos_theta });
}

float power_heuristic(float a, float b)
{
    const float a2 = a * a;
    const float b2 = b * b;
    return a2 / (a2 + b2);
}

Imath::C3f
environment(const SimpleSphere& scene, const Imath::V3f& direction, float bsdf_pdf)
{
    Imath::C3f result(0);
    for (const auto& light : scene.lights) {
        if (direction.dot(light.direction) >= std::cos(light.angle)) {
            float weight = 1;
            if (bsdf_pdf > 0)
                weight = power_heuristic(bsdf_pdf, cone_pdf(light.angle));
            result += weight * light.color * light.intensity;
        }
    }
    return result;
}

Imath::C3f ground_color(const SimpleSphere& scene, const Imath::V3f& position)
{
    const int x = static_cast<int>(std::floor(position.x * scene.ground_scale));
    const int z = static_cast<int>(std::floor(position.z * scene.ground_scale));
    return ((x + z) & 1) ? scene.ground_color1 : scene.ground_color2;
}

// Build the lobe for one BSDF instance: copy its parameter data, apply the
// assignments from shading globals on top (with optional [A, B] float remap)
// and construct the lobe in 'storage'.
Bsdf* build_bsdf(BsdfStorage&                storage,
                 const BsdfInstance&         instance,
                 const Scene::ShaderGlobals& globals)
{
    const BsdfDescription& description = *instance.description;
    BsdfDataStorage        shaded_data;
    std::memcpy(&shaded_data, &instance.data, description.data_size);
    for (const BsdfAssignment& assignment : instance.assignments) {
        const bsdl::LobeParam& parameter =
            description.parameters[assignment.parameter_index].lobe_parameter;
        std::memcpy(static_cast<char*>(static_cast<void*>(&shaded_data)) +
                        parameter.offset,
                    static_cast<const char*>(static_cast<const void*>(&globals)) +
                        assignment.globals_offset,
                    parameter.type_size);
        if (assignment.float_lerp_A != 0 || assignment.float_lerp_B != 0) {
            float& value = *reinterpret_cast<float*>(
                static_cast<char*>(static_cast<void*>(&shaded_data)) + parameter.offset);
            value = assignment.float_lerp_A +
                    value * (assignment.float_lerp_B - assignment.float_lerp_A);
        }
    }
    return description.create_lobe(&storage,
                                   &shaded_data,
                                   globals.wo,
                                   globals.Nf,
                                   globals.Ngf,
                                   globals.backfacing,
                                   globals.path_roughness,
                                   globals.outer_ior,
                                   0);
}

// Build the BSDF used to shade a non-ground hit: one lobe per instance
// combined in a layered 'over' group. Children are constructed in
// storage[0..count-1] and the group in storage[count]; the returned pointer
// (and the group's child pointers) all live in 'storage'.
Bsdf* shade(BsdfStorage (&storage)[GROUP_BSDF_MAX + 1],
            const std::vector<BsdfInstance>& instances,
            const Scene::ShaderGlobals&      globals)
{
    const int count = std::min<int>(instances.size(), GROUP_BSDF_MAX);
    Bsdf*     children[GROUP_BSDF_MAX];
    // Layered 'over' combination: each BSDF is weighted by the
    // product of the filters of all the BSDFs on top of it.
    bsdl::Power filter = bsdl::Power::UNIT();
    for (int i = 0; i < count; ++i) {
        children[i] = build_bsdf(storage[i], instances[i], globals);
        // The instance weight composes with the layer filter.
        children[i]->set_weight(instances[i].weight * filter.toRGB(0));
        filter *= children[i]->filter_o(globals.wo);
    }
    return new (&storage[count]) GroupBsdf<GROUP_BSDF_MAX>(children, count);
}

// Accumulate each light directly into radiance to preserve summation order
// across path vertices. All lights share the same random sample, as before.
void direct_lighting(const SimpleSphere&         scene,
                     const Bsdf&                 bsdf,
                     const Scene::ShaderGlobals& globals,
                     const Ray&                  ray,
                     const Imath::V3f&           light_random,
                     Imath::C3f&                 radiance)
{
    for (const auto& light : scene.lights) {
        const Imath::V3f   wi = sample_cone(light.direction, light.angle, light_random);
        const bsdl::Sample sample       = bsdf.eval(globals.wo, wi);
        const float        light_pdf    = cone_pdf(light.angle);
        const float        light_weight = light.intensity / light_pdf;
        if (sample.pdf <= MIN_PDF || light_pdf <= MIN_PDF)
            continue;
        const Ray  shadow_ray = { globals.P + wi * 1e-4f,
                                  wi,
                                  { 1, 1, 1 },
                                  1e-4f,
                                  1e30f,
                                  globals.outer_ior,
                                  globals.path_roughness,
                                  light_pdf,
                                  ray.x,
                                  ray.y,
                                  ray.sample_index,
                                  ray.depth };
        const bool visible    = !scene.shadows || scene.trace(shadow_ray).obj < 0;
        if (visible && sample.pdf > 0)
            radiance += ray.weight * sample.weight.toRGB(0) * sample.pdf *
                        power_heuristic(light_pdf, sample.pdf) * light.color *
                        light_weight;
    }
}

Imath::C3f trace(const SimpleSphere&              scene,
                 const std::vector<BsdfInstance>& instances,
                 Ray                              ray,
                 int                              max_depth,
                 unsigned                         seed)
{
    Imath::C3f radiance(0);
    float      path_roughness = 0;
    for (int depth = 0; depth <= max_depth; ++depth) {
        Rng light_rng(seed, ray.x, ray.y, ray.sample_index, depth);
        Rng bsdf_rng(seed ^ 0x9e3779b9u, ray.x, ray.y, ray.sample_index, depth);
        const Scene::Hit hit = scene.trace(ray);
        if (hit.obj < 0) {
            radiance +=
                ray.weight * environment(scene, ray.direction, depth == 0 ? 0 : ray.pdf);
            break;
        }

        Scene::ShaderGlobals globals = scene.globals_at_hit(ray, hit);
        globals.path_roughness       = path_roughness;
        // One slot per child BSDF (or the ground lobe), plus one for the
        // group that combines them.
        BsdfStorage storage[GROUP_BSDF_MAX + 1];
        Bsdf*       bsdf;
        if (hit.obj == 2) {
            bsdl::spi::DiffuseLobe<Bsdf>::Data ground_data{ globals.Nf };
            bsdf = new (&storage[0]) bsdl::spi::DiffuseLobe<Bsdf>(
                reinterpret_cast<bsdl::spi::DiffuseLobe<Bsdf>*>(&storage[0]),
                bsdl::BsdfGlobals(globals.wo,
                                  globals.Nf,
                                  globals.Ngf,
                                  globals.backfacing,
                                  globals.path_roughness,
                                  globals.outer_ior,
                                  0),
                ground_data);
            bsdf->set_weight(ground_color(scene, globals.P));
        } else {
            bsdf = shade(storage, instances, globals);
        }

        direct_lighting(scene, *bsdf, globals, ray, light_rng.next(), radiance);

        const bsdl::Sample sample = bsdf->sample(globals.wo, bsdf_rng.next());
        const Imath::V3f   wi     = sample.wi;
        if (sample.pdf <= MIN_PDF)
            break;
        ray.origin    = globals.P + wi * 1e-4f;
        ray.direction = wi;
        ray.weight *= sample.weight.toRGB(0);
        ray.path_roughness = sample.roughness;
        ray.pdf            = sample.pdf;
        ray.depth          = depth + 1;
        if (!scene.shadows || scene.trace(ray).obj < 0) {
            radiance += ray.weight * environment(scene, ray.direction, ray.pdf);
            break;
        }
        // The camera hit is depth 0; max_depth counts indirect continuations.
        // Keep the BSDF-sampled environment contribution above even at the limit.
        if (depth == max_depth)
            break;
        if (depth >= 2) {
            const float survive =
                std::clamp(std::max(ray.weight.x, std::max(ray.weight.y, ray.weight.z)),
                           0.05f,
                           0.95f);
            if (bsdf_rng.next().x > survive)
                break;
            ray.weight /= survive;
        }
        path_roughness = std::max(path_roughness, sample.roughness);
    }
    return radiance;
}

} // namespace

Scene::Hit SimpleSphere::trace(const Ray& ray) const
{
    constexpr float epsilon = 1e-5f;
    Hit             nearest{ ray.tmax, -1 };
    const auto consider_sphere = [&](const Imath::V3f& center, float radius, int object) {
        const Imath::V3f origin       = ray.origin - center;
        const float      b            = origin.dot(ray.direction);
        const float      c            = origin.dot(origin) - radius * radius;
        const float      discriminant = b * b - c;
        if (discriminant < 0)
            return;

        const float root = std::sqrt(discriminant);
        for (const float t : { -b - root, -b + root }) {
            if (t <= ray.tmin || t >= nearest.t)
                continue;
            const Imath::V3f position = ray.origin + t * ray.direction;
            const bool       inside_bite =
                (position - bite_center).length2() < bite_radius * bite_radius - epsilon;
            const bool inside_outer = position.length2() < 1.0f - epsilon;
            if ((object == 0 && !inside_bite) || (object == 1 && inside_outer))
                nearest = { t, object };
        }
    };

    consider_sphere({ 0, 0, 0 }, 1.0f, 0);
    consider_sphere(bite_center, bite_radius, 1);
    if (ground && std::abs(ray.direction.y) > epsilon) {
        const float t = (-1.0f - ray.origin.y) / ray.direction.y;
        if (t > ray.tmin && t < nearest.t)
            nearest = { t, 2 };
    }
    return nearest;
}

const BsdfGlobal* find_bsdf_global(std::string_view name)
{
    using Globals                         = Scene::ShaderGlobals;
    static constexpr BsdfGlobal globals[] = {
        { "wo", bsdl::ParamType::VECTOR, offsetof(Globals, wo), sizeof(Globals::wo) },
        { "P", bsdl::ParamType::VECTOR, offsetof(Globals, P), sizeof(Globals::P) },
        { "N", bsdl::ParamType::VECTOR, offsetof(Globals, N), sizeof(Globals::N) },
        { "Nf", bsdl::ParamType::VECTOR, offsetof(Globals, Nf), sizeof(Globals::Nf) },
        { "Ngf", bsdl::ParamType::VECTOR, offsetof(Globals, Ngf), sizeof(Globals::Ngf) },
        { "u", bsdl::ParamType::FLOAT, offsetof(Globals, u), sizeof(Globals::u) },
        { "v", bsdl::ParamType::FLOAT, offsetof(Globals, v), sizeof(Globals::v) },
        { "outer_ior",
          bsdl::ParamType::FLOAT,
          offsetof(Globals, outer_ior),
          sizeof(Globals::outer_ior) },
        { "path_roughness",
          bsdl::ParamType::FLOAT,
          offsetof(Globals, path_roughness),
          sizeof(Globals::path_roughness) },
        { "x", bsdl::ParamType::INT, offsetof(Globals, x), sizeof(Globals::x) },
        { "y", bsdl::ParamType::INT, offsetof(Globals, y), sizeof(Globals::y) },
        { "sample_index",
          bsdl::ParamType::INT,
          offsetof(Globals, sample_index),
          sizeof(Globals::sample_index) },
        { "depth",
          bsdl::ParamType::INT,
          offsetof(Globals, depth),
          sizeof(Globals::depth) },
    };
    for (const BsdfGlobal& global : globals) {
        if (global.name == name)
            return &global;
    }
    return nullptr;
}

Scene::ShaderGlobals SimpleSphere::globals_at_hit(const Ray& ray, const Hit& hit) const
{
    const Imath::V3f position = ray.origin + hit.t * ray.direction;
    if (hit.obj == 2) {
        const Imath::V3f normal = { 0, 1, 0 };
        const Imath::V3f wo     = -ray.direction;
        return { wo,
                 position,
                 normal,
                 normal,
                 normal,
                 position.x,
                 position.z,
                 ray.outer_ior,
                 ray.path_roughness,
                 false,
                 ray.x,
                 ray.y,
                 ray.sample_index,
                 ray.depth };
    }
    const Imath::V3f spherical_position =
        hit.obj == 0 ? position.normalized() : (position - bite_center).normalized();
    Imath::V3f       normal     = hit.obj == 0 ? spherical_position : -spherical_position;
    const Imath::V3f wo         = -ray.direction;
    const bool       backfacing = normal.dot(wo) < 0;
    const Imath::V3f facing_normal   = backfacing ? -normal : normal;
    constexpr float  one_over_pi     = 0.31830988618379067154f;
    constexpr float  one_over_two_pi = 0.15915494309189533577f;
    const Imath::V3f uv_position =
        hit.obj == 0 ? spherical_position
                     : bsdl::Frame((-bite_center).normalized()).local(spherical_position);
    const float u = std::acos(std::clamp(uv_position.z, -1.0f, 1.0f)) * one_over_pi;
    // Gradient from left to right. Squared to compress the range to the left, because we mostly use
    // v for roughness.
    const float v = bsdl::pown<2>(bsdl::CLAMP(0.5f * (1 + spherical_position.x), 0, 1));
    return { wo,    position, normal,           facing_normal,      facing_normal,
             u,     v,        ray.outer_ior,    ray.path_roughness, backfacing,
             ray.x, ray.y,    ray.sample_index, ray.depth };
}

void render(const SimpleSphere&              scene,
            const std::vector<BsdfInstance>& instances,
            std::vector<Imath::C3f>&         image,
            int                              resolution,
            int                              samples,
            int                              depth,
            unsigned                         seed,
            unsigned                         threads)
{
    const std::size_t side = static_cast<std::size_t>(resolution);
    if (resolution <= 0 || side > image.max_size() / side)
        throw std::length_error("Invalid render image size");
    const std::size_t pixel_count = side * side;
    image.assign(pixel_count, Imath::C3f(0));
    const unsigned worker_count =
        threads == 0 ? std::max(1u, std::thread::hardware_concurrency()) : threads;
    std::vector<std::thread> workers;
    for (unsigned worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back([&, worker] {
            for (int y = static_cast<int>(worker); y < resolution; y += worker_count) {
                for (int x = 0; x < resolution; ++x) {
                    Imath::C3f color(0);
                    for (int sample = 0; sample < samples; ++sample) {
                        Rng              rng(seed, x, y, sample, -1);
                        const Imath::V3f jitter = rng.next();
                        const float      px =
                            (2.0f * (x + jitter.x) / resolution - 1.0f) * 1.2f;
                        const float py =
                            (1.0f - 2.0f * (y + jitter.y) / resolution) * 1.2f;
                        Ray ray = { { 0, 0, 5 }, Imath::V3f{ px, py, -5 }.normalized(),
                                    { 1, 1, 1 }, 1e-4f,
                                    1e30f,       1.0f,
                                    0,           1.0f,
                                    x,           y,
                                    sample,      0 };
                        color += trace(scene, instances, ray, depth, seed);
                    }
                    image[static_cast<std::size_t>(y) * side +
                          static_cast<std::size_t>(x)] = color / float(samples);
                }
            }
        });
    }
    for (auto& worker : workers)
        worker.join();
}
