#pragma once

#include <Imath/ImathVec.h>

#include <cstdint>

class Rng
{
  public:
    Rng(unsigned seed, int x, int y, int sample, int depth);

    // Return one point from a 3D Sobol sequence. The complete point is
    // digitally scrambled using the seed, pixel coordinates, and bounce depth;
    // the sample index selects the Sobol point.
    Imath::V3f next();

  private:
    std::uint32_t scramble_ = 0;
    std::uint32_t index_    = 0;
};
