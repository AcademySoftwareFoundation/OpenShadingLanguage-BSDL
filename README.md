# BSDL

This is a companion repository to the [Open Shading Language (OSL)](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage)
project at the [Academy Software Foundation (ASWF)](https://www.aswf.io/).

BSDL is a production, GPU-friendly bidirectional scattering distribution
function (BSDF) library open sourced by Sony Pictures Imageworks. It provides
header-based BSDF lobe implementations for physically based shading, including
diffuse, microfacet, dielectric, thin-film, sheen, toon, hair, and MaterialX
models.

The library is designed for integration into renderers and shading systems:
the core lobe code is C++17 header-only, while generated lookup tables and the
spectral Jakob-Hanika data are built and linked by CMake.

## Repository layout

- [core](core) contains the BSDL headers, LUT generators, and installable
  `BSDL::BSDL` CMake target.
- [bsdltool](bsdltool) contains a command-line lobe inspector, renderer, and
  PNG image-diff utility.
- [testsuite](testsuite) contains manifest-driven CTest image regression tests
  and their reference images.
- [cmake](cmake) contains package configuration templates.

## Build and install

BSDL requires a C++17 compiler, CMake 3.20 or newer, Imath, and zlib.
The `BSDL::BSDL` target itself links Imath and the generated spectral LUT
library; zlib is only needed by `bsdltool` and the PNG tests.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix /desired/install/prefix
```

An installed consumer can use:

```cmake
find_package(BSDL CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE BSDL::BSDL)
```

The `BSDL::BSDL` target provides the source headers, generated LUT headers,
Imath, and the spectral LUT library.

## bsdltool

`bsdltool` reflects registered BSDF lobe parameters, renders a diagnostic
preview, and compares RGB PNG images. Render a lobe with:

```sh
./build/bsdltool/bsdltool render --bsdf 'spi::basic_diffuse(Nf, (1,1,1), 0.3, 0.0)' \
    --samples 64 --resolution 512 -o diffuse.png
```

Use `render --help` to list available lobes and options. The preview renderer
uses a perspective camera, a CSG-cut unit sphere, infinite cone lights, and an
optional checkerboard ground plane. `--threads N` caps renderer workers.

Compare a rendered image to a reference using normalized RGB RMSE:

```sh
./build/bsdltool/bsdltool diff reference.png result.png \
    --threshold 0.001 --scale 10 -o diff.png
```

`diff` exits with status 1 when RMSE exceeds the threshold. The optional output
is an amplified absolute per-channel difference image. See
[bsdltool/README.md](bsdltool/README.md) for the full command reference.

## Tests

Configure tests, build, and run the image regression suite:

```sh
./build.sh --test
```

Run a matching subset:

```sh
./build.sh --test '^metal$'
```

Update references for the selected tests while running CTest:

```sh
./build.sh --test '^metal$' --update
```

References can also be updated deliberately:

```sh
BSDL_UPDATE_REFERENCES=1 ctest --test-dir build --output-on-failure
```

The test run writes its visual status report to
`build/testsuite/TESTS.md`; reference updates also refresh the checked-in
[testsuite/TESTS.md](testsuite/TESTS.md) gallery. Further test workflow details
are in [testsuite/README.md](testsuite/README.md).
