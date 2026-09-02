#pragma once

#include "bsdfs.h"

#include <cstddef>
#include <string_view>
#include <vector>

struct Ray {
    Imath::V3f origin;
    Imath::V3f direction;
    Imath::C3f weight; // throughput
    float      tmin;
    float      tmax;
    float      outer_ior;
    float      path_roughness; // A camera ray starts with 0
    float      pdf;            // Optional, when coming from a sample
    // For sampling purposes and random numers: pixel, AA iteration and depth
    int x, y, sample_index, depth;
};

struct Scene {
    struct Light {
        Imath::V3f direction; // infinite
        float      angle; // Apperture in radians, PI would cover the whole unit sphere
        Imath::V3f color;
        float      intensity;
    };

    struct BsdfGlobals {
        Imath::V3f wo;
        Imath::V3f P;
        Imath::V3f N;
        Imath::V3f Nf;
        Imath::V3f Ngf;
        float      u;
        float      v;
        float      outer_ior;
        float      path_roughness;
        bool       backfacing;
        // propagated from Ray
        int x, y, sample_index, depth;
    };
    struct Hit {
        float t;
        // Hit primitive. A negative value denotes a ray miss.
        int obj;
    };

    virtual Hit         trace(const Ray& ray) const                          = 0;
    virtual BsdfGlobals globals_at_hit(const Ray& ray, const Hit& hit) const = 0;

    std::vector<Light> lights;
    bool               shadows       = true;
    bool               ground        = false;
    Imath::C3f         ground_color1 = { 0.2f, 0.2f, 0.2f };
    Imath::C3f         ground_color2 = { 0.8f, 0.8f, 0.8f };
    float              ground_scale  = 1.0f;
};

inline float cone_pdf(float angle)
{
    return 1.0f / (2.0f * bsdl::PI * (1.0f - std::cos(angle)));
}

struct BsdfGlobal {
    std::string_view name;
    bsdl::ParamType  type;
    std::size_t      offset;
    std::size_t      size;
};

struct BsdfAssignment {
    std::size_t parameter_index;
    std::size_t globals_offset;
    // A and B remap a float global x in [0, 1] to A + x * (B - A).
    // A == B == 0 means no remap.
    float float_lerp_A = 0;
    float float_lerp_B = 0;
};

// One BSDF as specified on the command line: its registry description, the
// parameter data, the assignments from shading globals to parameters and an
// optional color weight. render() takes a list of these and combines them
// with a GroupBsdf.
struct BsdfInstance {
    const BsdfDescription*      description = nullptr;
    BsdfDataStorage             data{};
    std::vector<BsdfAssignment> assignments;
    Imath::C3f                  weight = { 1, 1, 1 };
};

const BsdfGlobal* find_bsdf_global(std::string_view name);

struct SimpleSphere : public Scene {
    // The CSG difference of the unit sphere and a hardcoded spherical bite.
    Hit         trace(const Ray& ray) const override;
    BsdfGlobals globals_at_hit(const Ray& ray, const Hit& hit) const override;

    Imath::V3f bite_center = { 1.0f, 1.0f, 1.0f };
    float      bite_radius = 1.3f;
};

void render(const SimpleSphere&              scene,
            const std::vector<BsdfInstance>& instances,
            std::vector<Imath::C3f>&         image,
            int                              resolution,
            int                              samples,
            int                              depth,
            unsigned                         seed,
            unsigned                         threads = 0);