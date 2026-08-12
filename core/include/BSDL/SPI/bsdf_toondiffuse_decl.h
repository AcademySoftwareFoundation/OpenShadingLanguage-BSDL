// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <BSDL/bsdf_decl.h>
#include <BSDL/ramp_decl.h>

BSDL_ENTER_NAMESPACE

namespace spi
{

template <typename BSDF_ROOT, bool TR = false>
struct ToonDiffuseLobeGen : public Lobe<BSDF_ROOT> {
    using Base = Lobe<BSDF_ROOT>;

    static constexpr LabelSet labels = { TR ? Label::BACK : Label::FRONT,
                                         Label::DIFFUSE,
                                         Label::SURFACE };

    struct Data {
        Imath::V3f       N;
        float            flatness;
        float            terminator_shift;
        const RampInput* ramp; // Hidden input
        using lobe_type = ToonDiffuseLobeGen<BSDF_ROOT, TR>;
    };
    template <typename D> static typename LobeRegistry<D>::Entry entry()
    {
        static_assert(std::is_base_of<Data, D>::value); // Make no other assumptions
        using R = LobeRegistry<D>;
        return { name(),
                 { R::param(&D::N),
                   R::param(&D::flatness),
                   R::param(&D::terminator_shift),
                   R::close() } };
    }

    template <typename T>
    BSDL_INLINE_METHOD
    ToonDiffuseLobeGen(T*, const BsdfGlobals& globals, const Data& data);

    static constexpr const char* name()
    {
        return TR ? "toon_translucent" : "toon_diffuse";
    }
    static constexpr const char* space() { return "spi"; }

    BSDL_INLINE_METHOD Sample eval_impl(const Imath::V3f& wo,
                                        const Imath::V3f& _wi) const;
    BSDL_INLINE_METHOD Sample sample_impl(const Imath::V3f& wo,
                                          const Imath::V3f  rnd) const;

  private:
    float  K;
    float  terminator_shift;
    Ramp<> ramp;
};

template <typename BSDF_ROOT>
using ToonDiffuseLobe = ToonDiffuseLobeGen<BSDF_ROOT, false>;
template <typename BSDF_ROOT>
using ToonTranslucentLobe = ToonDiffuseLobeGen<BSDF_ROOT, true>;

} // namespace spi

BSDL_LEAVE_NAMESPACE
