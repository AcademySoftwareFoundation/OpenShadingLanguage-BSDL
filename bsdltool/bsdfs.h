#pragma once

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
                                        bsdl::mtx::TranslucentLobe<Bsdf>>;

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

    bsdl::Sample eval(const Imath::V3f& wo, const Imath::V3f& wi) const
    {
        const auto*  lobe   = static_cast<const bsdl::Lobe<Bsdf>*>(this);
        bsdl::Sample sample = dispatch([&](const auto& object) {
            return evaluate(object, lobe->frame.local(wo), lobe->frame.local(wi));
        });
        sample.weight       = bsdl::Power(weight_, 0);
        return sample;
    }

    bsdl::Sample sample(const Imath::V3f& wo, const Imath::V3f& random) const
    {
        const auto*  lobe   = static_cast<const bsdl::Lobe<Bsdf>*>(this);
        bsdl::Sample sample = dispatch([&](const auto& object) {
            return sample_impl(object, lobe->frame.local(wo), random);
        });
        sample.wi           = lobe->frame.world(sample.wi).normalized();
        sample.weight       = bsdl::Power(weight_, 0);
        return sample;
    }

  private:
    template <typename Lobe, typename = void>
    struct HasExtendedEvaluation : std::false_type {
    };

    template <typename Lobe>
    struct HasExtendedEvaluation<
        Lobe,
        std::void_t<decltype(std::declval<const Lobe>().eval_impl(
            std::declval<Imath::V3f>(),
            std::declval<Imath::V3f>(),
            true,
            true))>> : std::true_type {
    };

    template <typename Lobe>
    static bsdl::Sample
    evaluate(const Lobe& lobe, const Imath::V3f& wo, const Imath::V3f& wi)
    {
        if constexpr (HasExtendedEvaluation<Lobe>::value)
            return lobe.eval_impl(wo, wi, true, true);
        else
            return lobe.eval_impl(wo, wi);
    }

    template <typename Lobe>
    static bsdl::Sample
    sample_impl(const Lobe& lobe, const Imath::V3f& wo, const Imath::V3f& random)
    {
        if constexpr (HasExtendedEvaluation<Lobe>::value)
            return lobe.sample_impl(wo, random, true, true);
        else
            return lobe.sample_impl(wo, random);
    }

    float      roughness_ = 0;
    Imath::C3f weight_    = { 1, 1, 1 };
};

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

inline bool parse_vector(std::string_view text, Imath::V3f& value)
{
    float components[3];
    for (int i = 0; i < 3; ++i) {
        const std::size_t separator = text.find(',');
        if (!parse_float(text.substr(0, separator), components[i]))
            return false;
        if (i < 2 && separator == std::string_view::npos)
            return false;
        text = separator == std::string_view::npos ? std::string_view()
                                                   : text.substr(separator + 1);
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
    int create_data(void* data, const std::vector<std::string_view>& values) const
    {
        memset(data, 0, data_size);
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (!assign_parameter(data, parameters[i].lobe_parameter, values[i])) {
                const std::string& name = parameters[i].name;
                std::fprintf(stderr,
                             "Invalid value for parameter %s%zu\n",
                             name.empty() ? "#" : name.c_str(),
                             name.empty() ? i : 0);
                return 2;
            }
        }

        return 0;
    }
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
            using Data = typename Lobe::Data;
            const bsdl::BsdfGlobals globals(
                wo, Nf, Ngf, backfacing, path_roughness, outer_ior, lambda_0);
            return new (arena)
                Lobe(static_cast<Lobe*>(arena), globals, *static_cast<const Data*>(data));
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
    std::string name = std::string(Lobe::space()) + "::" + Lobe::name();
    registry.emplace(name, register_bsdf<Lobe>(name));
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
