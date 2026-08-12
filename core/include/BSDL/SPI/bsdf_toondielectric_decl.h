// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include "BSDL/config.h"
#include "BSDL/spectrum_decl.h"
#include <BSDL/bsdf_decl.h>
#include <BSDL/ramp_decl.h>

BSDL_ENTER_NAMESPACE

namespace spi
{

template <typename BSDF_ROOT> struct ToonDielectricLobe : public Lobe<BSDF_ROOT> {
    using Base = Lobe<BSDF_ROOT>;

    static constexpr LabelSet labels = { Label::GLOSSY,
                                         Label::FRONT,
                                         Label::BACK,
                                         Label::SURFACE,
                                         Label::LAYER };
    struct Data : public LayeredData {
        Imath::V3f       N;
        Imath::V3f       U;
        Imath::C3f       refl_0;
        Imath::C3f       refl_90;
        Imath::C3f       refr_tint;
        float            roughness_x;
        float            roughness_y;
        float            flatness;
        float            IOR;
        const RampInput* ramp; // Hidden input
        using lobe_type = ToonDielectricLobe<BSDF_ROOT>;
    };
    template <typename D> static typename LobeRegistry<D>::Entry entry()
    {
        static_assert(std::is_base_of<Data, D>::value); // Make no other assumptions
        using R = LobeRegistry<D>;
        return { name(),
                 { R::param(&D::N),
                   R::param(&D::U),
                   R::param(&D::refl_0),
                   R::param(&D::refl_90),
                   R::param(&D::refr_tint),
                   R::param(&D::roughness_x),
                   R::param(&D::roughness_y),
                   R::param(&D::flatness),
                   R::param(&D::IOR),
                   R::close() } };
    }

    template <typename T>
    BSDL_INLINE_METHOD
    ToonDielectricLobe(T*, const BsdfGlobals& globals, const Data& data);

    static constexpr const char* name() { return "toon_dielectric"; }
    static constexpr const char* space() { return "spi"; }

    BSDL_INLINE_METHOD Sample eval_impl(const Imath::V3f& wo, const Imath::V3f& wi) const;
    BSDL_INLINE_METHOD Sample sample_impl(const Imath::V3f& wo,
                                          const Imath::V3f& rnd) const;

    BSDL_INLINE_METHOD Power filter_o(const Imath::V3f& wo) const
    {
        return dorefr ? Power::ZERO() : (Power::UNIT() - albedo_impl());
    }

    BSDL_INLINE_METHOD Power albedo_impl() const
    {
        return dorefr ? Power::UNIT() : get_fresnel(cosNO).clamped(0, 1);
    }

  protected:
    BSDL_INLINE_METHOD Power get_fresnel(float c) const;

    // This represents the builtin hat lobe:
    //
    //       C / (1 + tan(\theta)^2p)
    //
    // where C is a normalization constant and p controls its flatness. We
    // make p = 1 / (1 - flatness^2), so flatness 0 p = 1 and the lobe turns
    // into a smooth cos(\theta)^2. Higher flatness goes to a steppy sigmoid.
    // This implementation assumes the lobe axis is Z.
    struct HatLobe {
        float p;
        float cos_T;
        float cos_B;
        float N_p;

        HatLobe() = default;
        BSDL_INLINE_METHOD HatLobe(float p);

        BSDL_INLINE_METHOD float D(const Imath::V3f& w) const;
        BSDL_INLINE_METHOD float sample_cos(float rnd) const;
        // Sampling PDF will be different to D (no perfect importance sampling)
        BSDL_INLINE_METHOD float pdf(const Imath::V3f& w) const;
        // When the lobe is clipped by the surface at some angle, this tells us
        // how much energy we lose so we can compensate.
        BSDL_INLINE_METHOD float energy_loss(float cutoff_cos) const;
        // Average lobe extent heuristic in tangent space
        BSDL_INLINE_METHOD float avg_tan() const { return 1 + 1 / p; }
        // Just for energy loss, not sampling
        BSDL_INLINE_METHOD float cdf(float cost) const;

      private:
        BSDL_INLINE_METHOD float proxy_area() const;
    };

    // Same thing for a custom user-defined ramp
    struct RampLobe {
        Ramp<> ramp;
        RampLobe() = default;
        BSDL_INLINE_METHOD RampLobe(const RampInput* input);

        BSDL_INLINE_METHOD float D(const Imath::V3f& w) const;
        BSDL_INLINE_METHOD float cdf(float cost) const;
        BSDL_INLINE_METHOD float sample_cos(float rnd) const;
        // Since we have free roughness stretching, it's reasonable to ask
        // artists to keep 45 degree averaged spread on the base ramps.
        BSDL_INLINE_METHOD float avg_tan() const { return 1; };
    };

    // Integrate density over the spherical cap (pseudo circle) but clipped
    // at offset (cosine) from the center by the surface plane.
    template <typename F>
    BSDL_INLINE_METHOD static float truncated_cone_sum(F cdf, float cosine_clip);

    Power refl_0;
    Power refl_90;
    Power refr_tint;
    union {
        HatLobe  hat;
        RampLobe ramp;
    } impl;
    float refl_prob;
    float cosNO;
    float IOR;
    float a_x;
    float a_y;
    bool  dorefl;
    bool  dorefr;
    bool  TIR;
    bool  isramp;
};

} // namespace spi

BSDL_LEAVE_NAMESPACE
