# Pigment OFX

Pigment is a CPU OpenFX image-processing suite for controlled spatial treatment of
photographic information. Stage 1 contains **ChromaDiffusion**, which diffuses two
linear opponent-color axes independently of luminance. It does not apply a transfer
function, LUT, gamut mapping, or HDR clamp.

The bundle currently targets Apple Silicon and has been validated with Nuke 17 and
DaVinci Resolve Studio 21.
DetailCollapse, Mass Formation, and DensityVeil are intentionally not implemented in
Stage 1.

The core also contains a Stage 2 **research-only Rolling YAB Mass reference**. It is
not registered as an OFX effect: Mass Formation is reserved as an internal backend
for the future DetailCollapse effect.

## Build

Requirements are CMake 3.25+, a C++17 compiler, Git, and the macOS SDK. CMake fetches
the pinned OpenFX 1.5.1 source when `OPENFX_ROOT` is not provided.

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --build build --target sign-local
cmake --build build --target stage
```

The build plug-in is `build/Pigment.ofx.bundle`; the explicit staging target copies
it to `build/stage/Pigment.ofx.bundle`. To build entirely offline, point
`OPENFX_ROOT` at an OpenFX 1.5.1 checkout and disable fetching:

```sh
cmake -S . -B build \
  -DOPENFX_ROOT=/path/to/openfx \
  -DPIGMENT_FETCH_OPENFX=OFF
```

For a local install prefix:

```sh
cmake --install build --prefix "$HOME/Library/OFX/Plugins"
```

## Nuke validation

The smoke test checks discovery, parameter creation, odd-sized float RGB/RGBA renders,
identity, non-identity processing, and alpha output:

```sh
validation_dir=$(mktemp -d /tmp/pigment-nuke.XXXXXX)
OFX_PLUGIN_PATH="$PWD/build" \
PIGMENT_NUKE_OUTPUT="$validation_dir" \
/Applications/Nuke17.0v1/Nuke17.0v1.app/Contents/MacOS/Nuke17.0 \
  -t tests/nuke_validate.py
```

## Resolve validation

Install to a user-local OFX path, restart Resolve with that path enabled, then add
ChromaDiffusion from the Color page's **Pigment** category:

```sh
cmake --install build --prefix "$HOME/Library/OFX/Plugins"
open -na "DaVinci Resolve" --env OFX_PLUGIN_PATH="$HOME/Library/OFX/Plugins"
```

Resolve 21.1 has been checked for registry discovery, effect instantiation, parameter
exposure, MediaIn/MediaOut connection, and a non-identity float render.

## Architecture

`pigment_core` has no OpenFX dependency. Images and scalar fields are strided views
with explicit coordinate bounds. Color conversion is represented by a replaceable
opponent transform. Masks, tonal weights, similarity metrics, and boundary
permeability are independent utilities.

Spatial algorithms implement `SpatialOperator`, report either a finite halo or a
full-region input requirement, and consume generic YAB planes plus independent
processing-strength and boundary-permeability fields. Either field can be constant
or per-pixel. ChromaDiffusion uses `DirectionalGaussianOperator`; the
interface does not assume convolution or separability and can later host region-aware
Mass Formation without changing the OFX wrappers.

## Mass Formation research harness

`pigment_mass_research` generates a synthetic HDR YAB scene containing a broad
silhouette and small high-contrast material detail. It reports fine-detail energy
before and after consolidation. Passing an output directory also writes PFM reference
images:

```sh
./build/pigment_mass_research \
  --mass-scale 4 \
  --structure-scale 5 \
  --output /tmp/pigment-mass-reference
```

The reference deliberately keeps `Mass Scale` separate from `Structure Scale`.
`buildStructureBoundaryField` simplifies its guide at Structure Scale before measuring
significant boundaries, so raw edge magnitude alone does not decide what survives.
The direct joint-bilateral rolling implementation requests the full source RoD and
prioritizes correctness and visual evaluation over production performance.
See [the Mass Formation research note](docs/MassFormationResearch.md) for the fixed
architecture, literature basis, and remaining prototype order.

## Color and alpha behavior

- Working-gamut selection changes only explicit RGB/XYZ/YAB matrices.
- ACEScg uses its D60 native white; the D65 spaces use their native D65 white.
- At 100% Luminance Preservation, the original Y plane is retained.
- RGB values are never clipped, including negative and HDR values.
- Premultiplied clips are processed in straight RGB and returned to their original
  representation. Alpha is copied exactly.
- `Amount = 0` and `Mix = 0` are direct-copy identity paths.
