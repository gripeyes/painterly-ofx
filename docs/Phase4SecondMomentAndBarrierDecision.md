# Gate C — final second-moment test and barrier-ablation decision

Date: 2026-09-30. Mean-plus-first checkpoint: **`4b3b293`**.

## Decision

**Stop the moment-based Gate-C path.** The controlled second-order extension
does not materially resolve hazy fashion skin or patch-like cloth. The following
low-persistence barrier ablation also fails to produce convincing continuous
photographic volume, despite substantial domain coalescence on fashion and knee.

The supported conclusion is **Gate C's current broad-field representation is
insufficient; low-persistence fragmentation is not the sole bottleneck**. This
does not prove that Gate B is optimal or that every possible domain construction
would fail. It does not justify a production hierarchy redesign. The next work
must reassess the broad-field representation, not add cubic/quartic moments,
denser anchors, stronger moment penalties, or downstream finishing.

A1/A2, corrected A3, public alphas/appearances/supports, and production Gate B
are unchanged. No Spill/Metal work, OFX installation, Nuke launch, or finishing
was used. Neither experiment is promoted to the normal Phase-4 renderer.

## Checkpoint and control

`4b3b293` commits the mean-plus-first implementation, report, tests, and five
saved photographic diagnostic sets. The second-order trial compares against
that formulation using the same in-memory A3 plates and hierarchy.

On all five fixtures, `first-only-yab.pfm` is byte-identical to the checkpoint's
saved mean-plus-first composite. Later, the fixed-barrier baseline for every
ablation run is byte-identical to the corresponding second-order composite.
Thus neither comparison quietly changes upstream discovery or appearance.

## Second-order statistics — no quadratic replacement image

Keep the previous qualified patches, centroid, mean term, first terms, source
gradient objective/survival equation, fixed contours, and support thresholds.
For patch a, let:

    mu[p] = support[p] / sum(support)
    center = sum(mu[p] * position[p])
    zX[p] = (x[p] - centerX) / patchRadius
    zY[p] = (y[p] - centerY) / patchRadius

Start with three centered-coordinate statistics:

    qXX = zX^2
    qXY = zX*zY
    qYY = zY^2

For stable comparable strengths, use the support-weighted inner product:

    <a,b> = sum(mu[p]*a[p]*b[p])

Apply two-pass weighted Gram–Schmidt in deterministic order against the
constant and first-coordinate directions, then previously accepted second
directions. Normalize each surviving statistic to unit weighted RMS. This
removes redundant mean/tilt content and conditions anisotropic supports. The
span remains the centered xx/xy/yy statistics together with the existing lower
terms; these are not a new image model.

Reject a direction if residual squared norm is below 1e-6 or below 1e-4 of its
original squared norm. Do not force a degenerate moment.

For each accepted normalized statistic psi:

    t = sum(mu[p]*psi[p]*automaticAppearance[p])
    E_second = tauSecond * (sum(mu[p]*psi[p]*u[p]) - t)^2
    tauSecond = secondStrength * supportedPatchMass / patchRadius^2

This adds sparse rank-one terms to the same bounded Poisson operator. Fixed
contour contributions are eliminated from the RHS. The statistic is measured
from corrected spatial plate appearance, not a filtered source. There is no
quadratic target image, fitted quadratic output, spline/RBF target, or newly
restored source residual. Values remain unclipped.

Y second strength is 20; AB is 1.25. Existing mean strengths remain 20/5 and
first strengths 20/2.5. Y/AB spacings and radii remain 32/12 and 64/24 rendered
pixels, with separate supports, domains, and solves. No strength/spacing sweep
or additional moment order was attempted.

## Same five photographic fixtures, Spill zero

Six plates; sixteen frozen vocabulary entries; Y/AB Chunk Scale 24/64;
Gradient Complexity 0.10. The supplied cheek/shoulder/knee crops retain original
dimensions. Fashion and low-light use the same 512-square fixtures as the
previous experiment. No full-resolution/proxy/PAR acceptance is implied.

Measurement-only second differences at 16/32/64-pixel spans provide xx/xy/yy
Hessian agreement, with Frobenius weighting of xy. These use unfiltered long
chords against the original source. Report relative Hessian error, correlation,
and energy ratio independently on Y/A/B. They can cross contours and cannot
certify photographic appearance. Existing Gaussian band metrics remain
measurement only; they never enter reconstruction.

| Fixture | Broad Y error, first → second | Hessian Y error at 32 px, first → second | Medium Y energy/source after second |
|---|---:|---:|---:|
| Cheek | 0.04530 → 0.04452 | 0.2642 → 0.2614 | 0.6084 |
| Shoulder | 0.03927 → 0.03834 | 0.3087 → 0.3058 | 0.5110 |
| Knee | 0.06382 → 0.06234 | 0.2633 → 0.2607 | 0.5610 |
| Fashion | 0.09296 → 0.09210 | 0.3162 → 0.3138 | 0.6166 |
| Low-light | 0.09732 → 0.09650 | 0.2873 → 0.2858 | 0.8051 |

The second-order result remains visually close to first-order. Fashion skin
still reads hazy; cloth still has disconnected patches and localized surviving
texture. The curves on the supplied crops receive small statistical corrections,
not the missing pictorial organization. No Gate-C pass is declared.

## Diagnostic-only barrier ablation

Run the same second-order Gate-C solver with the same A3 plate appearance,
alphas, supports, parameter settings, and source-gradient survival law. Do not
rebuild trees, change merge costs, or discover new plates.

Work on a **copy** of the selected plate Y/AB domain maps. For each originally
retained grid edge (p,q), define conservative persistence:

    bEdge = max(sourceBoundaryStrength[p], sourceBoundaryStrength[q])

At thresholds 0.25, 0.50, 0.75:

- Coalesce neighboring domains only across originally retained edges with
  bEdge <= threshold.
- All other originally retained edges remain hard solve barriers at the exact
  original coordinates. Hence bEdge > 0.75 remains protected in every trial.
- Keep original edge disappearance levels and the original gradient-survival
  equation. Newly admitted edges evaluate that same equation; no replacement
  gradient filter is introduced.
- A union can connect around the end of a protected edge. Mark that edge
  explicitly infinite so label coalescence cannot accidentally erase it.
- Explicit barriers exclude Poisson edges, fix endpoint appearance, and block
  supported patch traversal. Low-support safeguards remain active.

The original source cue is a multi-scale analysis cue, not a semantic guarantee
that every high-scoring edge is artistically important. This diagnostic tests
low-persistence removal under that frozen cue; it does not silently discard
strong edges to manufacture a success.

The alpha, Y support, and AB support fields remain fixed. No blur/dilation,
Spill, source-coordinate movement, or confidence fallback is used.

### Domain coalescence and protected contours

Plate-A domain counts (other plates are separately recorded in CSV):

| Fixture | Baseline Y/AB | Threshold .25 Y/AB | .50 Y/AB | .75 Y/AB |
|---|---:|---:|---:|---:|
| Cheek | 245 / 207 | 242 / 207 | 238 / 204 | 218 / 187 |
| Shoulder | 387 / 291 | 386 / 290 | 384 / 288 | 342 / 255 |
| Knee | 759 / 700 | 443 / 387 | 273 / 236 | 126 / 106 |
| Fashion | 4954 / 4890 | 2446 / 2388 | 1528 / 1481 | 776 / 750 |
| Low-light | 182 / 57 | 162 / 50 | 130 / 36 | 85 / 19 |

Every classified protected contour endpoint remains bit-exact to its automatic
plate appearance: **zero locked-value errors** across all plates, channels,
thresholds, and fixtures. All solves converge, maximum residual 8.57e-15.
These checks preserve the selected barriers; they do not independently certify
every final RGB silhouette peak.

Domain color views show coalesced labels. Internal protected edges can remain
inside one color label; the separate retained-contour views are authoritative
for those hard barriers.

### Appearance result

Fashion remains hazy and patch-like even after its substantial coalescence.
Knee domains become substantially larger without establishing the missing
continuous photographic broad form. Cheek/shoulder changes are slight;
low-light also does not yield a materially successful organization. No threshold
produces an accepted Gate-C result.

At threshold .75, broad normalized Y error changes from the fixed-barrier
second baseline as follows:

| Fixture | Fixed → ablated broad Y error |
|---|---:|
| Cheek | 0.04452 → 0.04911 |
| Shoulder | 0.03834 → 0.04163 |
| Knee | 0.06234 → 0.08113 |
| Fashion | 0.09210 → 0.09789 |
| Low-light | 0.09650 → 0.11861 |

Some intermediate thresholds improve individual knee measurements, but not the
photographic gate. This is why neither RMSE nor domain count alone decides
acceptance. Newly admitted source-gradient edges may also retain descriptive
information; a more photographic result from such survival would not establish
the requested broad-field mechanism.

## Residual failure and research boundary

Mean/tilt/curvature measurements constrain a finite set of regional statistics.
They do not specify the complete broad field between measurements or distinguish
all meaningful curved shading from descriptive gradients. The remaining
harmonic/attenuated-gradient solution can satisfy those statistics while still
reading hazy or patch-like. Enlarging low-persistence domains does not materially
resolve this failure in the controlled diagnostic.

Stop this statistical moment hierarchy here. A next research proposal must
explicitly explain how a different broad-field representation preserves
photographic curvature while extinguishing description inside fixed geometry.
Do not assume that more moments, more anchors, a Gate-B-only edit, or a stronger
downstream effect supplies that missing representation. No alternative family
is implemented in this change, and no Spectral Matting investigation is reopened.

## Saved evidence and reproduction

- `tests/visual/renders/phase4/gate-c-second-moments/{cheek,shoulder,knee,fashion,lowlight}/`
  contains original/first/second comparisons, unclipped composites, plate/support/
  latent/chunk/gradient diagnostics, `poisson.csv`, and `metrics.json`.
- `tests/visual/renders/phase4/gate-c-barrier-ablation/{fixture}/` contains the
  fixed-barrier baseline and `barrier-ablation.csv`.
- `barrier-{25,50,75}/` contains each diagnostic output, unclipped composite,
  Y/AB domain/retained-barrier contacts, difference/direction diagnostics, and
  metrics. Parent `barrier-ablation-comparison.png` shows source, fixed second
  baseline, and the three thresholds.
- Raw per-plate PFM inputs/results remain reproducible in the corresponding
  `build/phase4-c-second/` and `build/phase4-c-ablation/` harness directories.
- The fixed-barrier ablation baseline is byte-identical to the saved second
  result on all five fixtures.

    build/pigment_phase4_gate INPUT.ppm OUTPUT 16 6 24 64 .1 0 48 .15 .75 --second-moment-experiment
    build/pigment_phase4_gate INPUT.ppm OUTPUT 16 6 24 64 .1 0 48 .15 .75 --barrier-ablation

Both are research-only opt-ins. The ablation entry point returns before the
Spill scaffold. OFX never calls the diagnostic hierarchy-copy helper.

## Verification

All eight registered test suites pass after rebuilding CPU targets.
Supporting tests cover second-order curvature improvement on a known fixture,
finite/deterministic execution, Y/AB independence, retained-value preservation,
exact zero-scale bypass, copy-only ablation, and preservation of a hard contour
even when domains connect around its endpoint. Synthetic success is not a
photographic gate. `git diff --check` is clean.

Gate C has not passed. The moment path is closed; Gate D, Metal/parity, host
validation, and finishing remain deferred pending a different approved
broad-field hypothesis.
