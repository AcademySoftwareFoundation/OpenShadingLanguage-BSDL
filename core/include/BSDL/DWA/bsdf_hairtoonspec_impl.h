// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <BSDL/DWA/bsdf_toonspec_decl.h>
#include <BSDL/SPI/bsdf_physicalhair_impl.h>
#include <BSDL/ramp_impl.h>

BSDL_ENTER_NAMESPACE

namespace dwa
{

template <typename BSDF_ROOT>
template <typename T>
BSDL_INLINE_METHOD
HairToonSpecularLobe<BSDF_ROOT>::HairToonSpecularLobe(T*                 lobe,
                                                      const BsdfGlobals& globals,
                                                      const Data&        data):
    Base(lobe,
         globals,
         { { nullptr },
           data.T,
           data.IOR,
           data.offset,
           data.lroughness,
           data.aroughness,
           false,
           { 1, 1, 1 }, // white, then apply tint to the ramp
           {},
           1.0f,
           0.0f,
           data.h }),
    ramp(0),
    indirect_scale(data.indirect_scale)
{
    if (data.ramp) {
        ramp.load(data.ramp, true, true, data.tint);
    }
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD Sample
HairToonSpecularLobe<BSDF_ROOT>::eval_impl(const Imath::V3f& wo,
                                           const Imath::V3f& wi) const
{
    Sample      s   = Base::eval_impl(wo, wi);
    const float R   = s.weight[0] * s.pdf;
    const float rin = std::min(1.0f, R);
    auto [reval, rpdf] =
        ramp.size() ? ramp.eval(rin, false) : std::make_pair(Imath::C3f(R), 1.0f);
    // DWA applies 1 / pi here for some reason
    s.weight = Power(reval, 0) * (1 / s.pdf) * ONEOVERPI;
    return s;
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD Sample
HairToonSpecularLobe<BSDF_ROOT>::sample_impl(const Imath::V3f& wo,
                                             const Imath::V3f& rnd) const
{
    Sample s = Base::sample_impl(wo, rnd);
    return eval_impl(wo, s.wi);
}

} // namespace dwa

BSDL_LEAVE_NAMESPACE
