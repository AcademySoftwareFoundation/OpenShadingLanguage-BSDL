// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <BSDL/bsdf_decl.h>

BSDL_ENTER_NAMESPACE

namespace dwa
{

template <typename BSDF_ROOT> struct HairDiffuseLobe : public Lobe<BSDF_ROOT> {
    using Base = Lobe<BSDF_ROOT>;

    static constexpr LabelSet labels = { Label::DIFFUSE,
                                         Label::FRONT,
                                         Label::BACK,
                                         Label::CURVE };

    struct Data {
        Imath::V3f T;
        Imath::C3f tint_front;
        Imath::C3f tint_back;
        using lobe_type = HairDiffuseLobe<BSDF_ROOT>;
    };
    template <typename D> static typename LobeRegistry<D>::Entry entry()
    {
        static_assert(std::is_base_of<Data, D>::value); // Make no other assumptions
        using R = LobeRegistry<D>;
        return { name(),
                 { R::param(&D::T),
                   R::param(&D::tint_front),
                   R::param(&D::tint_back),
                   R::close() } };
    }

    template <typename T>
    BSDL_INLINE_METHOD HairDiffuseLobe(T*, const BsdfGlobals& globals, const Data& data);

    static constexpr const char* name() { return "dwa_hair_diffuse"; }

    BSDL_INLINE_METHOD Sample eval_impl(const Imath::V3f& wo,
                                        const Imath::V3f& _wi) const;
    BSDL_INLINE_METHOD Sample sample_impl(const Imath::V3f& wo,
                                          const Imath::V3f  rnd) const;

    Power tint_front;
    Power tint_back;
};

} // namespace dwa

BSDL_LEAVE_NAMESPACE
