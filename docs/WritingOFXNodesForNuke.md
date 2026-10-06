# Writing OFX Nodes for Nuke

A practical development guide, using this Pigment repository as a working example.
Written 2026-10-03. The build examples target macOS / Apple Silicon; the core OFX
design applies to other platforms too. Check the installed Nuke version's support
and licensing before relying on a particular GPU extension.

## 1. What you are building

An OFX node is a compiled image-effect plug-in. Nuke loads its binary, asks it to
describe its inputs and parameters, creates effect instances, and requests renders.

You generally write the renderer and host wrapper in C++. Python is useful for
Nuke validation scripts, not as the replacement for the compiled OFX entry points.
A native OFX effect does not need a gizmo, `menu.py`, or `init.py` to become a node.

Choose OFX when you want a cross-host image effect. Choose Nuke's native NDK when
you specifically need Nuke-only capabilities outside the OFX contract. A Python
gizmo/group is a third option for combining existing nodes rather than implementing
a native pixel processor.

The clean architecture is:

```text
Nuke / OFX wrapper
    parameters, clips, image layouts, host actions, errors
        ↓
Host-independent CPU renderer
    math, image processing, deterministic tests
        ↓ optional
GPU implementation
    same formulation, explicit capability and fallback policy
```

Keep the algorithm out of the host wrapper wherever possible. You should be able
to test the math without opening Nuke.

## 2. Start with a small effect

Your first node should do one simple thing: copy its input, apply an exposure gain,
or scale saturation. Do not start with spectral analysis, multiple backends, or a
complicated UI.

For example, an exposure effect can define:

```cpp
const float gain = std::exp2(exposureStops);
out.r = in.r * gain;
out.g = in.g * gain;
out.b = in.b * gain;
out.a = in.a;
```

This preserves finite negative/HDR RGB rather than clipping to `[0, 1]`. For this
particular operation, multiplying premultiplied RGB by gain is equivalent to
unpremultiplying, applying gain, and premultiplying again. That equivalence does
not hold for arbitrary nonlinear color operations.

First prove discovery, image copying, parameter animation, alpha handling and
save/reload. Then add the interesting algorithm.

## 3. Use the OFX C++ support library

The OFX ABI is a C interface. The SDK's C++ support library wraps its actions in
classes such as `OFX::ImageEffect`, `OFX::Clip`, and parameter descriptors.
Pigment uses that support library rather than manually dispatching every action.

Pin the SDK revision in your build. Avoid depending on whatever happens to be on
the SDK's default branch.

Useful examples in this repository:

| File | What it demonstrates |
| --- | --- |
| `src/plugins/ChromaDiffusion.cpp` | Smaller host wrapper to learn from first |
| `src/plugins/ChromaDiffusion.h` | Factory declaration |
| `src/plugins/Pigment.cpp` | Advanced UI, CPU/GPU routing and cached research rendering |
| `src/ofx/PluginMain.cpp` | Registration of multiple effect factories |
| `src/ofx/OfxImageHelpers.cpp` | Host storage converted into strided image views |
| `src/ofx/ParameterHelpers.cpp` | Reusable parameter-descriptor helpers |
| `CMakeLists.txt` | SDK, module, bundle, resources and local signing targets |
| `tests/nuke_validate.py` | Discovery and real host-render smoke test |

## 4. Understand the effect lifecycle

The main wrapper responsibilities are:

| Support-library callback | Responsibility |
| --- | --- |
| `describe()` | Labels, grouping and actual supported capabilities |
| `describeInContext()` | Input/output clips and persistent parameter definitions |
| Factory `createInstance()` | Construct the effect object |
| Effect constructor | Fetch clips and parameters; initialize per-instance state |
| `render()` | Evaluate parameters at render time and produce requested pixels |
| `isIdentity()` | Tell the host when the output is exactly an input |
| `getRegionOfDefinition()` | Describe where the effect can produce pixels |
| `getRegionsOfInterest()` | Describe which input regions it needs |
| `changedParam()` / `changedClip()` | UI updates and relevant state invalidation |
| Destruction / purge hooks | Release caches and other resources |

Do not perform expensive image analysis in descriptor callbacks. They describe a
plug-in, not a particular source image. Read animated parameter values at the time
specified by the render request, not just once when the node is created.

The precise action contracts are documented in the
[OpenFX image-effect actions reference](https://openfx.readthedocs.io/en/main/Reference/ofxImageEffectActions.html).

## 5. Stable identity, registration and compatibility

Give your node a permanent reverse-domain identifier, for example:

```text
com.yourstudio.Exposure
```

The identifier is not the visible label. Renaming the label should not create a
different effect identity. Parameter names are also persistent identifiers: labels
can be polished, but casually renaming stored parameter IDs can break old scripts.

Pigment registers factories in `src/ofx/PluginMain.cpp`:

```cpp
namespace OFX::Plugin {
void getPluginIDs(OFX::PluginFactoryArray& ids) {
    static MyExposureFactory factory("com.yourstudio.Exposure", 1, 0);
    ids.push_back(&factory);
}
}
```

This is a registration pattern, not a complete standalone source file: the factory
class and its callbacks must also be implemented and compiled into the module.

The support library supplies the exported `OfxGetNumberOfPlugins` and `OfxGetPlugin`
functions. Pigment's export list is `cmake/Pigment.exports`. Do not accidentally
hide those symbols when enabling hidden visibility for other C++ symbols.

When adding a new effect here, add its factory to registration and its `.cpp` file
to the OFX module in CMake. Adding a source file alone does not expose a node.

For choice parameters, preserve existing option indices in saved scripts. Append
new options instead of reordering old ones. Keep legacy interpretation intact or
provide an explicit versioned migration.

## 6. Declare only capabilities you actually implement

For a first effect, support float RGB/RGBA and CPU rendering. Add other depths,
components and GPU support only when they work.

A typical descriptor pattern is:

```cpp
void MyExposureFactory::describe(OFX::ImageEffectDescriptor& d) {
    d.setLabels("My Exposure", "My Exposure", "My Exposure");
    d.setPluginGrouping("My Studio");
    d.addSupportedContext(OFX::eContextFilter);
    d.addSupportedContext(OFX::eContextGeneral);
    d.addSupportedBitDepth(OFX::eBitDepthFloat);
    d.setSupportsTiles(false); // A simple first implementation, not a speed claim.
    d.setSupportsMultiResolution(true); // Only if proxy/render-scale handling works.
}
```

Filter context is appropriate for a single-input effect. General context supports
more flexible input arrangements, including optional masks. Define the clips
required by every context you advertise; Nuke may select General context.
See [OpenFX contexts](https://openfx.readthedocs.io/en/main/Reference/ofxImageEffectContexts.html).

Thread-safety declarations are promises. Do not copy `eRenderFullySafe` from another
node unless concurrent calls on the same instance are safe, including its caches.
Do not advertise temporal access if you only process the current frame.

## 7. Define inputs and live controls

Use standard Source and Output clip names:

```cpp
auto* source = d.defineClip(kOfxImageEffectSimpleSourceClipName);
source->addSupportedComponent(OFX::ePixelComponentRGB);
source->addSupportedComponent(OFX::ePixelComponentRGBA);

auto* output = d.defineClip(kOfxImageEffectOutputClipName);
output->addSupportedComponent(OFX::ePixelComponentRGB);
output->addSupportedComponent(OFX::ePixelComponentRGBA);

auto* exposure = d.defineDoubleParam("exposureStops");
exposure->setLabels("Exposure", "Exposure", "Exposure");
exposure->setDefault(0.0);
exposure->setRange(-20.0, 20.0);
exposure->setDisplayRange(-5.0, 5.0);
exposure->setHint("Scene-linear exposure in stops; alpha is unchanged.");
```

In the instance, fetch the same parameter name and evaluate it for `args.time`:

```cpp
double stops = 0.0;
exposure_->getValueAtTime(args.time, stops);
```

If an input is scalar data, such as a mask or packed ownership map, do not treat it
as ordinary RGB color. Define its components, optional status and validation rules
explicitly. Fetch optional inputs only when the selected operation needs them.

### Design the visible UI, not just the parameter list

A parameter existing internally does not prove that an artist can find or use it.

- Put the normal artist interface first and make its defaults useful.
- Put backend details and debug controls in a clearly named advanced/research layer.
- Use one plate/channel editor with a selector instead of repeating large panels.
- Keep hidden per-plate values persistent; visible proxy controls must edit the
  correct stored values, including animation at the relevant time.
- Guard proxy synchronization against recursive change callbacks.
- Refresh visibility on creation, script load and interface changes.
- Keep choice ordering stable when varying the active plate count; hide/disable
  invalid selection behavior without changing the meaning of saved indices.

Test creation and save/reload in the actual Properties panel. UI enumeration scripts
are supporting tests, not visual certification.

## 8. Render host images correctly

Inside `render()`, the safe sequence is:

1. Evaluate parameters at `args.time`.
2. Fetch the required Source and Output images using RAII ownership.
3. Validate image availability, depth, components, bounds and processing backend.
4. Build views carrying bounds, component count and signed row stride.
5. Process only the requested output render window.
6. Check cancellation during long work.
7. Let image ownership release on every return/error path.

For a basic CPU loop, `image.getPixelAddress(x, y)` avoids assuming a contiguous
zero-origin allocation. A missing source pixel needs a defined policy, usually
transparent black outside its bounds. Do not dereference a null pixel pointer.

For a faster inner loop, use strided views, but account for:

- nonzero or negative image origins;
- padded rows and potentially negative row strides;
- RGB versus RGBA component count;
- source/output bounds that differ;
- requested render windows smaller than the available image;
- render scale and pixel aspect ratio;
- whether storage is CPU memory or a negotiated native GPU resource.

Never index a host image as `((y * width) + x) * 4` without proving those assumptions.
Never retain fetched host-image pointers after the image handle has been released.

### Color and alpha policy

State your contract: for Pigment it is scene-linear RGB in → scene-linear RGB out.
No automatic display transform belongs inside that contract.

For nonlinear processing on premultiplied images, unpremultiply where alpha is
safely nonzero, process straight RGB, then premultiply. Define zero-alpha behavior
explicitly; hidden RGB can matter. Preserve alpha exactly when the effect is not
supposed to change coverage. Do not silently clip negative RGB or HDR values.

Validate an exact-copy path separately from the processed path. Avoid converting
RGB into another color space and back merely to produce an identity result.

## 9. Bounds, ROI, proxies and identity

RoD is the effect's region of definition. ROI is the input region requested to
compute an output region. A local pointwise effect usually needs the same input
region. A neighborhood operator needs a halo; an image-global analysis may need
the full Source RoD.

OFX canonical rectangles and image pixel rectangles are not interchangeable.
Convert consistently using render scale and PAR, including at proxies. Define
whether a radius is measured in canonical pixels or current rendered pixels.

If your first global implementation disables tiling, do not pretend it can analyze
arbitrary independent tiles correctly. Still respect the output render window.
The [OFX processing architecture reference](https://openfx.readthedocs.io/en/latest/Reference/ofxProcessingArch.html)
explains these region relationships.

For `isIdentity()`, return Source only when *every* output pixel is exactly unchanged.
Typical valid cases are Amount = 0 or Mix = 0. A debug output can invalidate an
otherwise valid identity claim. Keep a safe copy branch in `render()` too; the host
is not obliged to avoid rendering just because an identity optimization exists.

## 10. Build and package the plug-in

For the existing Pigment project, from the repository root:

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --build build --target sign-local
```

These are this repository's commands, not universal target names for all OFX projects.
For your own project, build a CMake MODULE, compile/link the SDK support library and
your factory registration, and package the result in the platform's OFX bundle layout.

Pigment's macOS bundle is:

```text
Pigment.ofx.bundle/
  Contents/
    Info.plist
    MacOS/
      Pigment.ofx
    Resources/
      Pigment.metallib
```

Match the binary architecture to the Nuke process, not merely the computer's CPU.
An arm64-only plug-in cannot load into an Intel/Rosetta Nuke process. Link dependent
libraries appropriately and inspect them with `otool -L` when diagnosing load errors.

Sign after assembling the executable and resources. This project's `sign-local`
target uses ad-hoc signing for local development; that is not a substitute for
distribution signing/notarization where required.

## 11. Load the real bundle in Nuke

Fully quit Nuke before replacing a loaded binary. Install the entire bundle,
including resources, not just the `.ofx` executable.

Pigment's system installation command is:

```sh
cmake --install build --prefix /Library/OFX/Plugins
```

It may require permission to write that directory. Do not replace a working
installation as part of a documentation-only or algorithm-only experiment.

For isolated development, launch Nuke with a build search path:

```sh
OFX_PLUGIN_PATH="$PWD/build" \
/Applications/Nuke17.0v1/Nuke17.0v1.app/Contents/MacOS/Nuke17.0
```

Adjust the executable path to your installed version. Avoid duplicate bundles with
the same identifiers in the effective search paths; otherwise you may test a stale
binary. Nuke scans OFX locations at startup and supports `OFX_PLUGIN_PATH`, as
documented by [Foundry](https://learn.foundry.com/nuke/content/comp_environment/configuring_nuke/loading_ofx_plugins.html).

Check the installed artifact independently:

```sh
file /Library/OFX/Plugins/Pigment.ofx.bundle/Contents/MacOS/Pigment.ofx
nm -gU /Library/OFX/Plugins/Pigment.ofx.bundle/Contents/MacOS/Pigment.ofx
codesign --verify --deep --strict /Library/OFX/Plugins/Pigment.ofx.bundle
```

Then create the node from Tab search, connect a source and Viewer, and render a
Write. Save the script, quit, reopen, and confirm identity, settings and animation
survive. Do not guess Nuke's generated OFX node-class string; discover it.

## 12. Automate host validation

Inside Nuke's Python environment you can inspect registration with:

```python
import nuke

matches = [name for name in nuke.nodeTypes(force_plugin_load=True)
           if "painterlyofx" in name.lower()]
print(nuke.ofxPluginPath())
print(matches)
```

Run the existing smoke test without polluting Git with generated images:

```sh
validation_dir=$(mktemp -d /tmp/pigment-nuke.XXXXXX)
OFX_PLUGIN_PATH="$PWD/build" \
PIGMENT_NUKE_OUTPUT="$validation_dir" \
/Applications/Nuke17.0v1/Nuke17.0v1.app/Contents/MacOS/Nuke17.0 \
  -t tests/nuke_validate.py
```

That particular script tests the simpler ChromaDiffusion effect; use the matching
Pigment validation scripts for the integrated node. Host rendering requires a
working license/configuration. If it is unavailable, report host validation as
unverified rather than substituting standalone success.

Test at least:

- float RGB and RGBA; straight and premultiplied sources;
- zero alpha, negative RGB and values above one;
- odd dimensions, unusual origins, cropped bounding boxes, PAR and proxies;
- exact bypass, Amount/Mix endpoints, optional mask and disconnected inputs;
- parameter animation and frame changes;
- save/reload and two instances with different settings;
- cancellation, concurrent rendering and cache purge;
- visible UI switching and every intended artist control;
- CPU/GPU parity when acceleration is enabled.

Render EXR in a suitable float format for numerical checks. PNG screenshots alone
cannot prove HDR/negative preservation or exact alpha behavior.

## 13. Make knob edits cheap through dependency-aware caching

Build a dependency graph, not one cache that invalidates on every parameter change:

```text
source analysis → plate/support construction → hierarchy → field
                                                         ↓
                          transport → color interaction → reconstruction/mix
```

Cache each stage against its actual inputs. Include source content/version, frame,
bounds, render scale, PAR, gamut and relevant parameter values where needed. Pointer
identity is not a reliable image-content identifier. Frame number alone is not
enough when an upstream node changes on the same frame.

Examples of sensible boundaries:

| Edit | Earliest stage normally needing rebuild |
| --- | --- |
| Amount / final Mix | Final reconstruction or blend |
| Color interaction law / density | Color interaction |
| Spill amount, with fixed reach/topology | Influence scaling / interaction |
| Spill reach or directed transport settings | Transport |
| AB support extent | AB support and its genuinely dependent stages |
| Y organization | Y hierarchy/field and dependent Y stages |
| Source pixels or working color space | Source analysis |

These are design examples, not permission to omit a real mathematical dependency.
Per-plate appearance overrides should not rediscover automatic plates. Track Y and
AB dependencies independently where the formulation permits it.

Keep caches per-instance or safely keyed/shared, memory-bounded, cancellable, and
purgeable. Publish only complete cache entries. Immutable intermediates are helpful
when Nuke requests renders concurrently.

Measure cold render, unchanged render and each artist knob separately on normal
production-size images. For our artist workflow, that means at least 1920×1080.
Expose an explicit Guided/Interactive class if expensive analysis uses reduced
resolution; never silently lower reference quality. Preserve full-size output and
validate edges, thin structures and effect placement against the reference.

## 14. Add GPU support after the CPU contract works

Keep CPU as the reference. Port the formulation, not a superficially similar filter.
Check which extensions the actual host supports instead of assuming that every OFX
host exposes the same Metal/OpenCL/CUDA behavior.

For Metal in this repository:

- CPU-backed images may use validated no-copy or staging upload paths.
- A native GPU handle is not a CPU pixel pointer.
- Use the negotiated device and queue; validate source/mask/output compatibility.
- Keep buffers and scratch resources alive until asynchronous work completes.
- Define whether a failure permits host retry or CPU fallback.
- Do not claim a Metal-only mode succeeded if it silently substituted CPU.

Use `kOfxStatGPURenderFailed` where the negotiated GPU contract calls for a host
retry, rather than accessing GPU storage as ordinary memory. Consult the
[OFX rendering reference](https://openfx.readthedocs.io/en/main/Reference/ofxRendering.html)
and the headers for the specific extension you implement.

Test final output and important intermediates. Record tolerances, alpha/identity
invariants, topology invariants, transfer time, GPU time and complete host time.
Optimizing a downstream kernel does not fix an upstream 30-second analysis.

## 15. Common failures and the quickest useful checks

| Symptom | First checks |
| --- | --- |
| Node absent from Tab search | Factory registration, architecture, exported symbols, bundle path, restart |
| Old controls still visible | Duplicate/stale loaded bundle; inspect actual installed UI |
| Knob exists but does nothing | Render-time parameter lookup, selected execution path, cache dependencies |
| Black output or crash | Missing image, wrong components/depth, bounds/stride, GPU handle misuse |
| Works at origin zero only | Absolute-coordinate indexing and bounds offsets |
| Edge seams at proxies/tiles | ROI halo, canonical-to-pixel conversion, global state built from partial input |
| Changed frame reuses old output | Source/frame cache identity and clip-change handling |
| Every knob takes seconds | Oversized invalidation domain; inspect stage timings |
| Saved script changes behavior | Renamed parameter IDs or reordered choices |
| GPU looks different | Color matrices, layout, precision, update order and fallback policy |
| GitHub rejects the push | Generated renders/snapshots committed into history |

Keep generated EXRs, PFM/NumPy dumps, snapshots and build folders out of Git.
Track source code, small input fixtures, tests, reports and intentional compact
evidence separately. In this repo, `tests/visual/renders/` is ignored. Removing an
oversized file in a later commit does not remove it from earlier history.

## 16. A sensible implementation order

1. Stable identifier, factory registration and copy-only float CPU node.
2. Small core operation plus unit tests.
3. Animated parameters, alpha/color contract, bounds and exact bypass.
4. ROI/proxy/PAR behavior and optional mask.
5. Actual Nuke discovery, render, save/reload and visible UI validation.
6. Dependency-aware caching and production-resolution knob timing.
7. Explicit interactive/reference modes only if necessary.
8. GPU implementation, parity and lifecycle tests.
9. Distribution packaging, signing and target-host certification.

Build → test → assemble/sign → install or isolate → restart → create → render →
save/reload → inspect. Keep those steps distinct: compiling is not host validation.

## Further reading

- [OpenFX SDK and support-library source](https://github.com/AcademySoftwareFoundation/openfx)
- [OpenFX reference](https://openfx.readthedocs.io/en/latest/Reference/)
- [Foundry: loading OFX plug-ins](https://learn.foundry.com/nuke/content/comp_environment/configuring_nuke/loading_ofx_plugins.html)
- [Pigment's host installation guide](HostNodeExposure.md) — contains historical
  research notes; verify current defaults against the live descriptor and installed UI.
