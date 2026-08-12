// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <BSDL/DWA/bsdf_toondiffuse_decl.h>
#include <BSDL/ramp_impl.h>

BSDL_ENTER_NAMESPACE

namespace dwa
{

template <typename BSDF_ROOT, bool TR>
template <typename T>
BSDL_INLINE_METHOD ToonDiffuseLobeGen<BSDF_ROOT, TR>::ToonDiffuseLobeGen(
    T*                                             lobe,
    const BsdfGlobals&                             globals,
    const ToonDiffuseLobeGen<BSDF_ROOT, TR>::Data& data):
    Base(lobe, globals.visible_normal(data.N), 1.0f, globals.lambda_0, TR),
    ramp(0)
{
    Base::sample_filter = globals.get_sample_filter(Base::frame.Z, !TR);
    if (data.ramp) {
        // NOTE: The third `true` (normalize) differs from the SPI version.
        // The DWA ramp path handles the normalization expectation internally
        // and avoids a double-dip with the 1/(2*pi) in the final weight.
        ramp.load(data.ramp, true /* use color */, true);
    }
}

template <typename BSDF_ROOT, bool TR>
BSDL_INLINE_METHOD Sample
ToonDiffuseLobeGen<BSDF_ROOT, TR>::eval_impl(const Imath::V3f& wo,
                                             const Imath::V3f& wi) const
{
    // NOTE: DWA uses the signed cosine of wi.z directly (not z after mirroring
    // like SPI does). The sign check determines which side of the normal the
    // sample falls on, enabling translucent transmission.
    float IdotN = CLAMP(wi.z, -1.0f, 1.0f);
    // For normal diffuse both wi and wo have to be above N (IdotN > 0).
    // For translucent mode we need IdotN < 0 (the other side).
    if ((!TR && IdotN <= 0) || (TR && IdotN >= 0))
        return {};

    // fabsf(IdotN) here is an intentional design choice: DWA renders best with
    // |cos| rather than cos on the back-face. See notes in the shader where
    // users may get different results from SPI for back-face lighting.
    if (ramp.size()) {
        // To match DWA, we don't normalize eval. Note rpdf will always be
        // normalized.
        // NOTE: 1e-6 is used here; prefer PDF_MIN for consistency with SPI.
        auto [reval, rpdf] = ramp.eval(1 - fabsf(IdotN), false /* normalize */);
        if (rpdf < 1e-6f)
            return {};
        const float      pdf    = rpdf / (2 * PI);
        const Imath::C3f weight = reval * (1 / rpdf); // 2 PI factors cancel
        // We don't expect this to work in spectral, otherwise we have to keep
        // lambda_0 for upsampling.
        return { wi, Power(weight, 0), pdf, 1.0f };
    }
    // Fallback uses |cos| * 1/pi (Lambertian with |cos|). The fabsf on the
    // back-face ensures the shader doesn't produce dark (or inverted) results.
    return { wi, Power::UNIT(), fabsf(IdotN) * ONEOVERPI, 1.0f };
}

template <typename BSDF_ROOT, bool TR>
BSDL_INLINE_METHOD Sample
ToonDiffuseLobeGen<BSDF_ROOT, TR>::sample_impl(const Imath::V3f& wo,
                                               const Imath::V3f& rnd) const
{
    Imath::V3f wi;
    if (ramp.size()) {
        const float cos_theta = 1 - ramp.sample(rnd.x);
        const float sin_theta = sqrtf(1 - SQR(cos_theta));
        const float phi       = 2 * PI * rnd.y;
        float       sin_phi, cos_phi;
        BSDLConfig::Fast::sincosf(phi, &sin_phi, &cos_phi);
        wi = { cos_phi * sin_theta, sin_phi * sin_theta, cos_theta };
    } else
        wi = sample_cos_hemisphere(rnd.x, rnd.y);

    if (TR)
        wi.z = -wi.z;
    Sample s = eval_impl(wo, wi);
    return s;
}

} // namespace dwa

BSDL_LEAVE_NAMESPACE
