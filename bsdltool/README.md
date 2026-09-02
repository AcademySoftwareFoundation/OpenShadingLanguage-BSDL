# BSDL Tool

This command-line helper imports BSDL BSDFs and renders them on a unit sphere.
It uses each lobe's `entry()` metadata to populate its `Data` struct from the
command line, then constructs and evaluates the selected lobe through BSDL's
static dispatch mechanism.

The renderer has a perspective camera on the positive Z axis looking at the
origin. It path traces a unit sphere with a hardcoded spherical CSG bite,
samples every infinite cone light directly,
and combines light- and BSDF-sampled environment contributions with power-
heuristic MIS. Russian roulette starts after the third bounce. Sampling uses a
deterministically scrambled base-2 low-discrepancy sequence keyed by pixel,
sample, bounce, and seed, and rendering uses automatic CPU threading. PNG
output is tone mapped with the ACES fitted curve and encoded as standard,
zlib-compressed RGB PNG.

For example:
```
$ bsdltool render --bsdf 'spi::basic_diffuse(Nf, (1,1,1), 0.3, 0.0)' \
	-o diffuse.png --resolution 512 --samples 64
```

`--bsdf` takes `NAME(ARGUMENT, ...)`. An argument can be an integer, float,
or parenthesized three-component vector. A symbol matching a `BsdfGlobals`
field—for example `Nf`, `v`, or `wo`—binds that lobe parameter to the
corresponding per-hit global. Its BSDL type must match the parameter type.
Float globals may include a remap range, such as `v[0.2:0.8]`, which maps a
global value $x$ to $0.2 + x(0.8 - 0.2)$. Remapping is only available for
float parameters.
Trailing parameters may be omitted and retain their zero-initialized value.

Use `bsdltool render --help` to list registered BSDFs and render options. Without a
light option, the tool uses a three-light studio setup. Add any number of
custom lights with:

```
-L X,Y,Z R,G,B angle_degrees intensity
```

The light direction is normalized internally and points from the shaded point
toward the light. A 180-degree light is visible to rays that miss the sphere,
so it can be used as a custom background. Additional options are `--depth`,
`--seed`, and `--exposure` in stops. Use `-o FILE` or `--output FILE` to choose
the PNG filename.

`--threads N` caps rendering at `N` worker threads. By default, `bsdltool` uses
all available hardware threads.

Use `bsdltool diff REFERENCE.png RESULT.png` to compare two RGB PNGs. It prints
their normalized RGB root mean squared error (RMSE) and exits with status 1 when
it exceeds `--threshold` (default $10^{-2}$). `-o DIFF.png` writes an absolute
per-channel error image; `--scale N` controls its visibility gain (default 10).

`--depth` is the maximum number of indirect BSDF continuations; `--depth 0`
renders only the camera-hit direct illumination.

Use `--noshadow` to disable direct-light occlusion rays. This is useful for
furnace tests, where all directions should receive the environment light.

Use `--ground COLOR1 COLOR2 SCALE` to add an infinite plane at $y=-1$, directly
under the unit sphere. The plane is shaded with `spi::diffuse` and a checkerboard
whose colors are `COLOR1` and `COLOR2`; `SCALE` controls the number of checks
per world-space unit.
