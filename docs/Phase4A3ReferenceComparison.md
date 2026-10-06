# Phase 4 A3 appearance comparison — 2026-09-30

This continuation preserves the frozen Eigen/Spectra eigenspace and constrained
latent recovery. It does not restart Spectral Matting research.

## Standalone SCU baseline

The accessible V-Sense C++ implementation of Aksoy et al.'s soft color
segmentation was built and run **outside Pigment**:

- Repository: <https://github.com/V-Sense/soft_segmentation>
- Revision: `a0e2717199787633eb849b5e66a59e9fd7f53b31`
- Standalone working directory: `/tmp/pigment-scu-baseline.xZ16oL`
- OpenCV 4.12.0 with ximgproc, built locally; no system installation.
- Compatibility changes: OpenCV enum names, explicit matrix-to-vector
  conversion, modern CMake minimum, removal of an unused Linux CPM binary link.
- No segmentation, unmixing, matte-regularization, or refinement algorithm was
  changed. No reference implementation code was copied into Pigment.
- Input: the same five photographic fixture images, resized to 128 pixels wide;
  tau 11. Native reference alpha extraction was used, not Pigment's frozen alpha.

| Fixture | Reference layers | Composite RGB RMSE, 0–255 |
|---|---:|---:|
| Cheek | 3 | 0.631 |
| Shoulder | 3 | 0.662 |
| Knee | 3 | 0.632 |
| Fashion | 4 | 0.597 |
| Low-light | 3 | 0.565 |

Saved source, layer-color/alpha contact sheets, reconstructed composites, and
per-layer occupancy/spatial variation are in
`tests/visual/renders/phase4/scu-reference/`.

The reference produces different layer appearances, including dark and chromatic
layers alongside a layer that retains photographic variation. This is evidence
that appearance unmixing is useful on these fixtures, **not** validation of
Pigment's implementation or a parity test. The reference uses 8-bit RGB and its
own alpha solution; Pigment uses unclipped YAB and the frozen spectral alphas.
Reference alpha regularization is not integrated into Pigment.

## Actual A3 defect

The previous tiny-neighborhood conditional means made most component colors
nearly identical copies of the source. A large spatial-variation score relative
to a global mean did not prove genuine layer separation. A denominator-only
conditioning term additionally biased nearly absent distributions toward zero.

Unclipped diagnostics ruled out large opposing-color cancellation as the main
automatic-composite problem: on the knee, automatic ownership-gradient energy
was approximately 0.0032 versus 49.93 appearance-gradient energy in Y. Chunk
synthesis increased the ownership term to approximately 0.889 and exposed
rectangular latent structure. Neither improving Poisson residuals nor fixing
the zero-biased conditioning alone removed it.

## Smallest appearance-stage escalation implemented

Under **unchanged alpha**, estimate full YAB Gaussian distributions on overlapping
analysis cells (32-pixel spacing, 32-pixel support half-width). Retain their means
and conditioned 3×3 covariance matrices with the latent representation. Bilinear
interpolation is of distribution parameters, not alpha filtering or source-image
output smoothing.

For each pixel, solve the fixed-alpha Gaussian color refinement:

    minimize Σ alpha_i (C_i - mu_i)^T Sigma_i^-1 (C_i - mu_i)
    subject to Σ alpha_i C_i = source

Its conditional solution is:

    residual = source - Σ alpha_i mu_i
    G = Σ alpha_i Sigma_i
    C_i = mu_i + Sigma_i G^-1 residual

This is an independently implemented conditional color-unmixing formulation,
not a claim to reproduce the complete SCU optimizer. Alpha sparsity, color-model
search, and reference matte filtering are not imported. HDR and negative colors
are not clipped. Confidence never changes occupancy.

After analysis-grid extension, repeat fixed-alpha color refinement against the
original full-resolution source, then regroup the refined latent contributions.
This corrects interpolation/product error and avoids returning an
analysis-resolution photograph. The full-resolution error is now recorded
separately from the analysis-grid error.

The standalone harness exports unclipped YAB PFM values and the metrics helper
separates ownership-gradient and appearance-gradient contributions. The new
`--gate-a-only` path saves diagnostics without running chunk synthesis or Spill.

## Gate status

The five original-size fixtures have been rerun. Full-resolution YAB composite
RMSE is `8.07e-9` (cheek), `3.59e-9` (shoulder), `9.71e-9` (knee), `1.21e-8`
(fashion), and `2.50e-9` (low-light). Spatial inter-layer RMS on the knee is
approximately `[0.07346, 0.00227, 0.01167]` in Y/A/B: the conditional appearances
are no longer identical local source copies. Covariances and local means are
retained as part of the latent representation, not discarded after refinement.

All 16 latent alpha diagnostics and all six public alpha/Y-support/AB-support
diagnostics on cheek, shoulder and knee were compared byte-for-byte with the
frozen checkpoint and were unchanged. A1/A2 were not reopened. Saved results
are in `tests/visual/renders/phase4/gate-a-distributions/`.

This checkpoints the A3 correction, **not a declaration that B–D pass**. In
particular, exact reconstruction is not proof that creative plate interaction
will be artifact-free. Public alpha on compressed dark crops retains rectangular
source structure; the chunk experiments must establish whether it survives
materially into the output. The prior `Phase4GateAPass.md` describes the earlier
appearance implementation and its metrics must not be used to validate this
revised formulation or the downstream stages.

Two downstream implementation defects were corrected separately: accepted
primitives incorrectly bypassed the specified gradient-survival equation, and
primitive tolerance increased rather than decreased with Gradient Complexity.
An affine-plus-oscillation regression now proves that qualified primitives
retain the requested residual fraction. The source atomic partition was also
only a low-edge threshold, not the specified watershed; marker-controlled
minimum-spanning-forest flooding is now under photographic evaluation. None of
these corrections changes latent extraction or appearance unmixing.

No Metal port, repeated installation/signing, or Nuke launch is justified by
unit-test passage alone.
