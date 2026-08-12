// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include "BSDL/config.h"
#include "BSDL/config_pvt.h"
#include <BSDL/SPI/bsdf_toondiffuse_decl.h>

BSDL_ENTER_NAMESPACE

namespace spi
{

template <typename BSDF_ROOT, bool TR>
template <typename T>
BSDL_INLINE_METHOD
ToonDiffuseLobeGen<BSDF_ROOT, TR>::ToonDiffuseLobeGen(T*                 lobe,
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

    // Squared for better linear response
    // NOTE: flatness in [0, 1] -> K in [0, 1]. K=1 gives cosine diffuse,
    // K=0 gives a sharp toon edge. Clamped to 0.001 to avoid degenerate powers.
    K = CLAMP(SQR(1 - data.flatness), 0.001f, 1);
    // Clamp to 0.99 to prevent division-by-zero in `1/(1-shift)` downstream.
    // The theoretical max of 1.0 would push the terminator to the zenith.
    terminator_shift = std::min(data.terminator_shift, 0.99f);
    if (data.ramp) {
        Ramp<> tmp(0);
        tmp.load(data.ramp, true /* use color */, false);
        ramp.cosine_resample(tmp, true);
        // NOTE: K = -1 is the sentinel value indicating ramp mode is active.
        // This avoids adding a separate `bool use_ramp` member to keep the
        // layout of this class compact.
        K = -1;
    }
}

// Compress/expand the angular range of the cosine lobe without calling
// acos/sin. Target: map cos_theta -> cos(acos(cos_theta) * scale), where
// `scale` controls how much of the hemisphere the lobe covers. scale < 1
// compresses the lobe (sharper edge), scale > 1 expands it (wider lobe).
//
// Implementation: fit a quadratic f(t) = 1 - a*t - b*t^2 where t = 1 -
// cos_theta. Constraints:
//   1. f(0) = 1 (peak at zenith is preserved)
//   2. f'(0) / f(0) = scale^2 (match Taylor expansion slope at the peak)
//   3. f(1) = cos(pi/2 * scale) = sin(pi/2 * (1-scale)) (match the horizon
//   value)
//
// The horizon value uses the approximation sin(x) ~ x for small (1-scale),
// which is exact at scale=1 and off by < 2% for scale in [0.5, 2.0].
BSDL_INLINE float scale_cos_angle(float cos_theta, float scale)
{
    // We want to change the angle without using expensive trig functions.
    // Target: cos_theta = cos(acos(cos_theta) * scale)
    // We achieve this by fitting a quadratic curve f(t) to the target function.

    // Map the peak of the lobe (cos=1) to t=0, and the horizon (cos=0) to t=1.
    const float t = 1 - cos_theta;
    const float a = SQR(scale);
    // Match the exact value at the horizon (t=1).
    // Target is cos(pi/2 * scale) = sin(pi/2 * (1 - scale)).
    // For small shifts, sin(x) ~= x, yielding:
    const float f_1 = 0.5f * PI * (1 - scale); // Can be negative
    // Solve for the quadratic curve's tail.
    // Since f(1) = 1 - a - b, we enforce b = 1 - a - f(1) to match f_1
    const float b = 1.0f - a - f_1;

    // Evaluate the polynomial: 1 - a*t - b*t^2. This fit is really good, in fact
    // we could use a linear one. But the savings would be negligible.
    return 1 - t * (a + b * t);
}

template <typename BSDF_ROOT, bool TR>
BSDL_INLINE_METHOD Sample
ToonDiffuseLobeGen<BSDF_ROOT, TR>::eval_impl(const Imath::V3f& wo,
                                             const Imath::V3f& _wi) const
{
    constexpr auto powf = BSDLConfig::Fast::powf;
    // When translucent we mirror wi to the other side of the normal and perform
    // a regular diffuse BSDF.

    Imath::V3f wi = { _wi.x, _wi.y, TR ? -_wi.z : _wi.z };

    // Map the compressed angle back to the "effective" cosine for the lobe.
    const float cos_theta =
        scale_cos_angle(std::min(1.0f, wi.z), 1 / (1 - terminator_shift));
    if (cos_theta < FLOAT_MIN)
        return {};

    // K < 0 means ramp is active (K itself is clamped to [0.001, 1]).
    const bool useramp = K < 0;

    // re-normalization factor after terminator shift.
    // - Ramp mode: uses a squared normfix based on terminator_shift. Works in the
    //   general case when scaling the angle.
    // - Non-ramp mode: `p` approximates the optimal exponent for energy
    // preservation
    //   (fit derived numerically to maintain constant albedo as flatness
    //   changes).
    const float p = -0.2f * SQR(K) + 0.363f * K + 1.571f;
    const float normfix =
        useramp ? 1 / SQR(1 - terminator_shift) : powf(1 / (1 - terminator_shift), p);
    if (useramp) {
        auto [reval, rpdf] = ramp.eval(1 - cos_theta, true);
        if (rpdf < PDF_MIN)
            return {};
        const float      pdf    = normfix * rpdf / (2 * PI);
        const Imath::C3f weight = reval * (1 / rpdf); // 2 PI and normfix factors cancel
        // We don't expect this to work in spectral, otherwise we have to keep
        // lambda_0 for upsampling.
        return { _wi, Power(weight, 0), pdf, 1.0f };

    } else {
        // Generalized Toon diffuse: f = normfix * cos^K * (K+1) / (2*pi)
        // When K=1 this reduces to cosine diffuse f = 1/pi.
        const float D   = normfix * powf(cos_theta, K) * (1 + K) / (2 * PI);
        const float pdf = D;
        if (D > EPSILON)
            return { _wi, Power(D / pdf, 1), pdf, 1.0f };
    }
    return {};
}

template <typename BSDF_ROOT, bool TR>
BSDL_INLINE_METHOD Sample
ToonDiffuseLobeGen<BSDF_ROOT, TR>::sample_impl(const Imath::V3f& wo,
                                               const Imath::V3f  rnd) const
{
    constexpr auto powf    = BSDLConfig::Fast::powf;
    const bool     useramp = K < 0;
    const float    base_cos =
        useramp ? 1 - ramp.sample(rnd.x) : powf(rnd.x, 1.0f / (K + 1.0f));
    const float cos_theta = scale_cos_angle(base_cos, 1 - terminator_shift);
    const float sin_theta = sqrtf(1.0f - SQR(cos_theta));
    const float phi       = 2 * PI * rnd.y;
    float       sin_phi, cos_phi;
    BSDLConfig::Fast::sincosf(phi, &sin_phi, &cos_phi);
    Imath::V3f wi = { cos_phi * sin_theta, sin_phi * sin_theta, cos_theta };
    if (TR)
        wi.z = -wi.z;
    return eval_impl(wo, wi);
}

} // namespace spi

BSDL_LEAVE_NAMESPACE
