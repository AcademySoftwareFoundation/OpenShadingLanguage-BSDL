// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#include "../bsdltool/png.h"

#include <zlib.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

namespace
{

void check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void set_u32(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value)
{
    for (int i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<unsigned char>(value >> (24 - 8 * i));
}

void test_png(const std::string& path)
{
    const std::vector<unsigned char> rgb = { 0,   1,   255, 254, 128, 16,  32, 64, 96,
                                             127, 129, 0,   255, 0,   255, 10, 20, 30 };
    const int dimensions[][2]            = { { 3, 2 }, { 2, 3 }, { 1, 6 }, { 6, 1 } };
    std::vector<unsigned char> decoded;
    int                        width = 0, height = 0;
    for (const auto& size : dimensions) {
        check(write_png_rgb(path, rgb, size[0], size[1]), "RGB PNG write failed");
        check(read_png_rgb(path, decoded, width, height), "RGB PNG read failed");
        check(width == size[0] && height == size[1], "RGB dimensions changed");
        check(decoded == rgb, "RGB pixels changed across a round trip");
    }

    const std::vector<Imath::C3f>    linear   = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 },
                                                  { 1, 1, 0 }, { 0, 1, 1 }, { 0, 0, 0 } };
    const std::vector<unsigned char> expected = { 232, 0,   0, 0, 232, 0,   0, 0, 232,
                                                  232, 232, 0, 0, 232, 232, 0, 0, 0 };
    check(write_png(path, linear, 3, 2, 0), "Linear PNG write failed");
    check(read_png_rgb(path, decoded, width, height), "Linear PNG read failed");
    check(width == 3 && height == 2 && decoded == expected,
          "Linear-to-RGB conversion or pixel order changed");

    check(!write_png_rgb(path, rgb, 2, 2), "Accepted an RGB buffer size mismatch");
    check(!write_png(path, linear, 2, 2, 0), "Accepted a linear buffer size mismatch");
    const int max_dimension           = std::numeric_limits<int>::max();
    const int invalid_dimensions[][2] = {
        { 0, 1 }, { 1, 0 }, { -1, 1 }, { 1, -1 }, { max_dimension, max_dimension }
    };
    for (const auto& size : invalid_dimensions) {
        check(!write_png_rgb(path, {}, size[0], size[1]), "Accepted invalid RGB size");
        check(!write_png(path, {}, size[0], size[1], 0), "Accepted invalid linear size");
    }
    // These products used to overflow signed int to zero before the size_t cast.
    check(!write_png_rgb(path, {}, 65536, 65536), "RGB size calculation wrapped");
    check(!write_png(path, {}, 65536, 65536, 0), "Linear size calculation wrapped");

    // Mutate a small PNG's IHDR (including its CRC) without allocating a huge image.
    check(write_png_rgb(path, { 1, 2, 3 }, 1, 1), "Fixture write failed");
    std::ifstream              input(path, std::ios::binary);
    std::vector<unsigned char> png{ std::istreambuf_iterator<char>(input),
                                    std::istreambuf_iterator<char>() };
    input.close();
    check(png.size() >= 33, "Could not read PNG fixture");
    const std::uint32_t max_u32              = std::numeric_limits<std::uint32_t>::max();
    const std::uint32_t huge                 = static_cast<std::uint32_t>(max_dimension);
    const std::uint32_t invalid_headers[][2] = {
        { 0, 1 }, { 1, 0 }, { max_u32, 1 }, { 1, max_u32 }, { huge, huge }
    };
    for (const auto& size : invalid_headers) {
        set_u32(png, 16, size[0]);
        set_u32(png, 20, size[1]);
        set_u32(png, 29, crc32(0, png.data() + 12, 17));
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(png.data()), png.size());
        output.close();
        check(bool(output), "Could not write invalid-size PNG fixture");
        check(!read_png_rgb(path, decoded, width, height), "Accepted invalid PNG size");
    }
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2)
        return 2;
    try {
        test_png(argv[1]);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        std::remove(argv[1]);
        return 1;
    }
    std::remove(argv[1]);
    return 0;
}
