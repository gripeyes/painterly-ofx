# Gate C — isolated regional Laplacian field experiment

Date: 2026-10-01. Starting checkpoint: `205de9b`.

## Decision

**The isolated regional eigenfield does not pass the photographic Gate-C test.**
Stop this experiment without increasing the mode budget or combining it with
the rejected moment/source-gradient machinery. Fashion remains decisive: low
mode counts produce hazy skin and patch-like cloth; higher counts recover more
form together with more medium-scale description, without resolving the cloth
organization. This is a failure of the tested boundary-lift plus local
low-frequency representation, not of Spectral Matting or the frozen A3 basis.

A1/A2, corrected A3, public plates and independent supports, and production
Gate B remain unchanged. The previous second-order and barrier-ablation
negative evidence remains preserved. No Spill, finishing, Metal port, OFX
installation, or Nuke run was used to rescue this result.

## Isolated formulation

`RegionalEigenField.*` is a standalone CPU-reference module. The harness
`--regional-eigen-sweep` branch returns before existing chunk synthesis,
moments, source-gradient survival, or Spill. It is not selected by the OFX
renderer and does not replace its current Gate-C implementation.

For each public plate and selected Y/AB chunk, use the corresponding frozen
support. Preserve automatic appearance exactly at:

- pixels with support below 0.02;
- RoD/image-border pixels;
- pixels adjacent to another selected chunk or negligible support.

Connected free interiors are solved independently. This explicitly preserves
both sides of retained contours; unsupported areas never acquire extrapolated
values. Original pixel coordinates and chunk labels do not change.

For a free interior, construct the geometry-only four-neighbor Dirichlet
Laplacian A. Its diagonal counts supported same-chunk neighbors, including
fixed neighbors; free neighbor entries are -1. Fixed neighbor automatic
appearance contributes to boundary RHS b. There are no color-dependent
operator weights, smoothing targets, fitted primitives, or moment penalties.

Compute the boundary lift and the lowest positive interior eigenpairs:

    A h = b
    A phi_k = lambda_k phi_k
    phi_k = 0 on the fixed boundary

Fit directly against the corrected automatic plate appearance t:

    W = diag(support)
    (Phi^T W Phi)c = Phi^T W(t-h)
    u = h + Phi c

The lift is harmonic and boundary-derived, not a globally filtered source.
The low-dimensional fit intrinsically excludes higher interior eigenmodes.
Values remain unclipped. Y and AB have separate domains, supports, lifts,
mode budgets, and coefficients. AB shares geometry between A and B only.
Public reconstruction uses the unchanged alpha weights.

Use dense Eigen solves for interiors of at most 48 pixels; larger interiors
use deterministic Spectra shift-invert solves with a sparse LDLT factor.
Mode signs are fixed by their largest absolute entry. The implementation
checks finite lift/fit values, positive eigenvalues, mode residuals, and
orthogonality. It does not claim that a geometry-only eigenmode is a
photographically meaningful form direction.

## Controlled photographic sweep

Four paired research budgets: **Y2/AB1, Y4/AB2, Y8/AB4, Y12/AB6**. All use
the same in-memory automatic plates and original selected hierarchy. Six
public plates, sixteen frozen vocabulary entries, Y/AB chunk cuts 24/64,
Spill zero. The legacy Gradient Complexity argument is unused in this branch.

Same fixtures as the prior experiments: cheek 582x492, shoulder 402x416,
knee 904x368, fashion 512x512, low-light/chroma 512x512. These research inputs
retain the harness's existing PPM/working-space interpretation. No production
proxy, PAR, gamut, or full-HD certification is inferred.

| Fixture | Medium Y energy/source, Y2 → Y12 | Broad normalized Y error, Y2 → Y12 | Visual finding |
|---|---:|---:|---|
| Cheek | 0.6045 → 0.6728 | 0.1044 → 0.0700 | Description falls, but low-mode cheek becomes soft/pale; increasing modes returns modelling rather than a decisive pictorial improvement. |
| Shoulder | 0.5103 → 0.6196 | 0.1089 → 0.0622 | Contours remain located; interior becomes subdued/patch-like, then description returns with modes. |
| Knee | 0.5081 → 0.5311 | 0.1541 → 0.1115 | Some broad curvature survives, but interior flattening and uneven residual description do not establish the required continuous form. |
| Fashion | 0.6383 → 0.7744 | 0.0901 → 0.0440 | Skin haze and disconnected cloth patches remain; more modes restore source description. Fails the decisive criterion. |
| Low-light/chroma | 0.8394 → 0.8945 | 0.1018 → 0.0659 | Geometry is stable but large fields lose interior character unevenly; greater fidelity is not improved organization. |

Band metrics use analysis-only Gaussian bands, never reconstruction targets.
Unfiltered long-baseline direction and second-difference diagnostics are also
saved. They are measurements, not field constraints. RMSE improvement does
not overrule the photographic failure. Review all four pairs, not only the
best-fitting pair.

## Numerical and preservation checks

- All nine registered tests pass, including existing legacy/Phase-4 tests
  and the new regional reference test. The existing Metal smoke test runs;
  no new artistic model is ported to Metal.
- A synthetic curved Dirichlet mode plus high-frequency oscillation recovers
  the broad field while removing oscillation. It also checks repeatability,
  neutral AB, unusual origin, exact fixed boundary values, and support holes.
  This is mathematical validation, not photographic acceptance.
- Across photographed modes, maximum absolute eigen residual is 4.85e-10.
  This is numerically sane; no further eigen-residual optimization is needed.
- 330 saved automatic alpha/support/appearance/chunk/retained/removed files
  are byte-identical to the previous second-order run across five fixtures.
  Thus this comparison does not quietly revise upstream appearance or B.
- Saved-data contour checks report zero changed retained endpoint, RoD border,
  and zero-support values across every plate, family, and pair. The checker
  uses binary contour markers and does not claim that 8-bit support exports
  certify the full 0.02 cutoff. The harness now includes the complete
  in-memory fixed-mask check for subsequent reproduction; the reference unit
  test also checks fixed values.
- Very small interiors use at most their available dimension. Complete-span
  interiors account for less than 0.18% of free supported Y mass at Y12 on
  every fixture. They cannot explain the dominant fashion failure.

Preserving contour endpoint values is stronger than merely drawing the same
labels, but does not certify every composite silhouette peak independently.
The appearance immediately inside a fixed contour can still read soft.

## Saved evidence and reproduction

Saved photographic evidence:

`tests/visual/renders/phase4/gate-c-regional-eigen/{cheek,shoulder,knee,fashion,lowlight}`

Each fixture contains source/automatic diagnostics, unchanged supports and
retained/chunk views, the four-way sweep contact, per-pair composite and
per-plate field contacts, unclipped source/composite YAB PFMs, signed
differences, direction maps, and metrics. Per-plate mode atlases and their
individual PGMs show every region's indexed eigenmode together. Each region
is peak-normalized independently **for presentation only**; these 8-bit maps
are not processing eigenvectors. Processing uses orthonormal double modes.

`regional-modes.csv` identifies plate, family, chunk, connected component,
interior size, mode, eigenvalue, residual, coefficients and lambda*c^2 energy.
Coefficients/energy are recorded for the largest Y12/AB6 fit; per-budget
support-weighted errors and used/requested counts are in `regional-fits.csv`.
`regional-retained-check.csv` preserves the saved-data exact-value check.

Complete per-plate unclipped fields and original harness diagnostics remain
in `build/phase4-c-regional-eigen`. Reproduce with the existing photographic
fixtures, for example:

    build/pigment_phase4_gate build/phase4-interior-fixtures/fashion.ppm \
      build/phase4-c-regional-eigen/fashion \
      16 6 24 64 .1 0 48 .15 .75 --regional-eigen-sweep

Render all five fixture names with identical settings. Contact/metrics helpers
do not modify processing arrays. No raw mode smoothing or artifact cleanup
is applied.

## Failure interpretation and next research boundary

The tested modes are low-frequency relative to each irregular domain, not
relative to a photographic broad-form model. A fixed per-domain count gives
different physical bandwidths in differently sized regions. In addition, the
mandatory harmonic lift retains all fixed-boundary appearance variation.
These properties plausibly contribute to uneven simplification and persistent
patch boundaries; the experiment does not separately prove their causal share.

The key observed tradeoff remains: a smaller interior span removes description
but does not preserve convincing fashion volume; a larger span restores both
form and unwanted description. This isolated family therefore has not broken
the tradeoff. Do not add modes, moments, denser anchors, more barrier ablation,
or selective gradient survival to manufacture a pass.

The smallest next research direction is the already identified **sparse
curve/field representation**, with explicit broad form/transition geometry and
retained-contour constraints (generalized-diffusion-curve-style raster fields).
That is a new hypothesis requiring its own controlled formulation, not a
claim that diffusion curves will succeed. It has not been implemented here.
Gate C remains failed; Gate D and Metal remain blocked.
