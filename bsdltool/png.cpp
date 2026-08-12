#include "png.h"

#include <zlib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{

void append_u32(std::vector<unsigned char>& bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<unsigned char>(value >> 24));
    bytes.push_back(static_cast<unsigned char>(value >> 16));
    bytes.push_back(static_cast<unsigned char>(value >> 8));
    bytes.push_back(static_cast<unsigned char>(value));
}

std::uint32_t read_u32(const unsigned char* bytes)
{
    return (std::uint32_t(bytes[0]) << 24) | (std::uint32_t(bytes[1]) << 16) |
           (std::uint32_t(bytes[2]) << 8) | bytes[3];
}

unsigned char paeth(unsigned char a, unsigned char b, unsigned char c)
{
    const int p  = int(a) + int(b) - int(c);
    const int pa = std::abs(p - int(a));
    const int pb = std::abs(p - int(b));
    const int pc = std::abs(p - int(c));
    return pa <= pb && pa <= pc ? a : (pb <= pc ? b : c);
}

void append_chunk(std::vector<unsigned char>&       bytes,
                  const char*                       type,
                  const std::vector<unsigned char>& data)
{
    append_u32(bytes, static_cast<std::uint32_t>(data.size()));
    const std::size_t begin = bytes.size();
    bytes.insert(bytes.end(), type, type + 4);
    bytes.insert(bytes.end(), data.begin(), data.end());
    append_u32(bytes,
               crc32(0, bytes.data() + begin, static_cast<uInt>(bytes.size() - begin)));
}

float aces_fitted(float value)
{
    value = std::max(0.0f, value);
    return std::clamp((value * (2.51f * value + 0.03f)) /
                          (value * (2.43f * value + 0.59f) + 0.14f),
                      0.0f,
                      1.0f);
}

unsigned char linear_to_srgb(float value)
{
    value = aces_fitted(value);
    value = value <= 0.0031308f ? 12.92f * value
                                : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    return static_cast<unsigned char>(
        std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

} // namespace

bool write_png(const std::string&             path,
               const std::vector<Imath::C3f>& image,
               int                            width,
               int                            height,
               float                          exposure)
{
    if (width <= 0 || height <= 0 ||
        image.size() != static_cast<std::size_t>(width * height))
        return false;

    const float                scale = std::exp2(exposure);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width * height * 3));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Imath::C3f color =
                image[static_cast<std::size_t>(y * width + x)] * scale;
            unsigned char* pixel = pixels.data() + static_cast<std::size_t>(3 * (y * width + x));
            pixel[0] = linear_to_srgb(color.x);
            pixel[1] = linear_to_srgb(color.y);
            pixel[2] = linear_to_srgb(color.z);
        }
    }
    return write_png_rgb(path, pixels, width, height);
}

bool write_png_rgb(const std::string&                path,
                   const std::vector<unsigned char>& pixels,
                   int                               width,
                   int                               height)
{
    if (width <= 0 || height <= 0 ||
        pixels.size() != static_cast<std::size_t>(width * height * 3))
        return false;

    std::vector<unsigned char> raw(static_cast<std::size_t>(height) * (1 + 3 * width));
    for (int y = 0; y < height; ++y) {
        unsigned char* row = raw.data() + static_cast<std::size_t>(y) * (1 + 3 * width);
        row[0]             = 0;
        std::copy_n(
            pixels.data() + static_cast<std::size_t>(3 * y * width), 3 * width, row + 1);
    }

    uLongf                     compressed_size = compressBound(raw.size());
    std::vector<unsigned char> compressed(compressed_size);
    if (compress2(
            compressed.data(), &compressed_size, raw.data(), raw.size(), Z_BEST_SPEED) !=
        Z_OK)
        return false;
    compressed.resize(compressed_size);

    std::vector<unsigned char> png = { 137, 80, 78, 71, 13, 10, 26, 10 };
    std::vector<unsigned char> ihdr;
    append_u32(ihdr, static_cast<std::uint32_t>(width));
    append_u32(ihdr, static_cast<std::uint32_t>(height));
    ihdr.insert(ihdr.end(), { 8, 2, 0, 0, 0 });
    append_chunk(png, "IHDR", ihdr);
    append_chunk(png, "IDAT", compressed);
    append_chunk(png, "IEND", {});

    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file)
        return false;
    const bool written = std::fwrite(png.data(), 1, png.size(), file) == png.size();
    return std::fclose(file) == 0 && written;
}

bool read_png_rgb(const std::string&          path,
                  std::vector<unsigned char>& pixels,
                  int&                        width,
                  int&                        height)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return false;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::rewind(file);
    std::vector<unsigned char> png(size > 0 ? static_cast<std::size_t>(size) : 0);
    const bool                 read =
        size > 0 && std::fread(png.data(), 1, png.size(), file) == png.size();
    std::fclose(file);
    if (!read || png.size() < 33 ||
        !std::equal(
            png.begin(),
            png.begin() + 8,
            std::initializer_list<unsigned char>{ 137, 80, 78, 71, 13, 10, 26, 10 }
                .begin()))
        return false;

    std::vector<unsigned char> compressed;
    std::size_t                offset = 8;
    width = height = 0;
    while (offset + 12 <= png.size()) {
        const std::uint32_t length = read_u32(png.data() + offset);
        if (offset + 12ull + length > png.size())
            return false;
        const unsigned char* type = png.data() + offset + 4;
        const unsigned char* data = type + 4;
        if (std::equal(type, type + 4, "IHDR")) {
            if (length != 13 || data[8] != 8 || data[9] != 2 || data[10] || data[11] ||
                data[12])
                return false;
            width  = static_cast<int>(read_u32(data));
            height = static_cast<int>(read_u32(data + 4));
        } else if (std::equal(type, type + 4, "IDAT"))
            compressed.insert(compressed.end(), data, data + length);
        else if (std::equal(type, type + 4, "IEND"))
            break;
        offset += 12 + length;
    }
    if (width <= 0 || height <= 0 || compressed.empty())
        return false;
    const std::size_t          stride = static_cast<std::size_t>(3 * width);
    std::vector<unsigned char> raw(static_cast<std::size_t>(height) * (stride + 1));
    uLongf                     raw_size = raw.size();
    if (uncompress(raw.data(), &raw_size, compressed.data(), compressed.size()) != Z_OK ||
        raw_size != raw.size())
        return false;

    pixels.resize(static_cast<std::size_t>(width * height * 3));
    for (int y = 0; y < height; ++y) {
        const unsigned char* source =
            raw.data() + static_cast<std::size_t>(y) * (stride + 1);
        unsigned char* destination = pixels.data() + static_cast<std::size_t>(y) * stride;
        const unsigned char* previous = y == 0 ? nullptr : destination - stride;
        for (std::size_t x = 0; x < stride; ++x) {
            const unsigned char left    = x < 3 ? 0 : destination[x - 3];
            const unsigned char up      = previous ? previous[x] : 0;
            const unsigned char up_left = previous && x >= 3 ? previous[x - 3] : 0;
            switch (source[0]) {
                case 0: destination[x] = source[x + 1]; break;
                case 1: destination[x] = source[x + 1] + left; break;
                case 2: destination[x] = source[x + 1] + up; break;
                case 3:
                    destination[x] = source[x + 1] + static_cast<unsigned char>(
                                                         (int(left) + int(up)) / 2);
                    break;
                case 4: destination[x] = source[x + 1] + paeth(left, up, up_left); break;
                default: return false;
            }
        }
    }
    return true;
}
