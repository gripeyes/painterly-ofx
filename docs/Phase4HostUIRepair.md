# Phase 4 installed 1080p hands-on baseline

Backend reference: `7cc3ab8`. Gate C remains unaccepted. Full Reference retains
the artistic formulation, spectral/component budgets, resolution, iteration count,
and stopping tolerances. Interactive is a separately selected approximation.

## Interface contract

New nodes default to Interface **Pigment**. Comparison indices and the underlying
legacy Comparison default remain unchanged. The artist interface routes Phase 4.
Research exposes named C0–C4 paths; Legacy Comparisons preserves other algorithms.
Color / Compute is initially expanded. Eight backing plate states are preserved
but hidden; one editor selects All or an active plate. All edits set the chosen
property on active plates only. It does not reset other properties or inactive
plates. The nonanimated editor writes animated backing state at the current frame.
Python automation uses `phase4PlateA…H` parameters: Nuke Python `setValue` on an
editor proxy does not deliver the OFX user-change callback. Actual GUI events do.

Actual installed-host testing caught a second issue: dynamically hiding nested
OFX groups lost child labels/choices in Nuke. Research groups are now flat and
initially expanded, with explicit leaf visibility. Host round-trip certification
is still required; parameter enumeration alone is not evidence.

## Historical backend inventory

History inspected includes `288d2fe`, `4c22423`, and all-history exact-string
searches. DetailCollapse exposes these exact choices:

- Reference Bilateral CPU
- Guided CPU
- Domain Transform CPU
- Guided Metal
- Domain Transform Metal (CPU fallback)

`Guided Metal` executes weighted guided-filter statistics/coefficient processing
using Metal/MPS. `Domain Transform Metal (CPU fallback)` is a reserved choice and
actually requests CPU retry/fallback. Both remain in DetailCollapse, where valid.
They are not interchangeable implementations of Phase-4 matting eigensolves,
plate-conditioned merging, or bounded Poisson fields. Applying a guided filter
to accelerate those stages would change the image formulation and is prohibited.

Historical integrated Pigment calls `MetalInstance::renderIntegrated` for its
legacy comparisons, including Current Guided DetailCollapse. It had no separate
backend selector. That path remains accessible in Legacy Comparisons. Device,
queue, no-copy/staging, pipeline registry, and scratch infrastructure are reused
by Phase 4 without invoking the guided artistic algorithm.

No exact artist choice named **Guided GPU** or **Full Metal** was found in this
repository's available Git history. These names have not been fabricated.
Phase-4 choices remain Auto (Hybrid), CPU Reference, Metal Spill (Hybrid).
Spectral Pigment always uses CPU interaction. Explicit Metal accelerates transport
as well as Linear/Density interaction; Auto uses faster cached CPU transport.

## Dependency repair

- Amount/Mix reuse cached interaction and perform final-stage work only.
- Spill strength, directionality, color law, density, and plate appearance edits
  reuse source analysis, grouping, supports, hierarchy, fields, and transport.
- Reach changes transport, not A1–C.
- Organization recuts cached monotone trees instead of rebuilding trees.
- Scale/Chroma Spread rebuild exact intrinsic support from retained analysis-grid
  alpha, without regrouping or repeating A1–A3.
- AB-only intrinsic support changes leave intrinsic Y support bit-exact.
- C1 Y/AB complexity and cut edits rebuild only their dependent field family.
- Independent recovery initializations, transform blocks, plate hierarchies, and
  full-resolution conditional appearance refinement execute in bounded parallel
  jobs. Reduction order is preserved.

Remaining Full work includes Y/AB-independent hierarchy/transport invalidation on
support changes. Full is deliberately not presented as interactive at 1080p.

## Explicit execution classes

Execution Class is separate from Compute Backend and is visible in Color / Compute
in both interfaces. Fresh artist nodes default to **Interactive / Guided**.
Existing saved execution choices retain their numeric indices. Full is labeled
**Full Reference (expensive)**, with a visible minutes-at-full-HD warning.
One Preview Quality choice replaces the visible implementation grid-size knob:
Fast / Balanced / Detailed (Research). Balanced is the artist default.
Internally these use long-edge budgets 64 / 128 / 256 with unchanged mode budgets.
Compute Backend selects CPU/Metal interaction within either class.

Full calls the existing renderer directly. Seven independent recovery starts and
component transformation blocks now execute in bounded parallel jobs, retaining
serial selection/reduction order. On the saved 512 fashion host fixture the new
Full output passes `oiiotool --diff` against the previous saved Full output.

Interactive computes the current spatial formulation on a block-average straight
RGB grid, without reducing spectral/component budgets or solver iterations.
Grouping, B, selected C and transport also run on that grid: it is **not** merely
an accelerated exact eigensolve. Pixel-scale budgets are explicitly rescaled to
that grid. The coarse fitting uses opaque analysis samples; this differs from
full alpha-weighted fitting. Full-resolution source YAB guides cached local
four-sample affine color-mixture interpolation (ridge .05, simplex normalization).
The output is `source + guided(coarse_result - coarse_source)`, followed by
full-resolution Mask/Amount/Mix and premultiplication/alpha handling. It lifts the
effect difference, not a low-resolution photograph. Unresolved source detail is
retained in this preview; fine organization and reference hierarchy geometry are
not promised identical. No obsolete DetailCollapse filter executes.

The historical Guided Metal architecture contributes device/queue, staging,
coefficient/reconstruction dispatch and persistent scratch infrastructure.
Its actual MPS box-statistic guided-filter coefficients are not mathematically
equivalent to Phase-4 matting/merging, so they were not inserted into this path.
This initial Guided analysis/recovery is CPU; existing Metal Linear/Density
interaction is reused. Spectral remains CPU. The preview is not branded a full
Metal implementation.

Both caches are retained independently, so switching classes does not discard a
previous Full analysis. Source/grid/gamut/premultiplication changes rebuild
guidance. Artist spatial edits reuse it and A1–A3. Downstream appearance edits
reuse the spatial states; final Amount/Mix does not rebuild interaction.

## Installed-host evidence

The signed arm64 bundle is installed at `/Library/OFX/Plugins/Pigment.ofx.bundle`.
`tests/nuke_phase4_artist_controls.py` renders through actual Nuke at 1920×1080;
`tests/phase4_host_profile_report.py` records stage times and cache rebuild deltas.
Fixtures are existing research plates reformatted to 1080p, not native-HD captures.
Write timings include full-frame float EXR output and are not isolated kernel times.

Actual NukeX GUI evidence includes the execution dropdown, visible artist knobs,
live Pictorial Scale edit, Density switching, plate-B Tone edit serialized into
its hidden animated backing state, and Pigment→Research→Pigment preservation.
Full-HD preview quality differs intentionally: fashion RGBA RMS difference from
Full is 0.00521734, maximum 0.0660704 (alpha exact). Reference parity tolerance is
not applied to this explicit approximation. Gate C remains unaccepted.

The direct coarse-RGB lift was preserved as negative preview evidence: it lost
photographic acuity. Only the effect-difference lift is installed now.

### 1920×1080 installed Nuke timing

Interactive128 final run (no competing reference render), seconds including EXR:

| Edit | Full | Interactive |
|---|---:|---:|
| Cold | 119.167 | 0.716 |
| Unchanged | 0.308 | 0.086 |
| Pictorial Scale | 212.335 | 0.160 |
| Structure Lock | 230.577 | 0.159 |
| Luma Organization | 10.530 | 0.131 |
| Chroma Organization | 29.729 | 0.123 |
| Chroma Spread | 62.350 | 0.157 |
| Spill Reach | 1.774 | 0.131 |
| Color Interaction → Density | 0.550 | 0.127 |
| Plate B Tone | 0.629 | 0.131 |

The Full run overlapped other diagnostic jobs during some stages; it is not an
uncontended microbenchmark. Interactive runs include kernel timings separately
in the profile JSON. Full cold analysis: A1 34.912s, A2 10.173s; hierarchy 51.677s
and C1 field 13.588s. Therefore exact full-HD interaction is not solved simply by
accelerating eigensolves. Guided primary edits reuse source analysis and cached
full-resolution guidance (both build counts stay one across the complete sweep).

Full is a final/reference class, not the claimed practical iteration class.
Interactive's benefit is measured on primary spatial knobs, not only Spill/Mix.
These measurements establish execution speed, not photographic acceptance of C1
or equivalence of the reduced hierarchy to Full.

## 1080p quality and testing policy

Full-HD is the minimum host artist-testing resolution. The two subsequent
Full-HD Reference fixture jobs were stopped at the user's request; their partial
logs are retained. The completed fashion Full timing/render is reused. No new
Full-HD Reference render was run for the quality sweep. Routine exact parity uses
saved stage inputs, 512 fixtures or representative full-resolution crops.

Fashion, knee, lowlight and one-pixel/high-contrast structures were rendered at
1920×1080 across three Guided quality settings, Spill off/strong, and Scale24/96.
Contact sheets use an identical display-only gamma; RGB data remain scene-linear.
Balanced cold host time including EXR: fashion .729s, knee .641s, lowlight .829s.
Detailed cold: 2.967s / 3.098s / 2.225s. No new speed-oriented model changes followed.

Balanced preserves the visible face/clothing and knee curvature without the
direct-upsample acuity loss. Source edge-peak p95 displacement is 0–1px on the
photographs, and 0px on both axes of the thin-line test after accounting for equal
peaks on opposite sides of a one-pixel line. This supports localization, not an
assertion of identical Reference hierarchy topology. About 1–3% of photographic
sample peaks differ by more than one pixel; those include competing fine edges.
Broad 32px-cell fashion Y/A/B RMSE against saved Full: .003521/.001612/.003429.
RGBA .005217 versus RGB .006024 RMSE differ because alpha is exact/zero-error.

Spill and Scale comparisons keep important contours visibly located. Balanced
does not show a broad halo in these views. Detailed is **not quality-certified**:
the synthetic C1 comparison develops a localized color/field artifact, and its
low-light edge metric is worse. It is retained explicitly as Research rather than
marketed as a universally better preview. No Gate-C acceptance is claimed.
Unresolved fine source detail remains in Guided; final fine-detail organization
must still be checked in Full. Only fashion currently has an identical-input
saved Full-HD comparison; knee/lowlight quality checks are not Full-HD parity.

13 regression suites pass. The repeated 72 photographic Linear/Density Metal
comparisons preserve CPU transport/state parity and bounded interaction error.
Guided CPU vs Hybrid 1080p fashion also passes image comparison. Spectral stays CPU.

The 72 photographic interaction cases have maximum YAB error 2.6077e-7 and
maximum RMSE 2.95692e-8. Full 512 fashion matches the previous output bit-exactly.
No Full-HD Reference job remains running. Existing Full-HD logs and all negative
research outputs are preserved, not deleted. Restart Nuke to load the newly
installed bundle; an already-running host retains its previously loaded module.

This is the hands-on research baseline, not final artistic certification. The
next step is artist feedback on Balanced at 1080p. Spectral artifact diagnosis
and another Gate-C family were not started during this milestone.
