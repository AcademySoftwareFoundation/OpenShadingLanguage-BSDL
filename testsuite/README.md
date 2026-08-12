# BSDL Test Suite

The test suite contains a compile/include test and manifest-driven image tests
for `bsdltool`.

Run the suite with `./build.sh --test`. Tests run sequentially, and each
`bsdltool` render uses all available hardware threads.

## Image-test manifest

[bsdltool_tests.txt](bsdltool_tests.txt) defines one image test per non-comment
line:

```text
name | bsdltool arguments
```

`name` is both the CTest test name and the basename of its reference image.
The arguments are passed to `bsdltool`; the harness automatically adds:

- `--resolution`, using `BSDLTOOL_TEST_RESOLUTION` from
  [CMakeLists.txt](CMakeLists.txt)
- `-o <build>/testsuite/results/<name>.png`
- `--threshold`, using `BSDLTOOL_TEST_DIFF_THRESHOLD` from
  [CMakeLists.txt](CMakeLists.txt)

For example:

```text
simple-diffuse | spi::basic_diffuse 0,0,1 1,1,1 0.3 0.0 -a 0:Nf --samples 8 --depth 1
```

The rendered output is compared with
[references](references)/`<name>.png` using normalized RGB RMSE. The threshold
is configured as `BSDLTOOL_TEST_DIFF_THRESHOLD` in
[CMakeLists.txt](CMakeLists.txt); the default is $0.01$. Render settings should
remain deterministic so reference comparisons are stable.

## Configure, build, and run

Configure with testing enabled, build, then run CTest:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run one named test with CTest's regular expression filter:

```sh
ctest --test-dir build -R '^simple-diffuse$' --output-on-failure
```

The build helper accepts the same filter as an optional argument:

```sh
./build.sh --test '^simple-diffuse$'
```

`--configure` can be combined with `--test`, for example:

```sh
./build.sh --configure --test '^simple-diffuse$'
```

## Update image references

To regenerate references for selected image tests, set
`BSDL_UPDATE_REFERENCES=1` for that CTest invocation:

```sh
BSDL_UPDATE_REFERENCES=1 ctest --test-dir build -R '^simple-diffuse$' --output-on-failure
```

To update all image tests listed in the manifest:

```sh
BSDL_UPDATE_REFERENCES=1 ctest --test-dir build --output-on-failure
```

Alternatively, configure `BSDL_UPDATE_REFERENCES=ON` with CMake. Disable that
option again for normal reference comparison runs.

Reference updates also regenerate the source-controlled
[TESTS.md](TESTS.md) overview, which lists every test and its reference image.

## Generate the image-test overview

Every image-test invocation generates [build/testsuite/TESTS.md](../build/testsuite/TESTS.md).
It has a row for every manifest test with a green pass, red failure, or gray
not-run marker, followed by the rendered image, copied reference, and (only
for failures) the amplified difference image. Difference PNGs are written to
`build/testsuite/results` only when their comparison fails.
