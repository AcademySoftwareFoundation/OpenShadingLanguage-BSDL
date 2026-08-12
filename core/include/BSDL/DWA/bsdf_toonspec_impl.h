// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <BSDL/DWA/bsdf_toonspec_decl.h>
#include <BSDL/ramp_impl.h>

BSDL_ENTER_NAMESPACE

namespace dwa
{

BSDL_INLINE_METHOD float ToonFresnel::eval(const float c) const
{
    assert(c >= 0);  // slightly above 1.0 is ok
    assert(eta > 1); // avoid singularity at eta==1
    // optimized for c in [0,1] and eta in (1,inf)
    const float g = sqrtf(eta * eta - 1 + c * c);
    const float A = (g - c) / (g + c);
    const float B = (c * (g + c) - 1) / (c * (g - c) + 1);
    return 0.5f * A * A * (1 + B * B);
}

template <typename BSDF_ROOT>
template <typename T>
BSDL_INLINE_METHOD
ToonSpecularLobe<BSDF_ROOT>::ToonSpecularLobe(T*                 lobe,
                                              const BsdfGlobals& globals,
                                              const Data&        data):
    Base(lobe, globals.visible_normal(data.N), data.U, 1.0f, globals.lambda_0, false),
    ramp(0),
    fresnel(data.IOR),
    tint(globals.wave(data.tint)),
    indirect_scale(data.indirect_scale),
    fresnel_blend(data.fresnel_blend),
    constant_reflectance(data.constant_reflectance),
    stretch_u(data.stretch_u),
    stretch_v(data.stretch_v)
{
    Base::sample_filter = globals.get_sample_filter(Base::frame.Z, true);
    if (data.ramp) {
        Ramp<> tmp(0);
        tmp.load(data.ramp, false /* no color */, false);
        // Guess our roughness from the unnormalize ramp integral
        const float roughness = CLAMP(tmp.integrate(), 0.0f, 1.0f);
        tmp.remap(
            [=](float x) {
                // This is how we do roughness regularization with a ramp. Bias
                // positions towards 1.0 to make it rougher.
                const float grow = std::max(globals.path_roughness - roughness, 0.0f);
                return BSDLConfig::Fast::powf(x, std::max(0.125f, SQR(1 - grow)));
            },
            false);
        // Now our roughness can be higher
        Base::set_roughness(std::max(roughness, globals.path_roughness));
        // Resample to cosine space
        ramp.cosine_resample(tmp, true);
    }

    // Decouple the layer fresnel if layer_IOR is provided
    ToonFresnel lfresnel = data.layer_IOR == 0 ? fresnel : ToonFresnel(data.layer_IOR);
    // Also use shading normal Nf to skip bump propagating down
    layer_fresnel = lfresnel.eval(CLAMP(globals.wo.dot(globals.Nf), 0.0f, 1.0f));
    if (data.layer_scale != 0.0f) {
        tint *= data.layer_scale;
        layer_fresnel *= data.layer_scale;
    }
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD Imath::V3f ToonSpecularLobe<BSDF_ROOT>::compute_n(const Imath::V3f& wo,
                                                                     const Imath::V3f& wi,
                                                                     bool stretch) const
{
    if (!stretch || (stretch_u == 0 && stretch_v == 0))
        return { 0, 0, 1 };
    // DWA uses wi reflected to compute this rotation
    Imath::V3f R = { wi.x, wi.y, -wi.z };

    // Rotate N to "stretch" the specular highlight
    const float dot_u_l = R.x;
    const float dot_u_c = wo.x;
    const float dot_u   = dot_u_l + dot_u_c;
    const float rot_u   = CLAMP(stretch_u * dot_u, -0.5f, 0.5f);
    Imath::V3f  N       = rotate(Imath::V3f(0, 0, 1), Imath::V3f(0, 1, 0), rot_u);

    const float dot_v_l = R.y;
    const float dot_v_c = wo.y;
    const float dot_v   = dot_v_l + dot_v_c;
    const float rot_v   = CLAMP(-stretch_v * dot_v, -0.5f, 0.5f);
    return rotate(N, Imath::V3f(1, 0, 0), rot_v);
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD Sample ToonSpecularLobe<BSDF_ROOT>::eval_impl(const Imath::V3f& wo,
                                                                 const Imath::V3f& wi,
                                                                 bool stretch) const
{
    Imath::V3f  N = compute_n(wo, wi, stretch);
    Imath::V3f  R = wi - 2.0f * wi.dot(N) * N;
    const float c = CLAMP(-wo.dot(R), 0.0f, 1.0f);
    if (wi.dot(N) < 0 || wo.dot(N) < 0)
        return {};
    auto [reval, rpdf] =
        ramp.size() ? ramp.eval(1 - c, true) : std::make_pair(Imath::C3f(2 * c), 2 * c);
    if (rpdf < PDF_MIN)
        return {};
    const float pdf = rpdf / (2 * PI);
    const float F   = LERP(
        fresnel_blend, constant_reflectance, fresnel.eval(CLAMP(wo.dot(N), 0.0f, 1.0f)));
    const Imath::C3f weight = reval * (1 / rpdf); // 2 PI factors cancel
    return { wi, Power(weight, 0) * tint * F, pdf, Base::roughness() };
}

template <typename BSDF_ROOT>
BSDL_INLINE_METHOD Sample
ToonSpecularLobe<BSDF_ROOT>::sample_impl(const Imath::V3f& wo,
                                         const Imath::V3f& rnd) const
{
    Imath::V3f R = { -wo.x, -wo.y, wo.z }; // Easy reflect
    // Build a frame around R
    Imath::V3f U = R.cross(wo.z < 0.99f ? wo : Imath::V3f(0, 1, 0)).normalized();
    Imath::V3f V = U.cross(R);

    const float cos_theta = ramp.size() ? 1 - ramp.sample(rnd.x) : sqrtf(1 - rnd.x);
    const float sin_theta = sqrtf(1 - SQR(cos_theta));
    const float phi       = 2 * PI * rnd.y;
    float       sin_phi, cos_phi;
    BSDLConfig::Fast::sincosf(phi, &sin_phi, &cos_phi);
    Imath::V3f wi = cos_phi * sin_theta * U + sin_phi * sin_theta * V + cos_theta * R;
    return eval_impl(wo, wi, false);
}

} // namespace dwa

BSDL_LEAVE_NAMESPACE
