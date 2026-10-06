# Phase 4 continuation — chunk synthesis not accepted

Date: 2026-09-30. Research branch: `codex/phase4-visual-basis`.

## Outcome

**Gate C has not passed the photographic appearance gate.** The approved
solid/linear/radial and bounded source-gradient formulations tested here do not
yet give convincing broad pictorial shading without softened/patch-like
appearance. No new spectral research is justified by this failure.

Gate D is not accepted or ready to port. The existing `PlateSpill` file is a
provisional scaffold, not a validated implementation of the approved directed
transport system. Phase 4 Metal work has not begun. The normal OFX bundle was
not rebuilt, installed, signed, or launched in Nuke during this continuation.

## Preserved foundation

- Frozen Eigen/Spectra and constrained-alpha basis: `2266872`.
- Earlier public representation checkpoint: `591ee62`.
- Corrected fixed-alpha spatial appearance checkpoint: `4f4a2cf`.
- No changes to the eigensolve or component recovery during this continuation.
- On cheek, shoulder, and knee, saved latent alpha and public
  alpha/Y-support/AB-support fields were byte-identical to the frozen baseline.
- Signed reconstruction `W_CMF` and nonnegative transport `F` remain separate.
- Neither confidence nor error creates source/base occupancy.

The previous A3 implementation made nearly identical local source copies in
different layers. It was corrected using stored local Gaussian color
distributions and fixed-alpha conditional color refinement, including original
full-resolution reconstruction. The standalone V-Sense SCU comparison and exact
provenance are in `Phase4A3ReferenceComparison.md`. No reference code was
integrated. Five original-size A3 fixtures reconstruct with YAB RMSE of roughly
`2.5e-9–1.2e-8`; this is numerical consistency, not proof of final artistic
success. Alphas were not redesigned to make the appearance solve easier.

## Hierarchy corrections and evidence

The provisional atomic partition was a low-edge threshold, not the specified
watershed. It was replaced with marker-controlled minimum-spanning-forest
flooding from regional minima of the source contour topography. This is still a
raster/UCM-style approximation, not gPb/OWT reproduction.

One shared source atomic RAG supplies separate growing Y and AB merge trees for
every public plate. Costs are recomputed from merged support-weighted appearance
and gradient statistics. Unsupported neighboring appearance cannot introduce a
strong conditional gradient. Grid-edge disappearance levels use tree LCAs;
same-atomic edges are zero, RoD outgoing edges infinity. Cuts never relocate
source grid edges.

The initial linear Internal Variation tolerance dominated the Ward increment.
On cheek Plate A, 10,393 atoms became 222 chunks at Scale 1 and 210 at Scale 128.
Disabling tolerance restored graded behavior. The same approved merge energy
now uses a conservative onset:

    T = 0.15 * clamp(InternalVariation, 0, 1)^4

The upper endpoint remains unchanged; this is control calibration, not a new
merge energy. Default cheek Plate-A Y counts at scales 0/1/8/24/64/128 are now
10,393/1,235/364/245/219/212. Knee counts are
12,355/2,302/863/759/735/730. Y and AB cuts remain independent of support radii.

The photographic sweep covers cheek, shoulder, knee, fashion, and a 512-pixel
low-light engineering resize. It checks every plate for nested chunk counts
and retained-contour subsets: no new retained pixels appear as scale grows.
Source overlays visibly distinguish internal boundary removal from contour
movement. These construction checks do **not** certify every silhouette peak,
render scale, PAR, or host behavior. Do not promote this to a full A–D pass.

Saved: `tests/visual/renders/phase4/gate-b-hierarchy-calibrated/`.

## Synthesis implementation checked before artistic rejection

- Support threshold 0.02; absent areas retain automatic appearance.
- Retained contours, support boundaries, and RoD borders use fixed automatic
  appearance; adjacent retained chunks contribute no samples.
- Solid, affine, and nine radial-center candidates; rejected primitives do not
  secretly become final ramps.
- The approved hierarchy-weighted survival equation applies to **all** models,
  including accepted primitives. Its former primitive bypass was a bug.
- Increasing Gradient Complexity tightens, rather than loosens, primitive
  qualification. Complexity 1 and Chunk Scale 0 are exact appearance bypasses.
- Primitive qualification analyzes supported 8/16-pixel source chords and
  second-difference oscillatory variance, so fine texture does not itself
  disqualify a broad field. This changes analysis only: no alpha filtering,
  compression-aware matting, source-image blur, or output rescue is added.
- Matrix-free bounded Poisson with sparse LDLT PCG preconditioning; true
  residual checked, tolerance `1e-5`, cap 400. Current fixture solves converge
  in one preconditioned iteration at approximately `1e-14`. This expensive
  CPU correctness preconditioner is not a proposed Metal implementation.
- Failed convergence produces a render failure, never an accepted preview.

Synthetic regressions now prove that affine and solid hypotheses retain the
requested oscillation fraction. They are supporting checks, not visual gates.

## Photographic result

Isolation: six plates, sixteen latent vocabulary entries, Y/AB cuts 24/64,
Gradient Complexity 0.35, Spill 0. No legacy mass, Veil, Source Smooth, TGV,
cleanup, Gaussian output smoothing, or PlaneMap processing.

Cheek, shoulder, knee, fashion, and low-light previews and unclipped diagnostics
were saved. The principal crops remain recognizable, but the synthesis reads
primarily as regional contrast/detail attenuation. Fashion exposes flat/patchy
cloth regions and softened skin instead of the intended natural broad planes.
The result is not sufficient to declare the new pictorial mechanism successful.

At Complexity 0.70, cheek/knee/fashion retain more photographic description but
do not establish the missing simplification. At 1, bit-exact bypass restores the
automatic appearance; that is not an artistic solution.

Supporting whole-frame Y energy measurements on fashion:

| Complexity | Fine energy/source | Medium energy/source | Broad normalized RMSE |
|---|---:|---:|---:|
| 0.35 | 0.420 | 0.687 | 0.0984 |
| 0.70 | 0.672 | 0.835 | 0.0454 |

Bands use analysis-only Gaussian measurements at sigma 1/4; no such image is
used by reconstruction. The numbers confirm attenuation, not organization.
Ownership-gradient metrics use saved 8-bit alpha and are explicitly diagnostic,
not float parity measurements. Stored PFM appearance/output values are
unclipped. Negative layer colors are permitted and not evidence of clipping.

Latest evidence:

- `tests/visual/renders/phase4/gate-c-broadqualification/`
- `tests/visual/renders/phase4/gate-c-broadqualification70/`
- `metrics.json`, `poisson.csv`, fit/model, source/simplified-gradient contacts,
  pre-Spill result, and layer diagnostics accompany the saved previews.

## Specific formulation limit and smallest next research change

In the current fallback, when no primitive explains the chunk, its broad
gradient baseline is zero. Outer-contour Dirichlet constraints supply a
harmonic interior and the approved residual survival supplies scaled source
gradients. Within one atomic region `ell_e=0`, so the same Complexity floor
scales both useful interior form and unwanted description. Outer constraints
alone do not provide independent control over naturally curved interior
photographic volume. The permitted primitive alternatives and corrected
fallback tested here have not resolved that tradeoff.

**The next experiment needs an explicit, support-aware broad interior gradient
representation**, rather than more residual/occupancy optimization. The smallest
extension is to preserve selected scale-persistent source broad-form gradient
constraints/critical structures inside each chunk, then use the same bounded
Poisson solve to extinguish descriptive residual gradients independently.
Specify and validate those interior constraints before adding them; they are
not present in the current outer-contour-only formulation. Keep plates,
hierarchy topology, retained contours, alpha, and pixel coordinates frozen.

This is a failure of the tested synthesis approximation, not evidence against
Spectral Matting, SCU, hierarchical segmentation, Fattal, or gradient-domain
reconstruction as research families. It is not a proof that every possible
parameter or Poisson formulation fails. Do not reopen A1/A2 or use Spill, Veil,
blur, or a Metal port to conceal the failed appearance gate.

## Regression status and remaining boundaries

Eight registered tests pass after rebuilding the CPU regression targets,
including legacy core/mass/DetailCollapse/Integrated/Phase3.2/Phase3.3 tests and
the Phase4 supporting suite. Existing Metal smoke also passes, but it does not
test a Phase4 GPU renderer. `git diff --check` is clean.

Full Phase4 proxy/PAR certification, creative-control analysis caching,
nonnegative directed Spill transport, Phase4 Metal parity, and full Nuke
validation remain incomplete. Preserve the research scaffold and diagnostics;
do not label this branch a completed Phase4 product.
