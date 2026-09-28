# Pigment OFX

Pigment is an OpenFX image-processing suite for controlled spatial treatment of
photographic information. The bundle contains the production Stage 1
**ChromaDiffusion** effect and a temporary **DetailCollapse (Research)** node used to
evaluate the Stage 2 Rolling YAB Mass reference. Neither path applies a transfer
function, LUT, gamut mapping, or HDR clamp.

The bundle currently targets Apple Silicon. DetailCollapse exposes the preserved
bilateral reference, Guided CPU, Domain Transform CPU, Guided Metal, and a reserved
Domain Transform Metal choice. It remains a research interface rather than the final
DetailCollapse UI. Mass Formation is still an internal mode of that node, not a
separate public effect. DensityVeil is not implemented.

## Build

Requirements are CMake 3.25+, Xcode's C++/Metal toolchains, Git, and the macOS SDK. CMake fetches
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

Nuke and Resolve scan `/Library/OFX/Plugins` by default on the validation machine:

```sh
cmake --install build --prefix /Library/OFX/Plugins
```

The repeatable host installation, node-discovery, troubleshooting, and future factory
registration workflow is documented in
[`docs/HostNodeExposure.md`](docs/HostNodeExposure.md).

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

The interactive Stage 2 validation scene is
[`tests/visual/DetailCollapseValidation.nk`](tests/visual/DetailCollapseValidation.nk).
It uses the normal `Pigment.ofx.bundle`, not a gizmo. Install the bundle in a standard
system OFX location, restart Nuke, and create **DetailCollapse** from Tab search. The
automated visual-reference render can be repeated with:

```sh
/Applications/Nuke17.0v1/Nuke17.0v1.app/Contents/MacOS/Nuke17.0 \
  -t tests/nuke_detailcollapse_validate.py
```

## Resolve validation

Install to the system OFX path and restart Resolve. Both Pigment identifiers should
appear; DetailCollapse remains a development node:

```sh
cmake --install build --prefix /Library/OFX/Plugins
open -na "DaVinci Resolve"
```

Resolve Studio 21 discovers `org.painterlyofx.DetailCollapse`. Native Metal rendering
is still a host-validation item; see the performance report before using this research
backend for production work.

## Architecture

`pigment_core` has no OpenFX dependency. Images and scalar fields are strided views
with explicit coordinate bounds. Color conversion is represented by a replaceable
opponent transform. Masks, tonal weights, similarity metrics, and boundary
permeability are independent utilities.

Spatial algorithms implement `SpatialOperator`, report either a finite halo or a
full-region input requirement, and consume generic YAB planes plus independent
processing-strength and boundary-permeability fields. Either field can be constant
or per-pixel. ChromaDiffusion uses `DirectionalGaussianOperator`; Rolling YAB Mass is
a full-region, region-aware operator. The interface assumes neither convolution nor
separability.

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
remains the immutable visual reference. The constant-time CPU alternatives and Guided
Metal use the same fields, scale separation, reintegration, and debug-view contract.
See [the Mass Formation research note](docs/MassFormationResearch.md) for the fixed
architecture, literature basis, and remaining prototype order.

## Performance harnesses

`pigment_backend_benchmark` compares Reference, Guided CPU, and Domain Transform CPU.
`pigment_metal_harness` measures no-copy/staged transfers, GPU duration, stage encoding,
allocation reuse, and CPU/Metal parity. For example:

```sh
./build/pigment_backend_benchmark --backend guided --width 1920 --height 1080 --mass-scale 8
PIGMENT_METAL_RESOURCE_DIR="$PWD/build" \
  ./build/pigment_metal_harness --width 1920 --height 1080 --mass-scale 8 --verify
```

## Color and alpha behavior

- Working-gamut selection changes only explicit RGB/XYZ/YAB matrices.
- ACEScg uses its D60 native white; the D65 spaces use their native D65 white.
- At 100% Luminance Preservation, the original Y plane is retained.
- RGB values are never clipped, including negative and HDR values.
- Premultiplied clips are processed in straight RGB and returned to their original
  representation. Alpha is copied exactly.
- `Amount = 0` and `Mix = 0` are direct-copy identity paths.
