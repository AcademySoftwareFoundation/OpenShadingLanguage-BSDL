// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <BSDL/DWA/bsdf_hairdiffuse_decl.h>

BSDL_ENTER_NAMESPACE

namespace dwa
{

template <typename BSDF_ROOT>
template <typename T>
BSDL_INLINE_METHOD HairDiffuseLobe<BSDF_ROOT>::HairDiffuseLobe(T*                 lobe,
                                                               const BsdfGlobals& globals,
                                                               const Data&        data):
    Base(lobe, data.T, globals.wo, 1.0f, globals.lambda_0, true),
    tint_front(globals.wave(data.tint_front)),
    tint_back(globals.wave(data.tint_back))
{
    Base::sample_filter = globals.get_sample_filter(Base::frame.Z, false);
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD Sample
HairDiffuseLobe<BSDF_ROOT>::eval_impl(const Imath::V3f& wo, const Imath::V3f& wi) const
{
    const Imath::V3f T          = { 0, 0, 1 };
    const Imath::V3f hd_cross_i = T.cross(wi);
    const Imath::V3f hd_cross_o = T.cross(wo);

    const float denominator =
        std::max(hd_cross_i.length() * hd_cross_o.length(), EPSILON);
    // Compute the proportion of transmission color vs. reflection color.
    const float kappa          = hd_cross_i.dot(hd_cross_o) / denominator;
    const float frontIntensity = (1.0f + kappa) / 2.0f;
    const float backIntensity  = 1.0f - frontIntensity;

    const Power tint = frontIntensity * tint_front + backIntensity * tint_back;

    const float cos_i = wi.z;
    const float irr   = sqrtf(std::max(1.0f - SQR(cos_i), 0.0f));
    const float bcsdf = SQR(ONEOVERPI) * irr;
    const float pdf   = 0.25f * ONEOVERPI;
    return { wi, tint * (bcsdf / pdf), pdf, 1.0f };
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD Sample
HairDiffuseLobe<BSDF_ROOT>::sample_impl(const Imath::V3f& wo, const Imath::V3f rnd) const
{
    float      randu = rnd.x;
    Imath::V3f wi;
    if (randu < 0.5) {
        randu        = Sample::stretch(randu, 0, 0.5f);
        Imath::V3f v = sample_uniform_hemisphere(randu, rnd.y);
        wi           = { v.x, v.y, -v.z };
    } else {
        randu        = Sample::stretch(randu, 0.5f, 0.5f);
        Imath::V3f v = sample_uniform_hemisphere(randu, rnd.y);
        wi           = { v.x, v.y, v.z };
    }
    return eval_impl(wo, wi);
}

} // namespace dwa

BSDL_LEAVE_NAMESPACE
