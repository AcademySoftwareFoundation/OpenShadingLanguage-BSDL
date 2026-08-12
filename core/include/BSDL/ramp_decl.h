#pragma once

#include "BSDL/config.h"
#include "BSDL/tools.h"
#include <BSDL/config_pvt.h>
#include <Imath/ImathColor.h>
#include <cstdint>

BSDL_ENTER_NAMESPACE

struct RampInput {
    static constexpr unsigned CAPACITY = 10;

    float      x0, x1, x2, x3, x4, x5, x6, x7, x8, x9;
    Imath::C3f y0, y1, y2, y3, y4, y5, y6, y7, y8, y9;
    float      input_scale;
    int        interpolation; // 0 linear, 1 constant
    unsigned   size;
};

template <unsigned MAX = RampInput::CAPACITY> struct Ramp {
    static constexpr uint16_t INTFORONE = 65535;

    struct RGB {
        uint32_t rgb1110;

        RGB() = default;
        BSDL_INLINE_METHOD RGB(const Imath::C3f& c) { set(c); }

        BSDL_INLINE_METHOD Imath::C3f get() const
        {
            const float scale11 = 1.0f / 2047;
            const float scale10 = 1.0f / 1023;

            Imath::C3f result;
            result.x = (rgb1110 & 0x7FF) * scale11;
            result.y = ((rgb1110 >> 11) & 0x7FF) * scale11;
            result.z = ((rgb1110 >> 22) & 0x3FF) * scale10;
            return result;
        }

        BSDL_INLINE_METHOD void set(const Imath::C3f& c)
        {
            const float MAX_RGB1110 = 1.0f;
            const float rc = c.x > 0 ? (c.x < MAX_RGB1110 ? c.x : MAX_RGB1110) : 0;
            const float gc = c.y > 0 ? (c.y < MAX_RGB1110 ? c.y : MAX_RGB1110) : 0;
            const float bc = c.z > 0 ? (c.z < MAX_RGB1110 ? c.z : MAX_RGB1110) : 0;

            const uint16_t rm = rc * 2047;
            const uint16_t gm = gc * 2047;
            const uint16_t bm = bc * 1023;

            rgb1110 = rm | (gm << 11) | (bm << 22);
        }
    };

    union Value {
        float scalar;
        RGB   color;
    };

    Ramp(const Ramp& o) = default;
    Ramp()              = default;
    BSDL_INLINE_METHOD Ramp(unsigned len): len(len) {}

    BSDL_INLINE_METHOD void                       load(const RampInput*  in,
                                                       bool              is_color,
                                                       bool              dointe,
                                                       const Imath::C3f& tint = Imath::C3f(1.0f));
    template <typename F> BSDL_INLINE_METHOD void remap(const F& f, bool dointe)
    {
        uint16_t safe_min = 0, safe_max = INTFORONE - len - 1;
        for (unsigned i = 0; i < len; ++i) {
            curve_x[i] = std::max(safe_min, std::min(safe_max, float2int(f(loc(i)))));
            safe_min   = curve_x[i] + 1;
            safe_max   = i == len - 2 ? INTFORONE : safe_max + 1;
        }
        total_integral = dointe ? integrate() : 1;
    }
    BSDL_INLINE_METHOD void cosine_resample(const Ramp<MAX>& o, bool dointe);

    BSDL_INLINE_METHOD unsigned size() const { return len; }
    BSDL_INLINE_METHOD void     set(unsigned i, float x, const Imath::C3f& y)
    {
        curve_x[i] = float2int(x);
        curve_y[i].color.set(y);
    }
    BSDL_INLINE_METHOD void set(unsigned i, float x, float y)
    {
        curve_x[i] = float2int(x);
        curve_y[i] = y;
    }

    BSDL_INLINE_METHOD std::pair<Imath::C3f, float> eval(float x,
                                                         bool  normalize = false) const;

    BSDL_INLINE_METHOD float sample(float rnd) const;
    BSDL_INLINE_METHOD float integrate(float xmax = BIG) const;
    BSDL_INLINE_METHOD float integral() const { return total_integral; }

  private:
    BSDL_INLINE_METHOD float loc(int i) const
    {
        return i < 0 ? 0.0f : (i > len - 1 ? 1 : int2float(curve_x[i]));
    }

    BSDL_INLINE_METHOD float scalar(int i) const
    {
        i = std::max(0, std::min(i, len - 1));
        return color ? MAX_RGB(curve_y[i].color.get()) : curve_y[i].scalar;
    }

    BSDL_INLINE_METHOD Imath::C3f rgb(int i) const
    {
        i = std::max(0, std::min(i, len - 1));
        return color ? curve_y[i].color.get() : Imath::C3f(curve_y[i].scalar);
    }

    BSDL_INLINE_METHOD float area(int i, float cut = BIG) const
    {
        const float xa = loc(i), xb = loc(i + 1);
        const float ya = scalar(i), yb = scalar(i + 1);
        if (cut < xb) {
            if (constant)
                return (cut - xa) * ya;
            else {
                const float a = (yb - ya) / (xb - xa);
                const float b = ya - a * xa;
                return 0.5f * (cut - xa) * (a * (cut + xa) + 2 * b);
            }
        } else
            return constant ? (xb - xa) * ya : 0.5f * (xb - xa) * (ya + yb);
    }

    BSDL_INLINE_METHOD static float int2float(uint16_t i)
    {
        return i * (1.0f / INTFORONE);
    }
    BSDL_INLINE_METHOD static uint16_t float2int(float f)
    {
        return CLAMP(f, 0.0f, 1.0f) * INTFORONE;
    }

    int16_t  len;
    bool     color;
    bool     constant;
    float    total_integral;
    uint16_t curve_x[MAX];
    Value    curve_y[MAX];
};

BSDL_LEAVE_NAMESPACE