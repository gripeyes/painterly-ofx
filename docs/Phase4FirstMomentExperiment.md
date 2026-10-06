# Gate C — regional means plus first spatial moments

Date: 2026-09-30. Starting checkpoint: `12b14ac` (`4-gatec`).

## Outcome

**Gate C remains failed.** Supported first moments modestly improve broad
direction and broad-form error on all five photographic diagnostics. They do
not resolve hazy fashion skin or patch-like cloth. Mean plus first moments is
therefore a useful preserved experiment, not an accepted appearance model.

No second-order/quadratic moment, new target filter, upstream redesign, Spill,
Metal, bundle installation, or Nuke run was added. A1–B remain frozen.

## Exact extension to the existing bounded solve

The previously tested supported patches, mean constraints, geometry, and
strengths are unchanged. For patch a, define:

    mu[p] = support[p] / sum(support)
    centroid = sum(mu[p] * position[p])
    extentX = sqrt(sum(mu[p] * (x[p]-centroidX)^2))
    extentY = sqrt(sum(mu[p] * (y[p]-centroidY)^2))

For each nondegenerate axis j:

    v[j,p] = mu[p] * (position[j,p]-centroid[j]) / extent[j]
    t[j]   = sum(v[j,p] * automaticAppearance[p])

The added energy is:

    E_first(u) = sum(patches a, axes j,
                     tauFirst[a] * (sum(v[a,j,p]*u[p]) - t[a,j])^2)

These are centered first spatial moments of the **corrected automatic plate
appearance**. They do not require a solid/linear/radial fit to qualify. The
coordinate weights sum to zero, so a constant offset contributes no first
moment. The RMS extent normalization gives a comparable scene-value scale
without interpreting the statistic as a pixelwise target field. On anisotropic
patches, the moments are shape statistics, not a claim of an exact gradient
estimator; separate long-chord diagnostics measure direction independently.

The regional mean term and the existing gradient objective remain present.
Signed first-moment weights create positive-semidefinite rank-one terms; these
are not probabilities, graph affinities, or transport capacities. No altered
source image, blur, smoothing target, fine residual reintegration, or new
primitive model is introduced.

Retained contour values are eliminated from the first-moment RHS just as for
the regional means. The operator, auxiliary sparse Schur-complement solve,
support thresholds, selected domains, and barrier coordinates remain unchanged.
Near-absent areas retain automatic appearance. Axes with RMS extent less than
0.15 of patch radius are rejected as degenerate. No quadratic coordinate or
second-order statistic is measured or constrained.

## Y and AB settings

| Setting | Y | AB |
|---|---:|---:|
| Site spacing, rendered pixels | 32 | 64 |
| Patch radius | 12 | 24 |
| Existing mean strength | 20 | 5 |
| New first-moment strength | 20 | 2.5 |

    tauFirst = firstStrength * supportedPatchMass / radius^2

Y and AB retain independent support fields, chunks, moments, and solves. The
mean strength was not increased. Public alphas and supports are not modified.
Spacing remains a research setting at 1x/PAR1, not a certified proxy/PAR UI.

First moments are opt-in via `--first-moment-experiment`. Existing mean-only
experiment flags keep first strength zero; normal OFX calls remain unchanged.

## Controlled photographic comparison

Six plates; sixteen frozen latent vocabulary entries; Y/AB Chunk Scale 24/64;
Gradient Complexity 0.10; Spill zero. No Veil, Local Softness, or legacy path.

For each fixture the harness solves mean-only and mean-plus-first-moments using
the same in-memory automatic plates and hierarchy. The newly saved mean-only
PFM composites are **byte-identical** to the earlier experiment's saved outputs
on all five fixtures. This checks that the comparison has not silently changed
the frozen appearance or hierarchy behavior.

Cheek/shoulder/knee use original supplied crop dimensions. Fashion uses the
saved 512-square reconstructive source roundtrip. Low-light/chroma uses the
same 512-square engineering resize as the previous trial; this is not a
full-resolution low-light acceptance claim.

| Fixture | Broad Y error, mean → first | Broad Y angle error at 32 px, mean → first | Medium Y energy/source after first |
|---|---:|---:|---:|
| Cheek | 0.04828 → 0.04530 | 6.557° → 6.300° | 0.6054 |
| Shoulder | 0.04100 → 0.03927 | 8.495° → 8.395° | 0.5083 |
| Knee | 0.06520 → 0.06382 | 6.942° → 6.738° | 0.5575 |
| Fashion | 0.09600 → 0.09296 | 7.562° → 7.346° | 0.6148 |
| Low-light | 0.10039 → 0.09732 | 12.704° → 12.304° | 0.8046 |

Direction diagnostics use centered **unfiltered long chords**, spanning 16,
32, and 64 pixels, independently on Y/A/B. Saved statistics include
source-magnitude-weighted cosine, angle error, relative vector error, and
measured source-gradient fraction. They are measured against the original
source, not a blurred broad target. Chords can cross contours, so whole-frame
agreement is supporting evidence, not a silhouette/acuity certificate.

Fine/medium energy and broad RMSE retain the prior analysis-only Gaussian
sigma 1/4 measurements; they never enter rendering. Fine Y energy/source is
0.2927 cheek, 0.3031 shoulder, 0.2308 knee, 0.3279 fashion, 0.5619 low-light.
The directional improvement does not depend on reinstating fine texture.

All photographic solves converge, with maximum relative residual 8.58e-15.
Counts of first-axis constraints across plates/channels are 836 cheek,
484 shoulder, 1150 knee, 584 fashion, and 418 low-light. Numerical convergence
does not convert the appearance failure into a pass.

## Specific residual failure

The first moments help retain regional directional variation rather than just
levels. Their improvement is modest; photographic results remain very close
to the mean-only trial. The fashion face still reads hazy and cloth interiors
still read patch-like, with localized surviving texture. The supplied curved
knee remains recognizable but does not demonstrate enough new broad-curvature
organization to override that failure. Low-light remains insufficiently
reorganized. No claim of a clean Gate-C result is made on any fixture alone.

Mean and first moments leave substantial curvature freedom between supported
sites. A bounded harmonic/attenuated-gradient interior can satisfy the measured
levels and tilt without carrying the missing smooth photographic curvature.
The conservative patch qualification also leaves thin/small contour-delimited
regions without usable interior measurements. First-order penalties do not
change that coverage. These observations diagnose the tested constraint, not
the spectral basis, corrected A3, or Gate-B hierarchy.

**Only now is a second-order broad-form term a justified next consideration.**
The smallest next experiment would measure qualified centered second spatial
moments in the same supported domains and bounded operator, alongside the
preserved mean/first terms. That is a proposal, not an implementation or a
promise that curvature constraints solve the visual failure. It must retain
all barriers, avoid a globally fitted quadratic replacement image, and be
tested for saddles, patch seams, and airbrush character. No such term is added
in this change.

## Saved diagnostics and reproduction

`tests/visual/renders/phase4/gate-c-first-moments/{cheek,shoulder,knee,fashion,lowlight}/`
contains source/mean-only/first comparisons, unclipped composite PFMs,
reconstructed RGB, signed-difference presentation, latent/public diagnostics,
alpha/support/appearance contacts, chunk/contour and gradient diagnostics,
moment-footprint contacts, `poisson.csv`, and `metrics.json`.

- `first-moment-comparison.png`: original automatic reconstruction, mean-only,
  mean-plus-first result.
- `baseline-y-direction-error-{16,32,64}.png` and matching `result-*`:
  measurement-only angle error, black aligned / white reversed; weak source
  gradients and unmeasured borders are black. Do not interpret black as proof
  of preservation in a flat or unmeasured region.
- `poisson.csv`: separate mean/first counts and target residual RMS per channel.
- Full per-plate raw fields remain reproducible in `build/phase4-c-first/`.

    build/pigment_phase4_gate INPUT.ppm OUTPUT 16 6 24 64 .1 0 48 .15 .75 --first-moment-experiment

## Regression status

All eight registered suites pass after rebuilding CPU targets. Added supporting
tests prove first-moment participation, improved long-chord direction on a known
curved fixture, deterministic reruns, independent Y/AB strength, neutral AB,
fixed contour values, and exact zero-scale bypass. Synthetic fixtures do not
pass the photographic gate. `git diff --check` is clean.

The first-order result is preserved as opt-in CPU research. Gate C remains
failed; Spill, Phase 4 Metal, parity, and host validation remain deferred.
