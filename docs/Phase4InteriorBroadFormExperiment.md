# Gate C — support-aware interior broad-form experiment

Date: 2026-09-30. Starting checkpoint: `02aa63f`, branch
`codex/phase4-visual-basis`.

## Outcome

**Gate C remains failed.** Sparse supported regional-average constraints improve
broad-form retention without restoring fine texture, but do not remove the hazy
skin / patch-like cloth failure on the fashion diagnostic. A fourfold strength
test does not resolve it. This is a specific limitation of the tested interior
constraint, not evidence against the frozen latent basis or gradient-domain
reconstruction as a family.

The experiment is opt-in in the standalone harness. The normal Phase 4 CPU call
keeps interior constraints disabled until photographic acceptance. No default
comparison, OFX parameter, upstream algorithm, or existing output was promoted.
Gate D and Metal are not started. No bundle installation or Nuke launch occurred.

## Preserved upstream systems

A1/A2, corrected A3, public grouping/supports, and plate-conditioned Gate-B
hierarchies are unchanged. Both paired synthesis solves use the **same in-memory
automatic appearance, support fields, and hierarchy**. Thus the comparison does
not depend on independently rediscovering plates.

The stronger fashion rerun also produced byte-identical saved public alpha,
Y support, and AB support fields. No signed CMF weight is used for transport,
no occupancy fallback is introduced, and no rejected Phase 3 path executes.

## Constraint formulation

For channel family d, let C be corrected automatic plate appearance and s the
corresponding supportY/supportAB. Each patch a is a grid-geodesic interior region
contained in one selected chunk, with no retained-edge or unsupported crossing.

    mu[a,p] = s[p] / sum(p in a, s[p])
    t[a]    = sum(p in a, mu[a,p] * C[p])

The existing bounded objective becomes:

    E(u) = sum((p,q) in chunk, w[p,q] * ((u[q]-u[p])-g'[p,q])^2)
         + sum(a, tau[a] * (sum(p in a, mu[a,p]*u[p]) - t[a])^2)

This constrains a **regional moment**, not every original pixel. It creates no
blurred-source target image. Targets are direct measurements of the corrected
plate appearance, not Gaussian/bilateral/guided/WLS/TGV output. The patch edges
are not reconstruction barriers or feathered masks. Fine detail is not added
back as a residual. Values remain unclipped.

Retained contours, support boundaries, and RoD values remain fixed to automatic
appearance. Their contribution is subtracted from the moment target before
solving for free variables. Negligible-support areas retain automatic appearance.
The original primitive qualification and hierarchy-weighted gradient survival
equations are unchanged. Chunk Scale zero and Complexity one bypass exactly.

### Site selection and independent Y/AB settings

- Support threshold: 0.02; candidate-site support at least 0.1.
- First site maximizes depth from the fixed contour; subsequent sites use
  deterministic intrinsic farthest-point placement, separated by the spacing.
- Maximum 64 sites per chunk. This is a research budget, not a new plate count.
- Y spacing 32 rendered pixels, patch radius 12; strength 20.
- AB spacing 64 rendered pixels, patch radius 24; strength 5.
- Domains smaller than spacing squared / 2 are skipped. Sites must have enough
  contour depth to admit the patch; average patch support must be at least 0.1.
- tau = strength * supported patch mass / radius squared.
- Y/AB use their own chunk domains, supports, measurements, and solves.

The reported trials are 1x/PAR1. These research spacings are not yet certified
canonical controls across proxies/PAR; no such host claim is made.

### Numerical implementation

The new normal operator is A0 + sum(tau * mu * mu^T). Matrix-free application
adds the rank-one regional terms. An auxiliary sparse system avoids storing a
dense all-pairs patch matrix:

    [ A0   M       ]
    [ M^T -diag(1/tau) ]

Its Schur complement is exactly the positive-definite constrained Poisson
operator. A sparse LDLT factor supplies the existing CPU PCG correctness
preconditioner. True residual tolerance remains 1e-5, maximum 400 iterations.
All five photographic trials converged; maximum relative residual was 8.43e-15.
This is not a proposed performant Metal solver.

## Photographic evidence

Six plates, sixteen frozen vocabulary entries; Y/AB Chunk Scale 24/64;
Gradient Complexity 0.10; Spill zero; no Veil or Local Softness.

Cheek, shoulder, and knee are the supplied crops at their original dimensions.
Fashion uses the saved reconstructive Gate-A source roundtrip at 512 square.
Low-light/chroma uses the existing photographic engineering input resized to
512 square. No claim about full-resolution low-light acceptance is made.

| Fixture | Broad Y normalized RMSE, before → after | Fine Y energy/source, before → after | Moment footprint, alpha-weighted |
|---|---:|---:|---:|
| Cheek | 0.0994 → 0.0483 | 0.2907 → 0.2926 | 14.1% |
| Shoulder | 0.1136 → 0.0410 | 0.3010 → 0.3029 | 20.6% |
| Knee | 0.1690 → 0.0652 | 0.2286 → 0.2305 | 16.2% |
| Fashion | 0.1363 → 0.0960 | 0.3276 → 0.3281 | 17.8% |
| Low-light | 0.1167 → 0.1004 | 0.5621 → 0.5619 | 16.7% |

Gaussian sigma 1/4 bands are **measurement only**, never inputs to the render.
Footprint uses saved display masks/8-bit alpha and is approximate; constraint
influence extends beyond its measurement footprint through the bounded solve.
These metrics show broad-level improvement, not photographic acceptance.

The cheek and knee retain more interior volume at strong simplification than
the no-interior solve. Shoulder broad-level error also decreases. Nevertheless,
the fashion face remains hazy and the cloth remains a collection of patches
with isolated surviving texture. Low-light interiors do not show sufficient
new organization. These failures prevent a Gate-C pass. Fixed boundary values
do not alone certify final composite acuity or every silhouette peak.

### Strength control trial

Repeat fashion with strengthY 80 and strengthAB 20, keeping all other settings
identical. Broad Y error changes only from 0.0960 to 0.0942; fine Y energy remains
0.3281. Hazy skin and patch-like cloth remain visible. Therefore merely increasing
moment stiffness is not the missing broad-form representation.

## Diagnosis of this constraint — not a new filter proposal

1. Regional averages preserve levels but do not specify broad gradient direction
   or curvature between sites. Many different interiors satisfy the same means.
   Harmonic/attenuated-gradient interpolation still decides that missing shape.
2. The high-confidence, deep-interior test correctly rejects unsupported/thin
   regions, but leaves many cloth and facial subdomains without broad anchors.
   Stronger penalties cannot affect a domain with no useful measurement.
3. Tiny contour-delimited domains can retain isolated descriptive boundary facts
   while their interiors simplify, producing patch-like cloth. This experiment
   does not prove the frozen hierarchy is wrong or authorize replacing it.
4. Lowering spacing toward texture scale would fit more description, not establish
   the independent broad-volume constraint the user requested.

The smallest next **same-family** experiment would measure supported first spatial
moments / long-baseline gradient direction alongside each regional mean, qualify
them against plate appearance, and constrain those moments in the same bounded
solve. That would test whether explicitly represented broad direction/curvature
is sufficient. It is not implemented here. No new filter/decomposition family,
upstream reopening, boundary relaxation, or Spill workaround is justified.

## Saved evidence and reproduction

- `tests/visual/renders/phase4/gate-c-interior/{cheek,shoulder,knee,fashion,lowlight}/`
- `tests/visual/renders/phase4/gate-c-interior-strong/fashion/`
- `interior-comparison.png`: automatic source reconstruction, no-interior,
  constrained result, with identical plate/hierarchy inputs.
- Separate results, signed difference presentation, alpha/support/appearance,
  chunk/contour diagnostics, source/simplified gradients, primitive selection,
  fit error, Y/AB moment-footprint contact sheets, `poisson.csv`, `metrics.json`.
- Source, baseline, and constrained unclipped YAB PFM composites are retained.
- Full per-plate PFM targets and synthesized fields are in
  `build/phase4-c-interior/`; reproducible by the harness below.

    build/pigment_phase4_gate INPUT.ppm OUTPUT 16 6 24 64 .1 0 48 .15 .75 --interior-experiment

Use `--interior-strong` for the fourfold strength comparison. Without either
flag the experiment is disabled. Diagnostic contact scripts do presentation
conversion only; PFM composites retain negative/HDR values.

## Tests and status

Eight registered regression suites pass after rebuilding CPU targets. New
supporting tests cover curved-form improvement, texture attenuation, neutral AB,
independent Y strength, deterministic rerun, exact contour values, and exact
zero-scale bypass. Synthetic success is explicitly not a visual gate.

A1/A2/A3/Gate B remain preserved. Gate C is not frozen as passed. Gate D, Phase 4
Metal, parity, host validation, performance certification, and downstream
finishing remain deferred.
