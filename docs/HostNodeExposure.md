# Exposing Pigment Nodes in Nuke and DaVinci Resolve

This is the repeatable development workflow for making the Pigment OpenFX nodes
visible after a build. Pigment is a native OFX bundle; no Nuke gizmo, Python wrapper,
Fusion macro, or Resolve DCTL is required.

The currently registered effects are:

| UI name | OFX identifier | Current status |
|---|---|---|
| Pigment ChromaDiffusion | `org.painterlyofx.ChromaDiffusion` | Stage 1 effect |
| Pigment DetailCollapse (Research) | `org.painterlyofx.DetailCollapse` | Stage 2 research effect |
| Pigment (Research) | `org.painterlyofx.Pigment` | Phase 3 integrated research effect |

Both are grouped under **Pigment** by their OFX descriptors.

## Normal rebuild and installation

Fully quit Nuke and Resolve before replacing a bundle they have already loaded. From
the repository root:

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --build build --target sign-local
cmake --install build --prefix /Library/OFX/Plugins
```

This installs the complete bundle at:

```text
/Library/OFX/Plugins/Pigment.ofx.bundle
```

Always install the complete `.ofx.bundle`. Do not copy only the executable: the Metal
backend also needs `Contents/Resources/Pigment.metallib`. Signing must happen after
the executable and resources have been copied into the bundle.

Useful installation checks:

```sh
codesign --verify --deep --strict --verbose=2 \
  /Library/OFX/Plugins/Pigment.ofx.bundle

file /Library/OFX/Plugins/Pigment.ofx.bundle/Contents/MacOS/Pigment.ofx

ls -l /Library/OFX/Plugins/Pigment.ofx.bundle/Contents/Resources/Pigment.metallib
```

The executable should report `arm64`, and signature verification should succeed.

## Nuke

### Normal interactive use

1. Install the bundle in `/Library/OFX/Plugins` as above.
2. Fully quit every Nuke process and launch Nuke normally.
3. In the Node Graph, press **Tab**.
4. Search for `ChromaDiffusion`, `DetailCollapse`, or `Pigment`.
5. Select the node and connect it between an image source and Viewer/Write node.

The nodes also appear under the **Pigment** OFX grouping. The DetailCollapse validation
scene is [`tests/visual/DetailCollapseValidation.nk`](../tests/visual/DetailCollapseValidation.nk).

### Test a build without installing it

Nuke honors `OFX_PLUGIN_PATH`, so a local build can be tested directly:

```sh
OFX_PLUGIN_PATH="$PWD/build" \
/Applications/Nuke17.0v1/Nuke17.0v1.app/Contents/MacOS/Nuke17.0
```

Adjust the Nuke application path for the installed version. Do not leave an older
installed Pigment bundle active while testing through `OFX_PLUGIN_PATH`; two copies
with the same OFX identifiers make it unclear which binary Nuke selected.

### Automated discovery checks

ChromaDiffusion:

```sh
validation_dir=$(mktemp -d /tmp/pigment-nuke.XXXXXX)
OFX_PLUGIN_PATH="$PWD/build" \
PIGMENT_NUKE_OUTPUT="$validation_dir" \
/Applications/Nuke17.0v1/Nuke17.0v1.app/Contents/MacOS/Nuke17.0 \
  -t tests/nuke_validate.py
```

DetailCollapse and its saved debug renders:

```sh
/Applications/Nuke17.0v1/Nuke17.0v1.app/Contents/MacOS/Nuke17.0 \
  -t tests/nuke_detailcollapse_validate.py
```

Integrated Pigment and its matched comparison/debug renders:

```sh
/Applications/Nuke17.0v1/Nuke17.0v1.app/Contents/MacOS/Nuke17.0 \
  -t tests/nuke_pigment_validate.py
```

Open `tests/visual/PigmentValidation.nk` after that check for a 1920×1080 interactive
Viewer setup. The node returned by Nuke's Tab search is labelled `Pigment`, while its
persistent OFX identifier remains `org.painterlyofx.Pigment`.

These checks use the actual OFX plug-in. Passing core tests alone does not prove that
Nuke discovered or rendered the bundle.

### If Nuke does not show the node

Check, in order:

1. Nuke was fully restarted after installation.
2. There is only one Pigment bundle in the effective OFX search paths.
3. `Pigment.ofx.bundle/Contents/MacOS/Pigment.ofx` exists and is arm64.
4. `Pigment.metallib` exists in `Contents/Resources`.
5. `codesign --verify` succeeds.
6. The terminal smoke test above discovers the expected OFX identifier.

Avoid adding `menu.py`, `init.py`, or a gizmo to solve an OFX discovery problem. A
missing node should be diagnosed as a bundle, architecture, signature, registration,
or host-loading issue.

## DaVinci Resolve

Resolve should use the system installation rather than the Nuke-oriented
`OFX_PLUGIN_PATH` development shortcut.

1. Fully quit Resolve.
2. Install and sign `/Library/OFX/Plugins/Pigment.ofx.bundle`.
3. Launch Resolve normally:

   ```sh
   open -na "DaVinci Resolve"
   ```

4. In the **Color** page, open the Effects/OpenFX library and search for `Pigment`,
   `ChromaDiffusion`, or `DetailCollapse`. Drag the effect onto a color node.
5. In the **Fusion** page, press **Shift-Space**, search for the same name, select the
   Pigment result, and choose **Add**.

Resolve's log can confirm discovery:

```sh
rg -i "Pigment|painterlyofx" \
  "$HOME/Library/Application Support/Blackmagic Design/DaVinci Resolve/logs/ResolveDebug.txt"
```

Discovery and rendering are separate checks. A log entry showing
`org.painterlyofx.DetailCollapse` proves that Resolve loaded the factory, but the node
must still be added and rendered to validate image layouts and Metal negotiation.

### Current Resolve caution

Resolve Studio 21 has discovered both the Pigment bundle and the DetailCollapse
research node on the development machine. A previous attempt to add DetailCollapse
made Resolve unresponsive before native Metal queue/buffer negotiation could be
recorded. Until that issue is diagnosed, do not treat Resolve discovery as proof that
the research Metal path is production-safe.

If Resolve hangs, preserve `ResolveDebug.txt` and any diagnostic report before
relaunching. Do not work around a native-buffer failure by treating an `MTLBuffer`
handle as CPU image memory. Pigment must return `kOfxStatGPURenderFailed` where the OFX
contract requires it so the host can retry with CPU-backed images.

## Registering a future Pigment effect

Installing the bundle only exposes factories that are compiled and registered. When
adding another public effect:

1. Give it a stable, unique identifier such as `org.painterlyofx.EffectName`.
2. Add its factory header and factory instance to
   [`src/ofx/PluginMain.cpp`](../src/ofx/PluginMain.cpp).
3. Add its wrapper source to the `Pigment` module source list in
   [`CMakeLists.txt`](../CMakeLists.txt).
4. In `describe()`, set all three labels and use
   `setPluginGrouping("Pigment")`.
5. Declare the actual supported contexts, components, bit depths, tiling behavior,
   and Metal capability. Do not advertise capabilities the implementation cannot
   render correctly.
6. Rebuild, run tests, sign, reinstall the complete bundle, and fully restart each
   host.
7. Verify both factory discovery and a real float RGB/RGBA render.

Mass Formation is intentionally not registered as its own effect. It remains an
internal backend of DetailCollapse. DensityVeil should receive a new public factory
only when its implementation stage begins.

## Short repeatable checklist

For ordinary development builds, the sequence is:

```text
Quit hosts → build → test → sign → install complete bundle → restart host
→ search for Pigment node → instantiate → render → inspect host log if needed
```

Following this sequence prevents most stale-binary, duplicate-identifier, missing
metallib, and cached-host problems.
