// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include "BSDL/bsdf_decl.h"
#include "BSDL/config.h"
#include "BSDL/spectrum_decl.h"
#include "BSDL/tools.h"
#include <BSDL/SPI/bsdf_toondielectric_decl.h>
#include <ImathVec.h>

BSDL_ENTER_NAMESPACE

namespace spi
{

template <typename BSDF_ROOT>
template <typename T>
BSDL_INLINE_METHOD
ToonDielectricLobe<BSDF_ROOT>::ToonDielectricLobe(T*                 lobe,
                                                  const BsdfGlobals& globals,
                                                  const Data&        data):
    Base(lobe,
         globals.visible_normal(data.N),
         data.U,
         0,
         globals.lambda_0,
         MAX_RGB(data.refr_tint) > 0),
    refl_0(globals.wave(data.refl_0).clamped(0, 1)),
    refl_90(globals.wave(data.refl_90).clamped(0, 1)),
    refr_tint(globals.wave(data.refr_tint)),
    IOR(globals.backfacing ? 1 / data.IOR : data.IOR),
    isramp(data.ramp)
{
    if (data.ramp)
        impl.ramp = RampLobe(data.ramp);
    else
        impl.hat = HatLobe(1 / CLAMP(SQR(1 - data.flatness), 0.01f, 1));
    a_x       = std::max(1e-3f, (globals.regularize_roughness(data.roughness_x)));
    a_y       = std::max(1e-3f, (globals.regularize_roughness(data.roughness_y)));
    cosNO     = Base::frame.Z.dot(globals.wo);
    TIR       = SQR(IOR) + SQR(cosNO) <= 1;
    refl_prob = TIR ? 1 : get_fresnel(cosNO).avg(globals.lambda_0);
    dorefl    = MAX_RGB(data.refl_0) > 0 || MAX_RGB(data.refl_90) > 0;
    dorefr    = refl_prob < 1 && MAX_RGB(data.refr_tint) > 0;
    if (!dorefr)
        refl_prob = 1;
    Base::sample_filter = globals.get_sample_filter(Base::frame.Z, !dorefr);
    Base::set_roughness(sqrtf(a_x * a_y));
    assert(cosNO >= 0);
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD ToonDielectricLobe<BSDF_ROOT>::HatLobe::HatLobe(float p): p(p)
{
    const float p_inv = 1.0 / p;
    // Analytically derived polynomial boundaries
    // We use polynomials in p_inv (1/p) to map the boundaries.
    // Derived from three mathematical constraints:
    //   1. Limit p->inf (p_inv=0): Both bounds collapse to the exact 45-degree
    //      inflection point (cos(45) = 1/sqrt(2) = 0.707106)
    //   2. Limit p=1 (p_inv=1): The lobe is perfectly soft, so the cubic spline
    //      claims the whole domain (z_T=1.0, z_B=0.0).
    //   3. Derivative at p_inv=0: Matched to the true density spread rate +/-
    //   0.45.
    // Solving this system yields the exact quadratic coefficients below.
    // Constraint 3 comes from the goldilocks zone theory for sigmoids. Allows us
    // to build the bounding box of a sigmoid.
    //
    // cos_T is the cosine offset where the top flat part drops from 1.0 (approx)
    // and cos_B is the cosine offset where the hat drops to almost 0. We use
    // those bounds to fit the CDF of the lobe.
    cos_T = CLAMP(0.707106f + 0.45f * p_inv - 0.157106f * SQR(p_inv), 1e-6f, 1 - 1e-6f);
    cos_B = CLAMP(0.707106f - 0.45f * p_inv - 0.257106f * SQR(p_inv), 1e-6f, 1 - 1e-6f);
    if (fabsf(cos_T - cos_B) < 1e-4f)
        cos_T = cos_B + 1e-4f; // Guarantee numerical stability at extreme p
    // Approximated total integral of the lobe \theta part
    N_p = 0.2928932f + 0.0404401f * p_inv;
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD float
ToonDielectricLobe<BSDF_ROOT>::HatLobe::D(const Imath::V3f& w) const
{
    constexpr auto powf  = BSDLConfig::Fast::powf;
    const float    utan2 = (1 - SQR(w.z)) / SQR(w.z);
    return 1 / ((1 + powf(utan2, p)) * (2 * PI * N_p));
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD float
ToonDielectricLobe<BSDF_ROOT>::HatLobe::sample_cos(float rnd) const
{
    constexpr auto powf = BSDLConfig::Fast::powf;

    // We use cos(\theta)^2 clipped to cos_B as a proxy
    return powf(1 - 3 * rnd * proxy_area(), 1 / 3.0f);
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD float
ToonDielectricLobe<BSDF_ROOT>::HatLobe::pdf(const Imath::V3f& w) const
{
    const float cost = std::max(0.0f, w.z);
    return cost < cos_B ? 0 : SQR(cost) / (2 * PI * proxy_area());
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD float ToonDielectricLobe<BSDF_ROOT>::HatLobe::proxy_area() const
{
    return (1 - pown<3>(cos_B)) * (1.0f / 3);
}

// This is a fit for the CDF of the lobe.
//  i.e. \int_0^{t} C sin(\theta) / (1 + tan(\theta)^2p)
// We did it numerically and then found a fit in the cosine space.
// NOTE: Don't try to use this for sampling, because:
//   1) The fit is good but its derived density is far from the target, yielding
//      high MC weights.
//   2) If you replace the target hat with this, the lobe is ugly.
//   3) Inverting this fit CDF is cumbersome.
//
// But it is very good for energy compensation.
//
template <typename BSDF_ROOT>
BSDL_INLINE_METHOD float ToonDielectricLobe<BSDF_ROOT>::HatLobe::cdf(float cost) const
{
    // Evaluates the C1-continuous Cubic Hermite Spline CDF.
    // Uses analytically derived polynomial bounds for z_T and z_B.
    if (cost >= 1)
        return 0;
    if (cost <= 0)
        return 1;

    if (cost >= cos_T) // Flat top plateau region
        return (1 - cost) / N_p;

    // Flat tail region
    if (cost <= cos_B)
        return 1.0;

    // C1 Cubic Spline transition region
    // The exact probability value where the curve meets the plateau
    float threshold_prob = (1 - cos_T) / N_p;

    // Normalize cost into u-space [0, 1]
    const float dz = cos_T - cos_B;
    float       u  = (cost - cos_B) / dz;
    float       u2 = pown<2>(u);
    float       u3 = pown<3>(u);

    // Hermite basis functions
    float h00 = 2 * u3 - 3 * u2 + 1;
    float h01 = -2 * u3 + 3 * u2;
    // float h10 = u3 - 2.0 * u2 + u; // Omitted: Multiplied by slope 0
    float h11 = u3 - u2;
    // Scaled derivative at cos_T: F'(cos_T) * dz
    // The true slope of the linear plateau is -1/N_p
    float M1 = -dz / N_p;
    // Evaluate the spline: F(u) = h00*P0 + h01*P1 + h10*M0 + h11*M1
    // Note: M0 (slope at z_B) is 0.0 because the tail is perfectly flat.
    float val = h00 + threshold_prob * h01 + M1 * h11;
    return CLAMP(val, 0.0f, 1.0f);
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD
ToonDielectricLobe<BSDF_ROOT>::RampLobe::RampLobe(const RampInput* input)
{
    Ramp<> tmp(0);
    tmp.load(input, false /* no color */, false);
    ramp.cosine_resample(tmp, true);
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD float
ToonDielectricLobe<BSDF_ROOT>::RampLobe::D(const Imath::V3f& w) const
{
    return ramp.eval(1 - w.z, true).second;
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD float
ToonDielectricLobe<BSDF_ROOT>::RampLobe::sample_cos(float rnd) const
{
    return 1 - ramp.sample(rnd);
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD float ToonDielectricLobe<BSDF_ROOT>::RampLobe::cdf(float cost) const
{
    return ramp.integrate(1 - cost) / ramp.integral();
}

// Integrate density over the spherical cap (pseudo circle) but clipped
// at offset (cosine) from the center by the surface plane.
template <typename BSDF_ROOT>
template <typename F>
BSDL_INLINE_METHOD float
ToonDielectricLobe<BSDF_ROOT>::truncated_cone_sum(F cdf, float cosine_clip)
{
    // Domain Warping: Map the cosine to a uniform [0, 1] domain using the CDF.
    // This flattens the arbitrary density distribution into a uniform unit disk,
    // allowing the integral to be solved as a purely geometric circular cap area.
    const float t = cdf(cosine_clip);
    // Then we compute a purely geometric integral. This works 100% correctly only
    // if the PDF is uniform, otherwise we suffer distortion. So the straight cut
    // becomes a curved "bite" in the disk. Nevertheless it is a good heuristic
    // when used for energy compensation.
    //
    // Unit hemi circle normalized integral stopping at t is
    //   (asin(t) + t sqrt(1 - t^2)) / 2
    // We fit it with this polynomial. 1 - (1 - t)^1.4 works even better
    // Coefficients derived from boundary matching
    const float a = 1.2732395f;
    const float b = 0.4535210f;
    const float c = -0.7267605f;
    // First half of the circle is unblocked, so .5 + .5 * ...
    return 0.5f + 0.5f * t * (a + t * (b + c * t));
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD Sample
ToonDielectricLobe<BSDF_ROOT>::eval_impl(const Imath::V3f& wo, const Imath::V3f& wi) const
{
    // R is the axis of the lobe. Either the reflected or refracted wo direction
    const Imath::V3f R =
        wi.z > 0 ? Imath::V3f{ -wo.x, -wo.y, wo.z } : refract(wo, { 0, 0, 1 }, IOR);
    // Now we build a frame on the axis with the aniso vector U, which is positive
    // X in the local BSDF frame. The frame f is nested in the BSDF one.
    const Frame f(R, { 1, 0, 0 });
    // Evaluate the lobe on w, local to the axis
    const Imath::V3f w = f.local(wi);
    // This is the closest tangent vector, we project R on the normal plane
    const Imath::V3f T = f.local(Imath::V3f(R.x, R.y, 0)); // Delay normalization

    if (w.z < FLOAT_MIN)
        return {};

    // Unstretch w and T by the roughness
    const Imath::V3f uw = Imath::V3f(w.x / a_x, w.y / a_y, w.z).normalized();
    const Imath::V3f uT = Imath::V3f(T.x / a_x, T.y / a_y, T.z).normalized();

    // Jacobian of the stretching to convert density
    const float jacobian = 1 / (a_x * a_y) * pown<3>(uw.z / w.z);
    // The cosine to the surface plane, which clips the lobe losing energy
    const float cutoff_cos = std::max(0.0f, uT.z);
    float       weight = 0, pdf = 0, expected_albedo = 1;
    if (isramp) {
        expected_albedo =
            truncated_cone_sum([&](float t) { return impl.ramp.cdf(t); }, cutoff_cos);
        pdf = impl.ramp.D(uw) * jacobian;
        // Except for energy compensation, perfect importance sampling
        weight = 1;
    } else {
        expected_albedo =
            truncated_cone_sum([&](float t) { return impl.hat.cdf(t); }, cutoff_cos);
        pdf           = impl.hat.pdf(uw) * jacobian;
        const float D = impl.hat.D(uw) * jacobian;
        // We found our weights to be safely under 2. We also scale up to conservate
        // energy. This increases noise but is better than darkening. The loss is
        // never higher than 0.5 anyway.
        weight = std::min(2.0f, D / pdf);
    }
    weight *= 1 / expected_albedo; // Energy compensation
    if (weight < PDF_MIN || pdf < PDF_MIN)
        return {};

    // The macronormal is our half vector, no microfacet model here.
    const float cosHO = std::min(wo.z, 1.0f);
    if (wi.z > FLOAT_MIN && dorefl) {
        const Power F = get_fresnel(cosHO);
        return { wi, F * (weight / refl_prob), refl_prob * pdf, Base::roughness() };
    } else if (wi.z < -FLOAT_MIN && dorefr) {
        const Power F = refr_tint * (Power::UNIT() - get_fresnel(cosHO)).clamped(0, 1);
        return {
            wi, F * (weight / (1 - refl_prob)), (1 - refl_prob) * pdf, Base::roughness()
        };
    } else
        return {};
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD Sample
ToonDielectricLobe<BSDF_ROOT>::sample_impl(const Imath::V3f& wo,
                                           const Imath::V3f& rnd) const
{
    constexpr auto sincosf = BSDLConfig::Fast::sincosf;
    if (!dorefl && !dorefr)
        return {};

    const bool reflect = rnd.z < refl_prob;
    // R is the axis of the lobe. Either the reflected or refracted wo direction
    Imath::V3f R =
        reflect ? Imath::V3f{ -wo.x, -wo.y, wo.z } : refract(wo, { 0, 0, 1 }, IOR);
    Frame f(R, { 1, 0, 0 });

    // Sample unstretched
    const float z   = isramp ? impl.ramp.sample_cos(rnd.y) : impl.hat.sample_cos(rnd.y);
    const float r   = sqrtf(std::max(0.0f, 1 - z * z));
    const float phi = 2 * PI * rnd.x;
    float       sinphi, cosphi;
    sincosf(phi, &sinphi, &cosphi);

    Imath::V3f uw = { r * cosphi, r * sinphi, z };

    // Stretch it
    Imath::V3f w = Imath::V3f(uw.x * a_x, uw.y * a_y, uw.z).normalized();
    // From lobe frame to BSDF space
    const Imath::V3f wi = f.world(w);

    if ((reflect && wi.z <= 0) || (!reflect && wi.z >= 0)) // Clip
        return {};

    return eval_impl(wo, wi); // Eval
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD Power ToonDielectricLobe<BSDF_ROOT>::get_fresnel(float c) const
{
    if (TIR) {
        // In non physical models like this, TIR can cause energy to blow
        // up with many bounces inside closed objects. This happens with high
        // roughness. That's why we are stealing 12% energy when roughness is
        // very high. Trial and error found this to be a good workaround.
        return refr_tint * std::max(0.0f, 1 - 0.125f * a_x * a_y);
    } else {
        // Because we are not using a real microfacet model, we evaluate fresnel
        // on the macro normal. We compensate by computing the average cosine with
        // an integral over a uniform spherical cap and a vector with cosine c.
        // First we guess the spherical cap cosine extent with the lobe avg tangent
        // and the roughness stretching.
        const float avg_tan   = isramp ? impl.ramp.avg_tan() : impl.hat.avg_tan();
        const float rough_cos = 1 / sqrtf(1 + a_x * a_y * SQR(avg_tan));
        // The integral tells us this would be the average cosine, decreases
        // with high roughness
        const float avg_cos = c * (1 + rough_cos) * 0.5f;
        // Then evaluate fresnel. This makes rougher surfaces more reflective
        // than transmissive.
        return LERP(pown<5>(1 - avg_cos), refl_0, refl_90);
    }
}

} // namespace spi

BSDL_LEAVE_NAMESPACE
