# Phase 4 visual-basis checkpoint

This checkpoint records the forensic recovery requested after the Gate-A2
research loop.  The source of truth is the photographic output, with numerical
residuals used only to establish that the eigensolve is sane.

## Provenance

The last useful photographic sheets were generated between 15:47 and 15:52 on
2026-09-29, immediately before commit `44e57e8` (`4-init-midway`).  The harness
file naming establishes the pipeline stage without relying on visual inference:

- `eigen-*.pgm`, assembled into the earlier eigen contact sheets, are A1 signed
  Eigen/Spectra eigenmodes.
- `latent-*.pgm` and `*-latents.png` are A2 non-negative constrained component
  alphas.
- `plate-*-alpha.pgm` and `*-plates.png` are public plate alphas.  These were
  already less successful because several plates were similar; they are not the
  basis being declared successful.
- `latent-appearance-*.ppm` and `*-appearance.png` are A3 prototype appearance
  contributions, not alpha or eigenmode diagnostics.

The recovered visual configuration is deliberately small:

1. deterministic full-resolution analysis for crops up to 512 pixels;
2. the exact symmetric closed-form matting Laplacian;
3. Eigen/Spectra smallest-eigenpair solution with 24 retained modes for the
   default 16-component vocabulary;
4. no CMF augmentation inside the frozen eigensolve;
5. the simple 12-iteration constrained component transform with exponent 1;
6. no spectral-input conditioning, mask smoothing, morphology, occupancy
   optimization, local-window stitching, or post-filtering.

The signed affine `W_CMF` and non-negative transport affinity `F` remain
separate in the analysis graph.  They are retained for reconstruction and
downstream support/spill, respectively, but they do not alter the frozen A1
operator.

## Reproduction

The saved `gate-a-frozen` contacts cover cheek, shoulder, knee, low-light, and
fashion fixtures.  They show that A1 contains distinct face, fabric,
background, highlight, shadow, and scale-dependent modes.  A2 is intentionally
accepted as an internal vocabulary: it contains weak and imperfect components,
but also multiple materially occupied, soft, spatially complex structures.

The five runs have maximum Eigen residuals between approximately `1.36e-13`
and `4.68e-12`.  They request and retain 16 vocabulary entries, with effective
per-pixel overlap between approximately `1.89` and `2.55`.  These numbers are
recorded for regression diagnosis, not as an objective to optimize further.

## Freeze rule

A1/A2 may only be reopened if a defect materially survives into public plate
alpha, Y/AB support, public appearance, or reconstructed Pigment output and
cannot be corrected at A3/public grouping.  Raw eigenmode or latent appearance
alone is not a failure condition.

