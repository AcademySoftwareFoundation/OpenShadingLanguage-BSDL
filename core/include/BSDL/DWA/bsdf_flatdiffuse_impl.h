// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <BSDL/DWA/bsdf_flatdiffuse_decl.h>

BSDL_ENTER_NAMESPACE

namespace dwa
{

template <typename BSDF_ROOT, bool TR>
template <typename T>
BSDL_INLINE_METHOD
FlatDiffuseLobeGen<BSDF_ROOT, TR>::FlatDiffuseLobeGen(T*                 lobe,
                                                      const BsdfGlobals& globals,
                                                      const Data&        data):
    Base(lobe,
         globals.visible_normal(data.N),
         1.0f,
         globals.lambda_0,
         TR || data.terminator_shift < 0)
{
    Base::sample_filter =
        globals.get_sample_filter(Base::frame.Z, !(TR || data.terminator_shift < 0));
    A                = 1 - 0.235f * data.sigma;
    B                = data.sigma * A;
    flatness         = data.flatness;
    flatness_falloff = data.flatness_falloff;
    terminator_shift = data.terminator_shift;
}

// Note (Ole): wo and _wi are in local frame coordinates

template <typename BSDF_ROOT, bool TR>
BSDL_INLINE_METHOD Sample
FlatDiffuseLobeGen<BSDF_ROOT, TR>::eval_impl(const Imath::V3f& wo,
                                             const Imath::V3f& _wi) const
{
    // When translucent we mirror wi to the other side of the normal and perform
    // a regular oren-nayar BSDF.

    // Note(Ole): it looks like Frame.Z is in world space, but _wi is in the local
    // frame space? If they were in the space space, we could compute NL as
    // dot(N,wi), but that currently gives strange results.
    const Imath::V3f N  = Base::frame.Z;
    const Imath::V3f wi = { _wi.x, _wi.y, TR ? -_wi.z : _wi.z };

    // From Moonray:
    // When flatness approaches 1.0 it tends to produce an extremely harsh
    // terminator, which can lead to polygonal artifacts where self-shadowing
    // occurs.  This remapping attempts to apply a targeted softening effect at
    // the terminator - similar to the "shadow terminator fix" that we used to
    // soften bump mapping

    // Calculate the flatness falloff off by remapping
    // the angle between the light and the normal.
    // Desmos expression:
    // https://www.desmos.com/calculator/z1iufecqmf
    const float terminator_shift_p1 = terminator_shift + 1.0f;
    const float theta = terminator_shift_p1 * fast_acos(CLAMP(wi.z, -1.0f, 1.0f));
    if (theta > PI * 0.5f)
        return {};
    const float a = 1.0f - flatness_falloff;
    // The power of "a" controls the linearity of the falloff parameter.
    // Raising to the power of 4 slightly biases the control towards
    // the low end giving more control over a low falloff value.  The
    // constant 100.0 controls how sharply the falloff occurs when the
    // parameter is set to 1.0.
    const float b = 1.0f + a * a * a * a * 100.0f;
    const float c = AI_PIOVER2 / b;
    const float d = c - AI_PIOVER2;
    const float t = (theta > AI_PIOVER2 - c) ? fast_cos((theta + d) * b) : 1.0f;

    const float flatness_t = flatness * t;

    // Back in the Arnold world; we interpolate local frame N between local up
    // [0,0,1] and local wi.

    const Imath::V3f flat_local_n =
        Imath::V3f((1 - flatness_t) * Imath::V3f(0, 0, 1) + _wi * flatness_t).normalize();

    const float NL = wi.dot(flat_local_n);

    // Note (Ole): this matches DWA pretty well for skydome, and their quad/rect
    // light
    const float NV = wo.z;

    if (NL > 0 && NV > 0) {
        const float cos_amax = fast_cos(std::min(PI, PI * 0.5f / (1 + terminator_shift)));
        const float pdf      = 1 / (2 * PI * (1 - cos_amax));
        // Simplified math from: A tiny improvement of Oren-Nayar reflectance model
        // - Yasuhiro Fujii http://mimosa-pudica.net/improved-oren-nayar.html
        const float LV = wi.dot(wo);

        const float s     = LV - NL * NV;
        const float stinv = s > 0 ? s / MAX(NL, NV) : s;

        // Note(Ole): DWA (without this we are seeing our surface get brighter with
        // flatness increase compared to DWAs). This is an ad hoc factor that tries
        // to minimize energy loss while also minimizing the difference in
        // brightness compared with lambertian (i.e. flatnesss = 0). const float
        // normalizationFactor = scene_rdl2::math::lerp(1.0f,
        //                                             0.75f,
        //                                             scene_rdl2::math::bias(mFlatness,
        //                                             0.75f));
        const float normalizationFactor =
            LERP(bias_curve01(flatness, 0.75f), 1.0f, 0.75f) / (1 - cos_amax);

        const float out = NL * ONEOVERPI * MAX(A + B * stinv, 0.0f) * normalizationFactor;

        return { _wi, Power(out / pdf, 1), pdf, 1.0f };
    }
    return {};
}

template <typename BSDF_ROOT, bool TR>
BSDL_INLINE_METHOD Sample
FlatDiffuseLobeGen<BSDF_ROOT, TR>::sample_impl(const Imath::V3f& wo,
                                               const Imath::V3f  rnd) const
{
    const float cos_amax  = fast_cos(std::min(PI, PI * 0.5f / (1 + terminator_shift)));
    const float cos_theta = LERP(rnd.x, 1.0f, cos_amax);
    const float sin_theta = sqrtf(1 - SQR(cos_theta));
    const float phi       = 2 * PI * rnd.y;
    float       sin_phi, cos_phi;
    BSDLConfig::Fast::sincosf(phi, &sin_phi, &cos_phi);
    Imath::V3f wi = { cos_phi * sin_theta, sin_phi * sin_theta, cos_theta };
    if (TR)
        wi.z = -wi.z;
    return eval_impl(wo, wi);
}

} // namespace dwa

BSDL_LEAVE_NAMESPACE
