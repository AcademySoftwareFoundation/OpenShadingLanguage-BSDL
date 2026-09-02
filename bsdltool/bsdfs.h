#pragma once

#include "BSDL/config.h"
#include <BSDL/bsdf_decl.h>
#include <BSDL/ramp_impl.h>
#include <BSDL/static_virtual.h>

#include <BSDL/MTX/bsdf_burley_diffuse_impl.h>
#include <BSDL/MTX/bsdf_conductor_impl.h>
#include <BSDL/MTX/bsdf_dielectric_impl.h>
#include <BSDL/MTX/bsdf_oren_nayar_diffuse_impl.h>
#include <BSDL/MTX/bsdf_schlick_impl.h>
#include <BSDL/MTX/bsdf_sheen_impl.h>
#include <BSDL/MTX/bsdf_translucent_impl.h>
#include <BSDL/SPI/bsdf_backscatter_impl.h>
#include <BSDL/SPI/bsdf_clearcoat_impl.h>
#include <BSDL/SPI/bsdf_dielectric_impl.h>
#include <BSDL/SPI/bsdf_diffuse_impl.h>
#include <BSDL/SPI/bsdf_metal_impl.h>
#include <BSDL/SPI/bsdf_oren_nayar_impl.h>
#include <BSDL/SPI/bsdf_physicalhair_impl.h>
#include <BSDL/SPI/bsdf_sheenltc_impl.h>
#include <BSDL/SPI/bsdf_thinlayer_impl.h>
#include <BSDL/SPI/bsdf_toondielectric_impl.h>
#include <BSDL/SPI/bsdf_toondiffuse_impl.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

struct Bsdf;

// Aggregate of several BSDFs combined with multiple importance sampling.
// Defined after Bsdf below; only the instantiations listed in VirtualBsdf
// can be used through a Bsdf pointer. Groups cannot be nested: Bsdf::eval /
// sample / albedo flatten a group into its child list so there is no
// recursion and a single dispatch call site, as GPU targets require.
template <int MAX> struct GroupBsdf;

constexpr int GROUP_BSDF_MAX = 8;

using VirtualBsdf = bsdl::StaticVirtual<bsdl::spi::DiffuseLobe<Bsdf>,
                                        bsdl::spi::TranslucentLobe<Bsdf>,
                                        bsdl::spi::BasicDiffuseLobe<Bsdf>,
                                        bsdl::spi::MetalLobe<Bsdf>,
                                        bsdl::spi::ClearCoatLobe<Bsdf>,
                                        bsdl::spi::PhysicalHairLobe<Bsdf>,
                                        bsdl::spi::HairDiffuseLobe<Bsdf>,
                                        bsdl::spi::HairSpecularLobe<Bsdf>,
                                        bsdl::spi::DielectricLobe<Bsdf>,
                                        bsdl::spi::ThinLayerLobe<Bsdf>,
                                        bsdl::spi::ToonDiffuseLobe<Bsdf>,
                                        bsdl::spi::ToonTranslucentLobe<Bsdf>,
                                        bsdl::spi::OrenNayarLobe<Bsdf>,
                                        bsdl::spi::OrenNayarTranslucentLobe<Bsdf>,
                                        bsdl::spi::ToonDielectricLobe<Bsdf>,
                                        bsdl::spi::SheenLTCLobe<Bsdf>,
                                        bsdl::spi::CharlieLobe<Bsdf>,
                                        bsdl::mtx::BurleyDiffuseLobe<Bsdf>,
                                        bsdl::mtx::ConductorLobe<Bsdf>,
                                        bsdl::mtx::DielectricLobe<Bsdf>,
                                        bsdl::mtx::OrenNayarDiffuseLobe<Bsdf>,
                                        bsdl::mtx::SchlickLobe<Bsdf>,
                                        bsdl::mtx::SheenLobe<Bsdf>,
                                        bsdl::mtx::TranslucentLobe<Bsdf>,
                                        GroupBsdf<GROUP_BSDF_MAX>>;

using BsdfLobes = VirtualBsdf::types;

struct Bsdf : public VirtualBsdf {
    template <typename LOBE>
    Bsdf(LOBE* lobe, float roughness, float, bool):
        VirtualBsdf(lobe),
        roughness_(roughness)
    {
    }

    void  set_roughness(float roughness) { roughness_ = roughness; }
    float roughness() const { return roughness_; }
    void  set_weight(const Imath::C3f& weight) { weight_ = weight; }
    void  set_weight(float u, float v) { weight_ = { u, v, 0 }; }

    // Dispatch to the lobe's albedo_impl (Lobe provides a Power::UNIT()
    // default), scaled by the Bsdf weight like eval() and sample().
    bsdl::Power albedo() const;

    // Dispatch to the lobe's filter_o (Lobe provides a Power::ZERO()
    // default): the fraction of light this BSDF transmits to layers below it
    // in a layered 'over' combination, for outgoing direction wo in world
    // space. NOT scaled by the Bsdf weight.
    bsdl::Power filter_o(const Imath::V3f& wo) const;

    // Defined after GroupBsdf below: the static dispatch needs every listed
    // type to be complete at the point of definition.
    bsdl::Sample eval(const Imath::V3f& wo, const Imath::V3f& wi) const;

    bsdl::Sample sample(const Imath::V3f& wo, const Imath::V3f& random) const;

  private:
    float      roughness_ = 0;
    Imath::C3f weight_    = { 1, 1, 1 };

    // Flat view of a Bsdf as a list of child BSDFs: a GroupBsdf contributes
    // its children, any other Bsdf behaves as a group of one. Defined out of
    // line below (needs GroupBsdf to be complete).
    struct BsdfChildList {
        BSDL_INLINE_METHOD BsdfChildList(const Bsdf* bsdf);

        const Bsdf* child(int i) const { return list[i]; }
        int         count() const { return nbsdfs; }
        float       choice_pdf(int i) const;

        const Bsdf*                      self;  // the BSDF itself (group of one)
        const Bsdf* const*               list;  // == self when not a group
        const GroupBsdf<GROUP_BSDF_MAX>* group; // null for a plain BSDF
        int                              nbsdfs;
    };
    BsdfChildList as_list() const { return BsdfChildList(this); }
};

// Aggregate BSDF: references up to MAX child BSDFs and combines them with
// multiple importance sampling (the balance heuristic implemented by
// bsdl::Sample::update). Sampling picks one child with probability
// proportional to the maximum channel of its albedo, then MIS-combines every
// child's response along the sampled direction; evaluation MIS-combines all
// children directly.
//
// Groups cannot be nested and this type has no lobe behavior of its own:
// Bsdf::eval / sample / albedo detect a group and iterate over its children
// instead of dispatching to it, so there is no recursion and a single
// dispatch call site, as GPU targets require. The *_impl methods below are
// dead stubs that exist only so that the static dispatch over the child
// lists still compiles for the GroupBsdf type.
//
// Children are referenced, not owned: they must outlive the group. Note only
// the instantiations listed in VirtualBsdf (currently GroupBsdf<GROUP_BSDF_MAX>)
// can be used through a Bsdf pointer.
template <int MAX> struct GroupBsdf : public Bsdf {
    static_assert(MAX > 0);

    // Placeholder so the registry machinery below can measure sizes; groups
    // are built from child pointers, never from parameter data.
    struct Data {
    };

    // Minimum per-child importance: keeps every selection probability non zero
    // so that eval and sample stay consistent with each other.
    static constexpr float MIN_IMPORTANCE = 1e-4f;

    GroupBsdf(const Bsdf* const* children, int count):
        Bsdf(this, 1.0f, 0.0f, false),
        count_(count < 0 ? 0 : (count > MAX ? MAX : count))
    {
        float total = 0;
        for (int i = 0; i < count_; ++i) {
            // No recursion allowed: children must be plain BSDFs, not groups.
            assert(children[i] != nullptr);
            assert(children[i]->get_id() != VirtualBsdf::GET_ID<GroupBsdf>());
            children_[i] = children[i];
            // Importance from albedo: maximum over the spectral channels.
            importance_[i] = std::max(children[i]->albedo().max(), MIN_IMPORTANCE);
            total += importance_[i];
        }
        float accum = 0;
        for (int i = 0; i < count_; ++i) {
            accum += importance_[i] / total;
            cdf_[i] = accum;
        }
        if (count_)
            cdf_[count_ - 1] = 1.0f; // Make sure the CDF ends exactly at 1
    }

    // Dead dispatch stubs, never called (see the comment above).
    BSDL_INLINE_METHOD bsdl::Sample eval_impl(const Imath::V3f&, const Imath::V3f&) const
    {
        return {};
    }
    BSDL_INLINE_METHOD bsdl::Sample sample_impl(const Imath::V3f&,
                                                const Imath::V3f&) const
    {
        return {};
    }
    BSDL_INLINE_METHOD bsdl::Power albedo_impl() const { return bsdl::Power::ZERO(); }
    BSDL_INLINE_METHOD bsdl::Power filter_o(const Imath::V3f&) const
    {
        return bsdl::Power::ZERO();
    }

    const Bsdf* const* children() const { return children_; }
    int                count() const { return count_; }
    float              cdf(int i) const { return cdf_[i]; }
    // Probability of picking child i when sampling.
    float choice_pdf(int i) const { return cdf_[i] - (i ? cdf_[i - 1] : 0.0f); }

  private:
    const Bsdf* children_[MAX];
    float       importance_[MAX];
    float       cdf_[MAX];
    int         count_;
};

// Trait to exclude GroupBsdf from the closure registry: it is built from an
// array of BSDF pointers, not from parameter data.
template <typename Lobe> struct IsGroupBsdf : std::false_type {
};
template <int MAX> struct IsGroupBsdf<GroupBsdf<MAX>> : std::true_type {
};

BSDL_INLINE_METHOD Bsdf::BsdfChildList::BsdfChildList(const Bsdf* bsdf):
    self(bsdf),
    list(&self),
    group(nullptr),
    nbsdfs(1)
{
    if (bsdf->get_id() == VirtualBsdf::GET_ID<GroupBsdf<GROUP_BSDF_MAX>>()) {
        group  = static_cast<const GroupBsdf<GROUP_BSDF_MAX>*>(bsdf);
        list   = group->children();
        nbsdfs = group->count();
    }
}

BSDL_INLINE_METHOD float Bsdf::BsdfChildList::choice_pdf(int i) const
{
    return group ? group->choice_pdf(i) : 1.0f;
}

inline bsdl::Power Bsdf::filter_o(const Imath::V3f& wo) const
{
    // Called on group children only (groups cannot be nested), so a direct
    // dispatch is fine. Deliberately NOT scaled by weight_: the filter is
    // the fraction of light this layer transmits to the layers below,
    // independent of how much light reached this layer.
    const auto* lobe = static_cast<const bsdl::Lobe<Bsdf>*>(this);
    return dispatch(
        [&](const auto& object) { return object.filter_o(lobe->frame.local(wo)); });
}

inline bsdl::Power Bsdf::albedo() const
{
    const auto  children = as_list();
    bsdl::Power a        = bsdl::Power::ZERO();
    for (int i = 0; i < children.count(); ++i) {
        const Bsdf* child = children.child(i);
        a += child->dispatch([&](const auto& object) { return object.albedo_impl(); }) *
             bsdl::Power(child->weight_, 0);
    }
    // A group's own weight scales the whole group.
    if (children.group)
        a *= bsdl::Power(weight_, 0);
    return a;
}

inline bsdl::Sample Bsdf::eval(const Imath::V3f& wo, const Imath::V3f& wi) const
{
    const auto   children = as_list();
    bsdl::Sample result   = {};
    for (int i = 0; i < children.count(); ++i) {
        const Bsdf*  child = children.child(i);
        const auto*  lobe  = static_cast<const bsdl::Lobe<Bsdf>*>(child);
        bsdl::Sample s     = child->dispatch([&](const auto& object) {
            return object.eval_impl(lobe->frame.local(wo), lobe->frame.local(wi));
        });
        s.weight *= bsdl::Power(child->weight_, 0);
        if (s.pdf > 0) {
            result.update(s.weight, s.pdf, children.choice_pdf(i));
            result.roughness = std::max(result.roughness, s.roughness);
        }
    }
    if (children.group)
        result.weight *= bsdl::Power(weight_, 0);
    return result;
}

inline bsdl::Sample Bsdf::sample(const Imath::V3f& wo, const Imath::V3f& random) const
{
    const auto children = as_list();
    const int  count    = children.count();
    if (count == 0)
        return {};
    // Pick the child to sample: via the group's importance CDF, or the
    // single child of a plain BSDF.
    int   chosen = 0;
    float x      = random.x;
    if (children.group) {
        while (chosen < count - 1 && random.x >= children.group->cdf(chosen))
            ++chosen;
        const float start = chosen ? children.group->cdf(chosen - 1) : 0.0f;
        x = bsdl::Sample::stretch(random.x, start, children.group->cdf(chosen) - start);
    }
    const Bsdf*  cchild = children.child(chosen);
    const auto*  clobe  = static_cast<const bsdl::Lobe<Bsdf>*>(cchild);
    bsdl::Sample cs     = cchild->dispatch([&](const auto& object) {
        return object.sample_impl(clobe->frame.local(wo),
                                  Imath::V3f(x, random.y, random.z));
    });
    if (cs.null())
        return {};
    cs.wi = clobe->frame.world(cs.wi).normalized();
    cs.weight *= bsdl::Power(cchild->weight_, 0);
    // MIS-combine the chosen child sample with every child's response along
    // the sampled direction. Children are never groups, so calling eval() on
    // them is not recursion: Bsdf::eval holds the only eval dispatch site.
    bsdl::Sample result = { cs.wi, bsdl::Power::ZERO(), 0, cs.roughness };
    for (int i = 0; i < count; ++i) {
        const bsdl::Sample s = i == chosen ? cs : children.child(i)->eval(wo, cs.wi);
        if (s.pdf > 0)
            result.update(s.weight, s.pdf, children.choice_pdf(i));
    }
    if (children.group)
        result.weight *= bsdl::Power(weight_, 0);
    return result;
}

struct Parameter {
    std::string     name;
    bsdl::LobeParam lobe_parameter;
};

inline bool parse_float(std::string_view text, float& value)
{
    const auto [next, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc() && next == text.data() + text.size();
}

inline bool parse_int(std::string_view text, int& value)
{
    const auto [next, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc() && next == text.data() + text.size();
}

inline std::string_view trim_string(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    return text;
}

inline bool parse_vector(std::string_view text, Imath::V3f& value)
{
    float components[3];
    for (int i = 0; i < 3; ++i) {
        const std::size_t separator = text.find(',');
        if (!parse_float(trim_string(text.substr(0, separator)), components[i]))
            return false;
        if (i < 2 && separator == std::string_view::npos)
            return false;
        text = separator == std::string_view::npos
                   ? std::string_view()
                   : trim_string(text.substr(separator + 1));
    }
    if (!text.empty())
        return false;

    value = { components[0], components[1], components[2] };
    return true;
}

inline bool
assign_parameter(void* data, const bsdl::LobeParam& parameter, std::string_view value)
{
    void* destination = static_cast<char*>(data) + parameter.offset;

    switch (parameter.type) {
        case bsdl::ParamType::INT:
            return parse_int(value, *static_cast<int*>(destination));
        case bsdl::ParamType::FLOAT:
            return parse_float(value, *static_cast<float*>(destination));
        case bsdl::ParamType::VECTOR:
        case bsdl::ParamType::COLOR:
            return parse_vector(value, *static_cast<Imath::V3f*>(destination));
        case bsdl::ParamType::STRING:
            *static_cast<bsdl::Stringhash*>(destination) =
                reinterpret_cast<bsdl::Stringhash>(value.data());
            return true;
        default: return false;
    }
}

struct BsdfDescription {
    Bsdf* create_lobe(void*             arena,
                      const void*       data,
                      const Imath::V3f& wo,
                      const Imath::V3f& Nf,
                      const Imath::V3f& Ngf,
                      bool              backfacing,
                      float             path_roughness,
                      float             outer_ior,
                      float             lambda_0) const
    {
        return VirtualBsdf::dispatch(id, [&](auto& object) -> Bsdf* {
            using Lobe = std::remove_reference_t<decltype(object)>;
            if constexpr (IsGroupBsdf<Lobe>::value) {
                // Groups are built from child BSDF pointers, not from data.
                return nullptr;
            } else {
                using Data = typename Lobe::Data;
                const bsdl::BsdfGlobals globals(
                    wo, Nf, Ngf, backfacing, path_roughness, outer_ior, lambda_0);
                return new (arena) Lobe(
                    static_cast<Lobe*>(arena), globals, *static_cast<const Data*>(data));
            }
        });
    }

    int                    id;
    std::string            name;
    std::vector<Parameter> parameters;
    size_t                 data_size;
    size_t                 lobe_size;
};

using BsdfRegistry = std::unordered_map<std::string, BsdfDescription>;

template <typename Lobe> struct LobeData : Lobe::Data {
};

template <typename... Types> struct AlignedStorageFor {
    static constexpr std::size_t size  = std::max({ sizeof(Types)... });
    static constexpr std::size_t align = std::max({ alignof(Types)... });

    using type = typename std::aligned_storage<size, align>::type;
};

template <typename> struct StorageForLobes;

template <typename... Lobes> struct StorageForLobes<std::tuple<Lobes...>> {
    using lobe_type = typename AlignedStorageFor<Lobes...>::type;
    using data_type = typename AlignedStorageFor<LobeData<Lobes>...>::type;
};

using BsdfDataStorage = typename StorageForLobes<BsdfLobes>::data_type;
using BsdfStorage     = typename StorageForLobes<BsdfLobes>::lobe_type;

template <typename Lobe> BsdfDescription register_bsdf(std::string name)
{
    using Data       = typename Lobe::Data;
    const auto entry = Lobe::template entry<Data>();

    BsdfDescription description;
    description.id        = VirtualBsdf::GET_ID<Lobe>();
    description.name      = std::move(name);
    description.lobe_size = sizeof(Lobe);
    description.data_size = sizeof(typename Lobe::Data);
    for (const bsdl::LobeParam& parameter : entry.params) {
        if (parameter.type == bsdl::ParamType::NONE)
            break;
        if (parameter.type == bsdl::ParamType::CLOSURE)
            continue;
        description.parameters.push_back(
            { parameter.key ? parameter.key : "", parameter });
    }
    std::stable_partition(
        description.parameters.begin(),
        description.parameters.end(),
        [](const Parameter& parameter) { return parameter.name.empty(); });
    return description;
}

template <typename Lobe> void add_bsdf(BsdfRegistry& registry)
{
    if constexpr (!IsGroupBsdf<Lobe>::value) {
        std::string name = std::string(Lobe::space()) + "::" + Lobe::name();
        registry.emplace(name, register_bsdf<Lobe>(name));
    }
}

template <typename Lobes> BsdfRegistry make_registry()
{
    BsdfRegistry registry;

    bsdl::for_each_type<Lobes>{ [&](auto type) {
        using Lobe = typename decltype(type)::type;
        add_bsdf<Lobe>(registry);
    } };

    return registry;
}
