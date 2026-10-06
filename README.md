# Pigment OFX

Pigment is an OpenFX suite for organizing photographic information into broader
tonal and chromatic masses while keeping important geometry localized. Its goal
is **pictorial organization, not a paint filter or a globally softened photograph**.

The main tool is a persistent Nuke OFX node, `org.painterlyofx.Pigment`.
It works in scene-linear RGB, separates luminance from opponent chroma internally,
and exposes both a compact artist interface and an instrumented research interface.

**Current status:** the installed 1080p Guided/Balanced research baseline is usable
for hands-on exploration. The automatic plate foundation and host/backend
engineering are checkpointed. **The broad-form artistic gate (Gate C) remains
unaccepted.** Linear YAB and Density are usable comparison laws; Spectral Pigment
remains experimental and photographically unstable. A working render, a passing
test or a faster backend is not artistic acceptance.

## Start here

- **Artist:** create Pigment in Nuke; use **Pigment → Interactive / Guided →
  Balanced**. Start with Linear YAB. Full Reference is explicitly expensive.
- **Investigator:** switch to **Research**, inspect C0/A3 first, then compare
  representations with identical alpha, supports, transport and color law.
- **Developer:** see [architecture](#under-the-hood), [build](#build-and-install)
  and [testing](#what-we-have-tested).
- **Research:** use the [58-row research map](docs/PigmentResearchMap.md) and
  [56-entry linked bibliography](docs/ResearchBibliography.md). Read the failure
  reports before proposing another formulation.

This README summarizes repository code and recorded experiments as of
**2026-10-07**. Older reports describe earlier checkpoints; statements such as
“Metal blocked” or “interface opt-in” there are historical, not the latest host
status. Research labels C0–C5 are names, **not serialized enum indices**.

## What we have built

| Part | What it does | Status / scope |
|---|---|---|
| ChromaDiffusion | Directional spatial chroma treatment with luminance-preservation controls | Established Stage-1 effect; separate from Phase 4 |
| DetailCollapse (Research) | Compares reference bilateral, Guided CPU/Metal and Domain Transform CPU treatment | Preserved legacy research node |
| Integrated Pigment, Phases 3–3.1 | Weighted-mean and density-seeking representative mass formation, residual/debug infrastructure | Preserved comparisons; known blur, ringing and melted/cellular risks |
| Phase 3.2 Pictorial Planes | Manual RGBA plane ownership, independent Y/AB transitions, regularized quadratic targets and detail extinction | Engineering checkpoint; not an accepted artistic foundation |
| Phase 3.3 Soft Pictorial Plates | Automatic convex-palette / spatial-mesh plates, Source Smooth and actual TGV, conditional shading and multigrid | Implemented CPU path; photographic isolation failed |
| Phase 4 Automatic Plate Graph | Fuzzy latent vocabulary, spatial appearances, public plates, plate-conditioned region hierarchy, selectable fields and directed Spill | Active research architecture; frozen upstream, unaccepted Gate C |
| Color Interaction | Linear YAB, nonspectral Density and compact Spectral Pigment on the same transport | Linear/Density CPU and hybrid Metal; Spectral CPU-only research |
| Host/debug/performance tools | Nuke scenes/scripts, snapshots, stage sheets, parity tests, profiling and dependency-aware caches | Implemented; evidence and limitations below |

Phase 4 starts from the **original straight source**, not the output of a legacy
effect. It does not execute Representative Mode, Weighted Mean, Veil, Guided
DetailCollapse, Phase-3 transitions, quadratic plane targets, Source Smooth or
TGV reconstruction as its core.

Source and optional effect Mask are the Phase-4 inputs. PlaneMap is a legacy
Phase-3.2/3.3 input and is ignored by Phase 4, including its validation.

## Hands-on Nuke interface

Fresh nodes default to the compact **Pigment** interface. The stored legacy
Comparison default remains Representative Mode for compatibility; artist-interface
routing selects Phase 4 without rewriting old comparison indices.

### Pigment controls

| Control | Meaning |
|---|---|
| Pictorial Scale | Coordinates graph-support scale and Y/AB hierarchy cut scales; not a blur radius |
| Structure Lock | Coordinates boundary discrimination, merge tolerance and transport protection |
| Luma / Chroma Organization | Independently select which internal Y/AB region boundaries disappear and their interaction strength |
| Luma / Chroma Complexity | Control source-gradient survival in the active C1 field; higher values retain more description |
| Chroma Spread | Changes AB support extent/participation independently of chunk cuts |
| Spill | Scales directed donor/receiver appearance interaction |
| Spill Reach | Controls graph-geodesic travel attenuation, independently of seed amplitude |
| Directionality / Asymmetry | Blends symmetric and directed interaction |
| Color Interaction | Linear YAB / Density / Spectral Pigment |
| Pigment Density | Allows nonlinear interaction to change Y; not transport reach or a display contrast curve |
| Amount / Mix | Masked overall strength and final straight-RGB blending |

Pigment currently maps to **C1**, not an artistically accepted new renderer.
These are useful research starting controls, not a certified aesthetic preset.

**One plate editor** selects All or an active plate A–H and exposes Enable,
Weight, Tone, Chroma A/B, Spill Out and Receive Spill. Backing per-plate animated
state is retained; inactive slots are not offered as normal editable plates.
All applies the selected property to active plates. For scripted animation use
backing parameters: setting a UI proxy through Python does not necessarily issue
the OFX GUI-change callback.

**Copy to Research** copies the current frame's evaluated macro state into
low-level controls for inspection. It is not an animation bake. Ordinary interface
switching preserves each interface's settings; it is not an implicit parameter copy.

Research exposes representation selection, plate/support/hierarchy controls and
stage diagnostics. Legacy Comparisons keeps older algorithms accessible.
See [macro equations](docs/PigmentArtistMode.md) and the later
[installed host/UI report](docs/Phase4HostUIRepair.md) for current defaults and
host-specific behavior.

### Execution and backend are different choices

| Choice | What actually runs |
|---|---|
| Interactive / Guided, Balanced | Explicit reduced spatial analysis plus full-resolution source-guided effect-difference reconstruction; default artist workflow |
| Full Reference (expensive) | Exact reference formulation; may take minutes at full HD |
| Auto (Hybrid) | CPU spatial work / cached CPU transport, Metal Linear/Density appearance interaction when available, CPU fallback |
| CPU Reference backend | CPU interaction within the selected execution class; choose Full Reference execution too for an exact full reference |
| Metal Spill (Hybrid) | Explicit Metal transport comparison and Linear/Density interaction; not a full-node GPU implementation |

Guided retains full output dimensions and lifts
`source + guided(coarse_result − coarse_source)`; it does not upscale a
low-resolution photograph. Balanced uses a 128-pixel analysis long-edge budget,
Fast 64 and Detailed 256. Mode/component budgets are not reduced, but the spatial
grid, hierarchy and alpha-weighting behavior differ from Full. **Guided is an
explicit approximation, not exact CPU/Metal parity.** Detailed remains Research:
a finer grid did not monotonically improve quality and exposed a field artifact.

The historical **Guided Metal** backend still exists in DetailCollapse, where it
computes guided-filter statistics. Its device/queue/buffer infrastructure is
reused, not its obsolete artistic smoothing. No exact historical choices called
“Guided GPU” or “Full Metal” were found in available history; those names are not
invented substitutes for the current hybrid implementation.

Spectral always uses CPU. Native Metal-host buffer calls currently request
CPU-buffer retry for Phase 4; hybrid acceleration is CPU-backed, not full native
host GPU certification.

## Under the hood

### Phase-4 data flow

```text
original straight RGB → explicit RGB/YAB conversion
  → A1 matting eigenspace → A2 fractional latent vocabulary
  → corrected A3 spatial appearance → public plates
  → shared source geometry + plate-conditioned Y/AB hierarchies
  → selected C0–C5 appearance representation
  → per-plate artistic appearance controls
  → directed graph Spill + selected color law
  → artist-adjusted alpha reconstruction
  → Amount / Mask / Mix → scene-linear RGB, original alpha
```

### A1 / A2: frozen spatial vocabulary

A1 constructs the **symmetric closed-form matting Laplacian** and solves its
smallest eigenpairs using Eigen/Spectra. The original hand-written solver and
row-normalized affinity iteration were inadequate.

Forensic recovery distinguished useful **signed eigenmode sheets** from A2 alpha
sheets and public-plate sheets by file/pipeline provenance, not by appearance alone.
The recovered configuration uses the simple constrained component transform;
it does not keep optimizing occupancy, sparsity or progressive splitting.

The frozen A1 eigensolve does **not** include the subsequently tried CMF
augmentation. Signed affine reconstruction flow `W_CMF` and separate nonnegative
directed affinity `F` remain available downstream. They are not interchangeable:
signed reconstruction weights are never probabilities or Spill capacities.

The vocabulary capacity is 24, normally requested at 16; public plate capacity is
8, normally 6. Raw components can be weak or imperfect. Their purpose is useful
soft spatial vocabulary, not finished masks or a hard color palette.
See [visual-basis provenance and freeze rule](docs/Phase4VisualBasisCheckpoint.md).

### A3: spatially varying appearance, not centroids

Corrected A3 retains local YAB Gaussian means and conditioned covariance matrices
for each latent component. Under fixed alpha it refines component colors subject
to exact source reconstruction:

```text
residual = source − Σ alpha_i mu_i
G        = Σ alpha_i Sigma_i
C_i      = mu_i + Sigma_i G⁻¹ residual
```

Full-resolution refinement corrects analysis-extension/product error. This is
an adapted conditional unmixing formulation, **not the complete Aksoy SCU system**.
The earlier local-neighborhood version made layers nearly identical source copies;
tiny composite RMSE alone failed to reveal that defect.
See [A3 reference comparison and correction](docs/Phase4A3ReferenceComparison.md).

### Public plates: ownership, support and appearance are separate

Each public plate stores:

- **alpha:** normalized reconstruction contribution;
- **supportY:** non-normalized luminance participation domain;
- **supportAB:** independent non-normalized chromatic participation domain;
- **Y/AB appearance:** spatial fields, not representative colors alone;
- affinity, adjacency and diagnostic information.

Supports overlap and do not have to sum to one. Support extent is independent of
Chunk Scale. Confidence is diagnostic; it does not silently return difficult
pixels to untouched source. Grouping retains latent contributions and fixed
canonical identities rather than drifting into equal/duplicate public plates.

### Gate B: plate-conditioned region merging

A source-derived contour cue and atomic watershed regions form one shared region
adjacency graph. Every plate has separate Y and AB merge organization informed by
its appearance, support and shared structural evidence.

Chunk Scale cuts a monotone merge tree: internal boundaries may disappear while
retained contours stay at their source coordinates. It is not a smoothing radius.
The hierarchy is **UCM-style**, not full gPb/OWT, and not a semantic object segmenter.
Production B is frozen; diagnostic barrier ablation did not replace it.

### Spill: appearance transport, not ownership growth

Spill uses directed nonnegative `F`, structural capacities, graph-geodesic distance,
intrinsic support, Spill Out and Receive Spill. Seed amplitude and travel distance
are separate. The repaired attenuation no longer consumes Reach as a seed penalty.

Spill never changes alpha, pixel coordinates or hierarchy topology. Y and AB have
separate support/transport/interaction. AB-only Spill preserves composite Y
bit-exactly in the tested paths. Zero Reach returns intrinsic seeds; it need not
disable local overlapping appearance interaction. Zero Spill bypasses interaction.

Weight adjusts normalized reconstruction ownership. Tone/A/B bias change appearance
after field construction. These creative edits do not rerun automatic extraction
or change hierarchy topology.

### Color law: scene-linear in, scene-linear out

- **Linear YAB:** unchanged arithmetic interaction baseline.
- **Density:** bounded material log-absorbance mixing with explicit scene
  magnitude/residual reconstruction; no spectral state.
- **Spectral Pigment:** compact three-coefficient smooth reflectance fit,
  transient 21-wavelength quadrature, equal-scattering infinite-thickness
  Kubelka–Munk interaction, XYZ conversion back to the configured RGB primaries.

Raw HDR/negative RGB is **not** treated as physical reflectance. Magnitude carries
positive scene intensity; a bounded material component enters the optical model;
an explicit residual preserves negatives and fit/out-of-model error. No DRT,
film response, grain, display transform or global HDR clamp is added.

The spectral prototype is not measured-pigment identification, Mixbox, finite
paint-layer simulation or the complete Jakob/Hanika lookup system.
Its instability and isolated, noninstalled correction are described below.

## Research representations and what failed

Failed paths stay available as comparisons, not production defaults.

| Path | Experiment | Photographic outcome |
|---|---|---|
| Phase 3 Weighted Mean | Rolling neighborhood mass formation | Softened detail without reliably forming fewer coherent masses |
| Phase 3.1 Representative | Density-seeking candidate representatives | Improved grape/laundry organization; ringing, melted/cellular transitions and switching risks remain |
| Phase 3.2 Manual Planes | Regularized quadratic plane targets | Engineering validated; artistic isolation not established, quadratic/transition risks motivated later reset |
| Phase 3.3 Automatic Plates | Four-color convex unmixing / RGBXY mesh + Source Smooth/TGV | Near-hard ownership, visible polygons and airbrushed appearance; failed |
| C0 A3 Passthrough | Corrected public appearance without field simplification | Cleanest photographic baseline; not itself strong pictorial reorganization |
| C1 Bounded Poisson | Simplified source gradients inside retained support-aware boundaries | Haze/flattening and patch-like cloth; broad-volume/detail tradeoff unresolved |
| C2 Preserved Second Moments | Regional means + first + centered second moments | Some numerical/directional improvement; did not materially resolve curved volume/haze |
| C3 Regional Eigen | Boundary lift + a small regional Laplacian basis | Tested budgets failed continuous convincing form; remains visibly problematic in hands-on comparisons |
| C4 Sparse Curve / Field | Automatic sparse value-transition loci + harmonic reconstruction | More organization, but slabs/patches and missing broad form; extraction topology was the main diagnosed limitation |
| C5 Layered Broad Fields | A few overlapping affine/radial fields plus structure/medium/micro residual split | Implemented distinct sublayers, but embossed fashion description, knee patches and insufficient useful organization; failed |

C5 fits sparse robust macro observations, not every source pixel, and uses separate
Y/AB layer geometry. Its actual prototype allows up to four Y / three AB fields.
The residual partition preserved too much descriptive “structure”; greedy broad
envelopes were also restrictive. This limited adaptation does not disprove
Richardt/Du layered representations.

**Boundary and ablation negative evidence:** broad side-boundary values did not
rescue the regional eigenfield. Removing low-persistence interior barriers enlarged
domains but did not solve fashion haze/patchiness. Fragmentation is therefore not
the sole demonstrated bottleneck. Higher moments, denser anchors and larger
eigenmode sweeps were closed, not silently incorporated into the active path.

**Spectral failure localization:** residual-only/zero-magnitude material was
encoded as black pigment; magnitude-independent K/S mixing let faint donors create
strong absorption and knee punctures. Artifacts existed before independent Y/AB
recombination. An isolated positive-scene-mass correction reduced punctures, but
remaining extreme K/S sensitivity and shadow/color outliers failed photographic
acceptance. A standalone authors' rgb2spec inversion comparison did not establish
an advantage over Density. The correction is an optional diagnostic policy,
**not a change to the installed artist baseline**.

C5 is appended to Research in the current source/build, preserving older indices.
Its latest host exposure has **not yet been certified in the installed Nuke node**;
an already-open Nuke process can retain the older module. Do not mistake the
source selector for installed-host proof.

Detailed evidence: [Phase 3](docs/Phase3IntegratedPigment.md),
[3.1](docs/Phase3_1DensitySeeking.md), [3.2](docs/Phase3_2PictorialPlanes.md),
[3.3](docs/Phase3_3SoftPictorialPlates.md),
[C1](docs/Phase4ChunkSynthesisFailure.md),
[moments / barrier decision](docs/Phase4SecondMomentAndBarrierDecision.md),
[regional eigen](docs/Phase4RegionalEigenFieldExperiment.md),
[boundary appearance](docs/Phase4BoundaryAppearanceDiagnostic.md),
[C4](docs/Phase4SparseTransitionFieldExperiment.md),
[C5](docs/Phase4LayeredBroadFields.md),
[Spectral](docs/Phase4SpectralDiagnosis.md).

## Papers behind the parts

These are relationships, not claims to reproduce whole papers.

| System | Main research foundations | What Pigment adopts / does not claim |
|---|---|---|
| A1/A2 | Levin/Lischinski/Weiss closed-form matting; Levin/Rav-Acha/Lischinski Spectral Matting | Matting eigenspace and fractional vocabulary; adapted recovery, not publication-perfect reproduction |
| Corrected A3 | Aksoy soft color segmentation | Spatial distributions and constrained reconstruction; not full SCU model search/opacity optimization |
| Flow / support / Spill | Aksoy/Aydin/Pollefeys information-flow matting; Levin colorization | Reconstruction affinity and support ideas; signed W and nonnegative transport explicitly separated |
| Gate B | Arbeláez contour hierarchy; He/Kang/Morel region merging | Source-cue hierarchy and scale-controlled merging; no full gPb, OWT or boundary-curve evolution |
| C1/C2 | Fattal gradient-domain manipulation; Pérez Poisson editing | Modified-gradient bounded solves, not HDR tone mapping |
| C3 | Discrete Laplacian interior basis | Geometry-aware regional diagnostic, not Spectral Matting |
| C4 | Orzan Diffusion Curves; Jeschke generalized curves | Limited raster value curves; not full GDCI or generalized edge-blur |
| C5 | Richardt semi-transparent gradient layers; Du linear-gradient layer decomposition | Overlapping parametric fields; no general vectorizer or faithful full automatic layered recovery |
| Information split | He/Liu CST; scale/texture papers; TGV ramp interpretation | Observable structure/broad/medium/micro distinction; no CST solver or TGV-as-output in C5 |
| Color Interaction | Sochorová/Jamriška, Haase/Meyer, Jakob/Hanika; Smits/Burns comparisons | Residual strategy, compact spectrum and K–M foundation; not measured pigments or a complete reference implementation |

The [bibliography](docs/ResearchBibliography.md) links papers, project/code sources,
version clarifications and all 56 distinct entries from the
[unaltered supplied table](docs/PigmentResearchMap.md).
[Color Interaction design](docs/Phase4ColorInteractionDesign.md) and the
[C5 report](docs/Phase4LayeredBroadFields.md) record the relevant preimplementation
reviews. Inclusion in the catalog does not reopen a frozen stage.

## What we have tested

### Numerical / regression

The current build registers **14 CTest suites**. The latest implementation
checkpoint recorded all 14 passing; this documentation update does not claim a
new full test run.

Coverage includes color transforms, HDR/negative values, masks, strides/origins,
premultiplication and alpha, identity endpoints, legacy algorithms, manual planes,
Phase-3.3 decomposition/solvers, frozen Phase-4 semantics, hierarchy determinism,
bounded solves, regional eigen/curve/layer diagnostics, Spill invariants and
color-law continuity.

Photographic Metal tests cover 72 Linear/Density combinations on fashion, knee and
lowlight, including C0/C1/C4, zero/strong Spill and zero/large Reach.
Recorded maximum YAB error: **2.6077e-7**; maximum RMSE: **2.95692e-8**.
Metal transport matched the CPU comparison fields bit-exactly on tested cases.
These numbers certify tested backend equivalence, not artistic quality.

### Photographic / comparative

Cheek, shoulder, knee, fashion and low-light/chroma are the core research images.
Grape, laundry, CGI and broad-color fixtures support older engineering regressions.
Shared full-precision upstream snapshots and hashes keep downstream comparisons
honest; saved A1/A2 diagnostics are reused rather than recomputed just for sheets.

Stage sheets expose eigenmodes, latent alphas, spatial appearances, public
reconstruction, alpha, Y/AB supports, chunks, contours, pre/post field and Spill,
signed differences and sublayers. Metrics accompany actual photographic inspection.
A lower RMSE or gradient energy does **not** pass a visual gate.

Ballerina has informed hands-on failure observations, but no reproducible local
source/scene was found for the standalone C5 run; that report does not claim a
ballerina test.

### Actual Nuke host

Recorded arm64 Nuke 17 testing exercised node creation/persistence, compact
Pigment/Research switching, Copy to Research, unified plate editing, color laws,
frame changes, unusual origins/formats, float HDR/negative input, alpha,
Amount/Mix endpoints and mask bypass. GUI evidence caught visibility issues that
parameter enumeration alone missed.

Host scripted parity samples are not exhaustive per-pixel certification.
Detailed standalone comparisons cover complete fixture images. The newer C5
selector/debug additions need separate installed-host verification.

### Performance at 1920×1080

Recorded M2 Guided/Balanced Nuke Write timings include float EXR output and existing
fixtures reformatted to HD; they are not native-camera-HD or isolated kernel
benchmarks.

| Edit / render | Guided/Balanced |
|---|---:|
| Cold fashion | 0.716 s |
| Unchanged | 0.086 s |
| Pictorial Scale | 0.160 s |
| Structure Lock | 0.159 s |
| Luma Organization | 0.131 s |
| Chroma Organization | 0.123 s |
| Chroma Spread | 0.157 s |
| Spill Reach | 0.131 s |
| Color Interaction → Density | 0.127 s |
| Plate B Tone | 0.131 s |

Subsequent Balanced cold checks were 0.729 / 0.641 / 0.829 s for fashion/knee/
lowlight. The preserved Full-HD fashion Full run was about 119 s cold, with some
spatial edits over 200 s under competing diagnostic jobs. It is evidence of cost,
not a clean microbenchmark or a reason to repeat expensive Full-HD runs routinely.

Only fashion has a saved identical-input Full-HD quality comparison:
Guided-vs-Full RGBA RMSE **0.005217**, maximum **0.0660704**, alpha exact.
Photographic edge-peak p95 displacement was 0–1 pixel; some competing fine edges
differed more. Guided retains unresolved source detail and does not promise
identical fine hierarchy topology. These are preview reliability observations,
not acceptance of C1.

See [installed 1080p evidence](docs/Phase4HostUIRepair.md) and
[hybrid Metal validation](docs/Phase4MetalInteractiveMilestone.md).

## Caching and implementation map

Source analysis, public grouping/support, hierarchy, selected field, transport,
interaction and finalization have separate dependency domains. Unchanged states
are reused. Organization recuts cached trees; Reach changes transport; color law,
Density and appearance edits reuse spatial work; Amount/Mix are final-stage.
AB-only support changes preserve intrinsic Y support. Exact Full still has
remaining cross-family invalidation costs documented in the host report.

Independent jobs use bounded parallelism with deterministic reduction/selection
order. Full and Guided caches are retained separately. Purge/lifecycle handling
releases retained scratch; failed backend results are not accepted cache entries.

| Location | Responsibility |
|---|---|
| `src/plugins/Pigment.cpp` | OFX lifecycle, clips, routing, host UI, proxy plate editor, validation and render requests |
| `src/core/Types.h`, `ColorSpace.*`, `Masking.*`, `Execution.h` | Strided/bounded views, explicit gamut matrices, masks, geometry and deterministic execution |
| `src/core/IntegratedPigment.*` | Shared controls/debug IDs and preserved integrated comparisons |
| `src/core/PictorialPlanes.*` | Phase 3.2 manual membership, fits and reference reconstruction |
| `src/core/SoftPictorialPlates.*`, `PhotographicDecomposition.*`, `ScreenedMultigrid.*`, `PigmentPhase33.*` | Phase 3.3 CPU models, solvers and renderer |
| `src/core/SpectralMattingBasis.*`, `LatentPlateGraph.*`, `Phase4Types.*` | Frozen spatial vocabulary, corrected appearance, public plates and graph/state types |
| `src/core/RegionHierarchy.*` | Shared atomic geometry and plate-conditioned merge trees |
| `src/core/ChunkGradientSynthesis.*`, `RegionalEigenField.*`, `SparseTransitionField.*`, `LayeredBroadFields.*` | Preserved Gate-C comparison families |
| `src/core/PlateSpill.*`, `ColorInteraction.*` | Prepared transport and independent appearance laws |
| `src/core/PigmentPhase4.*`, `Phase4Interactive.*`, `PigmentControls.h` | Reference orchestration, caches, macro mapping and Guided effect-difference lifting |
| `src/metal/PigmentMetal.*`, `shaders/Pigment.metal` | Pipelines, device/queues, staging/no-copy, scratch and hybrid kernels |
| `tests/`, `docs/` | Unit/parity harnesses, host scripts, photographic diagnostics and research decisions |

`pigment_core` has no OFX dependency. C++17, Eigen 3.4.0 and Spectra 1.1.0
provide the host-independent numerical layer. Metal/Objective-C++ supply the
Apple acceleration layer. Historical CPU, GPU and host constraints are documented,
not hidden behind generic “GPU” labels.

## Color, alpha and identity contract

- Configure the actual scene-linear working primaries: ACEScg, linear Rec.709/
  sRGB primaries, linear Rec.2020 or Display-P3 D65 primaries. This is not automatic
  color-space detection or an input transfer-function conversion.
- Explicit matrices use ACEScg D60 / appropriate D65 white conventions.
- Process premultiplied input in straight RGB and return the original convention;
  preserve source alpha and zero-alpha behavior.
- Finite HDR and negative values are not globally clipped. Spectral internal
  material bounds/floors are instrumented and have a residual strategy.
- Amount 0 / Mix 0 are direct-copy identity endpoints. Mask is effect strength,
  not plate ownership.
- Local numerical safeguards are not a promise of an artifact-free spectral law.
- No temporal coherence, moving geometry, production Resolve Phase-4 certification
  or cross-platform GPU support is claimed.

## Build and install

Apple Silicon/macOS is the validated bundle target. Requirements: CMake 3.25+,
Xcode C++/Metal tools, macOS SDK and Git. CMake fetches pinned Eigen/Spectra and,
unless supplied locally, OpenFX 1.5.1.

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

Output: `build/Pigment.ofx.bundle`; staged copy:
`build/stage/Pigment.ofx.bundle`. Local signing is development signing, not a
claim of distribution notarization.

To use a local OpenFX checkout, configure `OPENFX_ROOT=/path/to/openfx` and
`PIGMENT_FETCH_OPENFX=OFF`. Offline builds also require the Eigen/Spectra sources
already populated or provided through CMake FetchContent overrides.

Install only when ready to replace the host-loaded build:

```sh
cmake --install build --prefix /Library/OFX/Plugins
```

Close/restart Nuke to load a replaced module. A build-path environment setting
does not prove Nuke selected that copy instead of an installed/cached plugin.
See [host discovery/install guide](docs/HostNodeExposure.md) and
[Writing OFX Nodes for Nuke](docs/WritingOFXNodesForNuke.md).

Legacy Resolve discovery has been exercised, but it is not certification of the
current Phase-4 Nuke workflow or native Metal host path.

## Reproducing tests and diagnostics

```sh
ctest --test-dir build --output-on-failure
```

Useful tools:

- `pigment_phase4_gate`: frozen snapshots, comparative stage outputs and isolated
  C experiments; see each report for the exact arguments and source provenance.
- `pigment_phase4_metal_tests`: saved-input transport and Linear/Density parity.
- `pigment_spectral_continuity`: isolated spectral continuity/diagnostic sweeps.
- `pigment_backend_benchmark`, `pigment_metal_harness`: legacy/backend benchmarks.
- `tests/nuke_phase4_artist_controls.py` and
  `tests/phase4_host_profile_report.py`: actual host edits and cache rebuilds.
- `tests/nuke_phase4_interactive_validation.py` and
  `tests/nuke_phase4_preview_quality.py`: host endpoints/parity and Guided quality.
- `tests/nuke_phase4_research.py`, `tests/nuke_phase4_c5.py`: research UI and newer C5 exposure.
- `tests/phase4_*_diagnostics.py`: contact sheets, stage measurements and differences.

A licensed Nuke installation is required for host scripts. For example:

```sh
/Applications/Nuke17.0v1/Nuke17.0v1.app/Contents/MacOS/Nuke17.0 \
  -t tests/nuke_phase4_research.py
```

Research scenes include `tests/visual/PigmentPhase4Research.nk`; earlier
`PigmentValidation.nk` and `DetailCollapseValidation.nk` preserve legacy comparisons.
Generated evidence lives in report-specific `build/` and
`tests/visual/renders/` directories and may not be present in a fresh clone.

**Storage policy:** generated renders, full-precision snapshots, downloaded paper
collections and build artifacts are not source history. Keep links, scripts,
settings, provenance and reports in Git; do not reintroduce large render archives.

## What remains open

1. Hands-on artist evaluation of the preserved 1080p baseline.
2. An accepted broad-form spatial representation: none of C1–C5 has passed the
   required photographic gate; Gate-C research is currently paused.
3. Spectral photographic stability/usefulness: diagnosed but unaccepted; no Metal port.
4. Installed-host verification of newly appended C5 controls/debug views.
5. Further acceleration only for formulations that survive artistic evaluation,
   with CPU reference and explicit Guided approximation kept distinguishable.

The durable result is both a working research instrument and a documented set of
negative experiments. We preserve that evidence rather than equating added
complexity with progress.
