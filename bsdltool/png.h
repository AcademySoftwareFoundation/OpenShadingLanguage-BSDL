// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <Imath/ImathColor.h>

#include <string>
#include <vector>

bool write_png(const std::string&             path,
               const std::vector<Imath::C3f>& image,
               int                            width,
               int                            height,
               float                          exposure);

bool write_png_rgb(const std::string&                path,
                   const std::vector<unsigned char>& pixels,
                   int                               width,
                   int                               height);

bool read_png_rgb(const std::string&          path,
                  std::vector<unsigned char>& pixels,
                  int&                        width,
                  int&                        height);
