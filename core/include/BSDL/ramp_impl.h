// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <BSDL/ramp_decl.h>
#include <BSDL/tools.h>
#include <cstdint>

BSDL_ENTER_NAMESPACE

template <unsigned MAX>
BSDL_INLINE_METHOD void
Ramp<MAX>::load(const RampInput* in, bool is_color, bool dointe, const Imath::C3f& tint)
{
    len      = std::min(in->size, MAX);
    color    = is_color;
    constant = in->interpolation == 1;
    const float input_scale =
        std::min(1.0f, in->input_scale != 0 ? in->input_scale : 1.0f);
    const float*      inx = &in->x0;
    const Imath::C3f* iny = &in->y0;
    // And let's live dangerously assuming array like layout
    uint16_t safe_min = 0, safe_max = INTFORONE - len - 1;
    for (unsigned i = 0; i < len; ++i) {
        // Ensure proper spacing between control points
        curve_x[i] =
            std::max(safe_min, std::min(safe_max, float2int(inx[i] * input_scale)));
        safe_min = curve_x[i] + 1;
        safe_max = i == len - 2 ? INTFORONE : safe_max + 1;
    }

    if (color) {
        for (unsigned i = 0; i < len; ++i)
            curve_y[i].color.set(iny[i] * tint);
    } else {
        for (unsigned i = 0; i < len; ++i)
            curve_y[i].scalar = MAX_RGB(iny[i]);
    }
    total_integral = dointe ? integrate() : 1;
}

template <unsigned MAX>
BSDL_INLINE_METHOD void Ramp<MAX>::cosine_resample(const Ramp<MAX>& o, bool dointe)
{
    // Map from angle to cosine space for easy integral/sampling. Note we
    // have to flip it to preserve 0 - 1 order.
    len               = o.len;
    color             = o.color;
    constant          = o.constant;
    uint16_t safe_min = 0, safe_max = INTFORONE - len - 1;
    for (unsigned i = 0; i < len; ++i) {
        const float angle01 = o.loc(i);
        const float cos01   = 1 - BSDLConfig::Fast::cosf(angle01 * PI * 0.5f);
        curve_x[i]          = std::max(safe_min, std::min(safe_max, float2int(cos01)));
        curve_y[i]          = o.curve_y[i];
        safe_min            = curve_x[i] + 1;
        safe_max            = i == len - 2 ? INTFORONE : safe_max + 1;
    }
    total_integral = dointe ? integrate() : 1;
}

template <unsigned MAX> BSDL_INLINE_METHOD float Ramp<MAX>::integrate(float xmax) const
{
    float     total = 0;
    uint16_t  uxmax = float2int(xmax);
    const int begin = curve_x[0] > 0 ? -1 : 0;
    const int end   = curve_x[len - 1] < INTFORONE ? len : len - 1;
    for (int i = begin; i < end && curve_x[i] < uxmax; ++i)
        total += area(i, xmax);
    return total;
}

template <unsigned MAX>
BSDL_INLINE_METHOD std::pair<Imath::C3f, float> Ramp<MAX>::eval(float x,
                                                                bool  normalize) const
{
    const int begin = curve_x[0] > 0 ? -1 : 0;
    const int end   = curve_x[len - 1] < INTFORONE ? len : len - 1;
    if (x < 0 || 1 < x)
        return { Imath::C3f(0), 0 };
    if (total_integral < EPSILON)
        return { {}, 0.0f };
    for (int i = begin; i < end; ++i) {
        if (x <= loc(i + 1)) {
            const float length = loc(i + 1) - loc(i);
            // TODO: Add more interpolation modes
            Imath::C3f y =
                constant ? rgb(i) : LERP((x - loc(i)) / length, rgb(i), rgb(i + 1));
            const float pdf = MAX_RGB(y) / total_integral;
            if (normalize)
                // Use the linear interpolation integral, even if y is non linear
                return { y * (1 / total_integral), pdf };
            else
                return { y, pdf };
        }
    }
    return { Imath::C3f(0), 0 };
}

template <unsigned MAX> BSDL_INLINE_METHOD float Ramp<MAX>::sample(float rnd) const
{
    float       sum   = 0;
    const float total = integrate();
    const int   begin = curve_x[0] > 0 ? -1 : 0;
    const int   end   = curve_x[len - 1] < INTFORONE ? len : len - 1;
    for (int i = begin; i < end; ++i) {
        const float area_i   = area(i);
        const float next_cdf = i < end - 1 ? (sum + area_i) / total : 1;
        if (rnd < next_cdf) {
            const float prev_cdf = sum / total;
            const float u        = (rnd - prev_cdf) / (next_cdf - prev_cdf);
            if (constant)
                return LERP(u, loc(i), loc(i + 1));
            const float a = scalar(i);
            const float b = scalar(i + 1);
            // The normalized PDF is p(t) = (a + (b-a)*t) * (2/(a+b))
            // then the CDF is u = (0.5*(b-a)*x^2 + a*x)* (2/(a+b))
            // Inverting the CDF just consist in solving the quadratric equation of
            // type a*x^2 + b*x + c=0.
            float t =
                u * (a + b) / std::max(a + sqrtf(LERP(u, SQR(a), SQR(b))), FLOAT_MIN);
            return LERP(t, loc(i), loc(i + 1));
        }
        sum += area_i;
    }
    return 0;
}

BSDL_LEAVE_NAMESPACE