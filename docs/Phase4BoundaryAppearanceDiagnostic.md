# Gate C — retained contour location versus appearance

Date: 2026-10-01. Starting checkpoint: `4051868`.

## Decision

**Broad side-boundary values do not rescue the low-dimensional regional field.**
Close the regional-eigen family at the tested representation/budgets. Do not
increase modes, add moments, or re-ablate B. The experiment proves that exact
boundary appearance carries descriptive variation into the field, but removing
that variation does not restore convincing continuous photographic volume.

Fashion is visibly worse: fabric becomes larger flat blocks and facial form
becomes block-like/hazy. The supplied cheek and shoulder lose useful modelling;
knee receives little improvement. Low-light interiors flatten unevenly. The
changed boundary representation removes more medium information, but not in
the required pictorial organization. No photographic Gate-C pass is declared.

This is failure of the tested robust side-level/trend plus harmonic/eigenfield
approximation. It does not establish that every curve-based appearance
representation would fail. Fixed contour location and boundary appearance
remain conceptually distinct; exact source boundary values are not mandatory
for every future model merely because this alternative also failed.

## Controlled A/B comparison

- A: exact corrected-A3 contour-adjacent values; harmonic lift; Y2/AB1 modes.
- B: identical operator, chunk labels, supported free interiors and Y2/AB1
  budget; robust broad values fitted separately on each directed contour side.
- Both fit residual coefficients to the same corrected A3 interior appearance
  using the same support-weighted least squares. No additional gradients,
  moments, target images, or Spill enter either result.

All five original photographic fixtures and prior parameters are retained:
six public plates, sixteen vocabulary entries, Y/AB cuts 24/64, Spill zero.
No hierarchy discovery or artistic appearance parameters change. A1/A2,
corrected A3, public reconstruction alpha and supports remain frozen.

## Broad side values — exact formulation

For each plate and independently Y/AB, enumerate directed adjacent chunk pairs
(i,j). Collect contour-adjacent pixels **on side i only**, with support >=0.02,
and split disconnected traces by eight-connectivity. Eight-neighbor traversal
traces existing pixels; it does not dilate or reposition the contour.

For trace T, use support weights w and a support-weighted centroid. Obtain the
principal tangent direction from the weighted coordinate covariance. Define
normalized broad trend coordinate:

    t[p] = dot(direction, position[p]-centroid) / max(1, sqrt(lambda_max))

Y fits `a+b*t` if at least eight samples and coordinate variance >=4 px^2;
otherwise fit a constant. AB fits a constant independently for A and B.
This is one broad projected along-contour trend, **not** a detailed arclength
spline. Curved/hairpin traces can be inadequately represented; that limitation
is part of this specific approximation, not evidence against all side functions.

For each channel, perform three deterministic weighted LS passes. The first
uses support; subsequent passes use Cauchy weights:

    q = max(1e-6, 1.4826 * median(abs(residual)))
    wRobust[p] = w[p] / (1 + (residual[p]/(2*q))^2)

Use only a conditioning ridge of 1e-10*sum(w). At a junction pixel belonging to
several directed side traces, average the independently predicted **own-side**
values with support weights. No opposite-side samples are imported. Tiny traces
receive a robust level, not a forced trend. A single-sample trace can retain its
source value; no mask cleanup is performed.

Only supported retained-contour pixels change boundary appearance. Support-hole
boundaries and RoD borders elsewhere keep their safe original values. Contour
intersections with the RoD may receive their own side prediction. Boundary
values remain unclipped. Their harmonic lift is recomputed on the exact same
geometry-only Dirichlet operator. Mode shapes and fitted interiors do not
redefine contour locations or cross barriers.

## Photographic measurements and checks

| Fixture | Medium Y energy/source, exact → broad | Broad normalized Y error, exact → broad |
|---|---:|---:|
| Cheek | 0.6045 → 0.5179 | 0.1044 → 0.1154 |
| Shoulder | 0.5103 → 0.3856 | 0.1089 → 0.1172 |
| Knee | 0.5081 → 0.4888 | 0.1541 → 0.1569 |
| Fashion | 0.6383 → 0.4906 | 0.0901 → 0.1028 |
| Low-light/chroma | 0.8394 → 0.6843 | 0.1018 → 0.1130 |

These analysis bands are measurements only. Neither Gaussian analysis bands
nor RMSE are processing targets or artistic acceptance tests. Long-baseline
direction/curvature metrics and signed differences are also saved.

- All nine registered test suites pass. New synthetic checks confirm separate
  left/right appearance values, reduced boundary oscillation, deterministic
  side fits, and exact imposition of new values without cross-side mixing.
- 330 upstream alpha/support/appearance/chunk/retained/removed exports remain
  byte-identical to the frozen regional-eigen run.
- Exact-side photographic composites are bit-exact to previous Y2/AB1 outputs
  on four fixtures. Low-light max difference is 4.66e-10, RMSE 5.28e-13, from
  requesting only the two/one retained eigenpairs rather than twelve/six.
- Saved-data checks find zero boundary-appearance edits outside the frozen
  retained markers, zero imposed-boundary-value errors, and zero extrapolation
  errors at zero-support pixels on all plates/families. These do not claim that
  quantized support verifies every in-memory 0.02 threshold.
- No retained contour coordinate, adjacency edge, hierarchy topology, support,
  or alpha field changes. Same-location contours can nonetheless become less
  photographically readable when their appearance values are simplified.

## Saved evidence / reproduction

`tests/visual/renders/phase4/gate-c-boundary-appearance/{cheek,shoulder,knee,fashion,lowlight}`

Each includes Original / A / B comparison, unchanged public/support/chunk and
retained views, per-plate boundary appearance and reconstructed fields, unclipped
boundary/source/composite PFMs, coefficient/eigen diagnostics, direction maps,
signed differences, metrics, preservation JSON and boundary-check CSV.
Per-plate full reconstructed PFMs remain in the corresponding build directory.

    build/pigment_phase4_gate build/phase4-interior-fixtures/fashion.ppm \
      build/phase4-c-boundary-appearance/fashion \
      16 6 24 64 .1 0 48 .15 .75 --boundary-appearance-experiment

Use the same command for all five fixtures. `phase4_boundary_diagnostics.py`
saves numerical/image evidence and compares against `build/phase4-c-regional-eigen`.

## Next family, not another eigenfield correction

Move to a sparse curve/field representation with explicit broad interior
transition geometry and independent side values. A useful first isolation is
raster curve-controlled harmonic fields inside unchanged chunks, with sparse
level/transition curves from corrected plate appearance rather than statistical
moments or more eigenfunctions. Broad derivative constraints may be considered
only as a distinct documented model, not dense source-gradient restoration.

Jeschke's generalized diffusion curves offer control away from boundaries via
Laplace-function blending and a generalized edge-blur formulation. A simple
curve-constrained raster harmonic prototype would be related but **not** an
implementation of that full model. Do not label it GDCI merely because it uses
curves and a Laplacian. [Primary paper](https://pub.ista.ac.at/group_wojtan/projects/2016_Jeschke_GDCI/paper_preprint.pdf).

The next family must earn its own photographic pass. No A1–B redesign,
moment/eigen sweep, barrier ablation, Spill, or Metal follows this failed
boundary diagnostic.
