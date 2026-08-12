// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <BSDL/SPI/bsdf_physicalhair_decl.h>
#include <BSDL/bsdf_decl.h>
#include <BSDL/ramp_decl.h>

BSDL_ENTER_NAMESPACE

namespace dwa
{

template <typename BSDF_ROOT>
struct HairToonSpecularLobe : public spi::HairSpecularLobe<BSDF_ROOT> {
    using Base = spi::HairSpecularLobe<BSDF_ROOT>;

    static constexpr LabelSet labels = { Label::GLOSSY,
                                         Label::FRONT,
                                         Label::BACK,
                                         Label::CURVE,
                                         Label::LAYER };

    struct Data : public LayeredData {
        Imath::V3f       T;
        float            IOR;
        float            offset;
        float            lroughness;
        float            aroughness;
        Imath::C3f       tint;
        float            indirect_scale;
        float            h;    // Offset in the curve
        const RampInput* ramp; // Hidden input
        using lobe_type = HairToonSpecularLobe<BSDF_ROOT>;
    };

    static constexpr const char* name() { return "dwa_hairtoon_specular"; }

    template <typename D> static typename LobeRegistry<D>::Entry entry()
    {
        static_assert(std::is_base_of<Data, D>::value); // Make no other assumptions
        using R = LobeRegistry<D>;
        return { name(),
                 { R::param(&D::T),
                   R::param(&D::IOR),
                   R::param(&D::offset),
                   R::param(&D::lroughness),
                   R::param(&D::aroughness),
                   R::param(&D::tint),
                   R::param(&D::indirect_scale),
                   R::close() } };
    }

    template <typename T>
    BSDL_INLINE_METHOD
    HairToonSpecularLobe(T*, const BsdfGlobals& globals, const Data& data);

    BSDL_INLINE_METHOD Sample eval_impl(const Imath::V3f& wo, const Imath::V3f& wi) const;
    BSDL_INLINE_METHOD Sample sample_impl(const Imath::V3f& wo,
                                          const Imath::V3f& rnd) const;

    // Poor man's layering, instead of the usual albedo LUT, we
    // use fresnel on the macro normal
    BSDL_INLINE_METHOD Power albedo_impl() const { return Base::albedo_impl(); }
    BSDL_INLINE_METHOD Power filter_o(const Imath::V3f& wo) const
    {
        return Base::filter_o(wo);
    }

  protected:
    Ramp<> ramp;
    float  indirect_scale;
};

} // namespace dwa

BSDL_LEAVE_NAMESPACE
