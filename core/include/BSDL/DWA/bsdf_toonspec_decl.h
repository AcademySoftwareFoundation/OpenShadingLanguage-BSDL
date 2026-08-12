// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <BSDL/bsdf_decl.h>
#include <BSDL/ramp_decl.h>

BSDL_ENTER_NAMESPACE

namespace dwa
{

struct ToonFresnel {
    BSDL_INLINE_METHOD ToonFresnel(float eta): eta(CLAMP(eta, IOR_MIN, IOR_MAX)) {}
    BSDL_INLINE_METHOD
    float eval(const float c) const;

  private:
    static constexpr float IOR_MIN = 1.00000012f;
    static constexpr float IOR_MAX = 1000;
    float                  eta;
};

template <typename BSDF_ROOT> struct ToonSpecularLobe : public Lobe<BSDF_ROOT> {
    using Base = Lobe<BSDF_ROOT>;

    static constexpr LabelSet labels = { Label::GLOSSY,
                                         Label::FRONT,
                                         Label::SURFACE,
                                         Label::LAYER };

    struct Data : public LayeredData {
        Imath::V3f       N;
        Imath::V3f       U;
        float            IOR;
        float            fresnel_blend;
        float            constant_reflectance;
        float            layer_IOR;
        float            roughness;
        float            stretch_u;
        float            stretch_v;
        Imath::C3f       tint;
        float            indirect_scale;
        float            layer_scale;
        const RampInput* ramp; // Hidden input
        using lobe_type = ToonSpecularLobe<BSDF_ROOT>;
    };

    static constexpr const char* name() { return "dwa_toon_specular"; }

    template <typename D> static typename LobeRegistry<D>::Entry entry()
    {
        static_assert(std::is_base_of<Data, D>::value); // Make no other assumptions
        using R = LobeRegistry<D>;
        return { name(),
                 { R::param(&D::N),
                   R::param(&D::U),
                   R::param(&D::IOR),
                   R::param(&D::fresnel_blend),
                   R::param(&D::constant_reflectance),
                   R::param(&D::layer_IOR),
                   R::param(&D::roughness),
                   R::param(&D::stretch_u),
                   R::param(&D::stretch_v),
                   R::param(&D::tint),
                   R::param(&D::indirect_scale),
                   R::param(&D::layer_scale, "layer_scale"),
                   R::close() } };
    }

    template <typename T>
    BSDL_INLINE_METHOD ToonSpecularLobe(T*, const BsdfGlobals& globals, const Data& data);

    BSDL_INLINE_METHOD Sample eval_impl(const Imath::V3f& wo,
                                        const Imath::V3f& wi,
                                        bool              stretch = true) const;
    BSDL_INLINE_METHOD Sample sample_impl(const Imath::V3f& wo,
                                          const Imath::V3f& rnd) const;

    // Poor man's layering, instead of the usual albedo LUT, we
    // use fresnel on the macro normal
    BSDL_INLINE_METHOD Power albedo_impl() const { return tint; }
    BSDL_INLINE_METHOD Power filter_o(const Imath::V3f& wo) const
    {
        return Power(1 - layer_fresnel, 1);
    }

  protected:
    // Reflection axis to bounce and eval from
    BSDL_INLINE_METHOD Imath::V3f
    compute_n(const Imath::V3f& wo, const Imath::V3f& wi, bool stretch) const;

    Ramp<>      ramp;
    ToonFresnel fresnel;
    Power       tint;
    float       layer_fresnel;
    float       indirect_scale;
    float       fresnel_blend;
    float       constant_reflectance;
    float       stretch_u;
    float       stretch_v;
};

} // namespace dwa

BSDL_LEAVE_NAMESPACE
