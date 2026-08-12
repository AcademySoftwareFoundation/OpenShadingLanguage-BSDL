// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <BSDL/bsdf_decl.h>

BSDL_ENTER_NAMESPACE

namespace dwa
{

template <typename BSDF_ROOT, bool TR = false>
struct FlatDiffuseLobeGen : public Lobe<BSDF_ROOT> {
    using Base = Lobe<BSDF_ROOT>;

    static constexpr LabelSet labels = { TR ? Label::BACK : Label::FRONT,
                                         Label::DIFFUSE,
                                         Label::SURFACE };

    struct Data {
        Imath::V3f N;
        float      sigma;
        float      flatness;
        float      flatness_falloff;
        float      terminator_shift;
        using lobe_type = FlatDiffuseLobeGen<BSDF_ROOT, TR>;
    };
    template <typename D> static typename LobeRegistry<D>::Entry entry()
    {
        static_assert(std::is_base_of<Data, D>::value); // Make no other assumptions
        using R = LobeRegistry<D>;
        return { name(),
                 { R::param(&D::N),
                   R::param(&D::sigma),
                   R::param(&D::flatness),
                   R::param(&D::flatness_falloff),
                   R::param(&D::terminator_shift),
                   R::close() } };
    }

    template <typename T>
    BSDL_INLINE_METHOD
    FlatDiffuseLobeGen(T*, const BsdfGlobals& globals, const Data& data);

    static constexpr const char* name()
    {
        return TR ? "dwa_flat_diffuse_translucent" : "dwa_flat_diffuse";
    }

    BSDL_INLINE_METHOD Sample eval_impl(const Imath::V3f& wo,
                                        const Imath::V3f& _wi) const;
    BSDL_INLINE_METHOD Sample sample_impl(const Imath::V3f& wo,
                                          const Imath::V3f  rnd) const;
    float                     A, B;
    float                     flatness;
    float                     flatness_falloff;
    float                     terminator_shift;
};

template <typename BSDF_ROOT>
using FlatDiffuseLobe = FlatDiffuseLobeGen<BSDF_ROOT, false>;
template <typename BSDF_ROOT>
using FlatDiffuseTranslucentLobe = FlatDiffuseLobeGen<BSDF_ROOT, true>;

} // namespace dwa

BSDL_LEAVE_NAMESPACE
