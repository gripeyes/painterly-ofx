# Phase 3.2 Pictorial Planes

Phase 3.2 is implemented as comparison mode **Pictorial Planes (Phase 3.2)** in
the persistent `org.painterlyofx.Pigment` effect. Representative Mode remains
the default for newly created nodes.

## Implemented graph

The branch converts straight RGB to YAB once, builds fine/medium/broad analysis
bands, reads four independent raw Plane Map channels, propagates Y and AB
memberships independently, fits source-derived quadratic YAB fields, extinguishes
fine and medium residuals, and reconstructs at the original pixel coordinates.
Veil and Local Softness are downstream optional stages and both bypass cleanly.

Plane occupancy uses saturating total coverage. If raw enabled memberships sum to
`S`, occupancy is `clamp(S, 0, 1)`, each plane receives its proportional share of
that occupancy, and base receives the remainder. A 0.5/0.5 overlap therefore has
zero base contribution.

Plane target fitting samples original, unblurred YAB on a deterministic 4x4
stratified lattice. Two fixed Cauchy IRLS passes suppress incoherent texture and
isolated highlights. Gaussian images are used only for residual-scale analysis,
scale-persistent structure, and optional Local Softness.

The current Metal prototype solves screened membership propagation on a
quarter-resolution grid with fixed deterministic Jacobi iterations. It uses the
analytic screened-Poisson 10–90% width calibration and independent Y/AB lambda.
The host-independent reference verifies the canonical width, proxy scaling, and
pixel-aspect scaling within five percent. A full residual-correction multigrid
implementation remains a possible refinement if visual evaluation exposes
convergence or large-width problems.

## OFX and Nuke

General context exposes optional float RGBA **PlaneMap** in addition to the
existing effect Mask. PlaneMap is raw data: R/G/B/A map to Planes A/B/C/D and is
not unpremultiplied or color converted. Its bounds, RoD, and PAR must match Source.
Filter context remains single-input. Missing PlaneMap, Amount zero, Mix zero, and
Original comparison preserve exact identity for the final view.

The validation script creates an editable RGBA ownership map, renders the legacy
comparisons and Phase 3.2 diagnostics, and saves a 1920-by-1080 Viewer graph in
`tests/visual/PigmentValidation.nk`. Replace its Expression map with four painted
Roto/RotoPaint masks for artistic evaluation.

## Validation result

- All six CTest targets pass.
- Metal no-copy/staging, alpha/HDR identity, and Plane Map overlap checks pass.
- Nuke 17.0v1 discovers `OFXorg.painterlyofx.Pigment_v1`, reports three inputs,
  renders the Phase 3.2 path, and opens the saved Viewer scene normally.
- At 512 by 512 on Apple M2, measured Nuke Pictorial Planes renders were about
  0.36–0.39 seconds after warmup. The initial serial full-sample fit took about
  3.7 seconds; deterministic stratified fitting removed that bottleneck.
- The installed bundle is arm64, ad-hoc signed, and contains the exact current
  `Pigment.metallib`.

## Visual status

The engineering map proves that AB propagation can be visibly broader than Y,
overlap does not reintroduce base, and Fine/Medium extinction operates with Veil
and Local Softness disabled. It also shows that arbitrary, non-semantic plane maps
can create broad soft or halo-like target transitions. That is expected to be the
main visual risk during the next artist-authored map pass.

The artistic isolation gate is not declared passed by the procedural engineering
map. It requires painted cheek, shoulder, knee, and low-light maps aligned to the
actual pictorial ownership regions. Do not change the default comparison mode until
those renders are reviewed.
