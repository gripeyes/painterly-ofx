# Phase 4 Gate A2 — Progressive Recovery Result

Date: 2026-09-29

## Outcome

Gate A1 remains validated. Gate A2 does not pass the photographic acceptance gate, so A3, public-plate acceptance, Gate B, Gate C, Gate D, and Metal work were not started.

The global progressive Spectral Matting recovery and the approved local-window escalation fail in complementary ways on the shoulder fixture:

- Global recovery is soft, overlapping, deterministic, and materially occupied, but retains visible compression-cell structure in the latent vocabulary.
- Reference-style local-window recovery with deterministic overlap matching removes those cells, but collapses to seven active components with a 22.1% near-hard fraction and visible semantic/window-shaped ownership.

Neither result satisfies the A2 requirement of a distinct, soft, overlapping, spatially complex basis without grid cells, arbitrary window blobs, or semantic cutouts.

## Preserved validated work

- The symmetric closed-form matting Laplacian uses Eigen/Spectra smallest-eigenpair solving.
- The current CMF-augmented shoulder solve reaches maximum residual approximately `3.0e-14` and maximum cross-mode correlation approximately `4.1e-15`.
- `W_CMF` is represented by signed affine reconstruction coefficients.
- The separate transport affinity `F` is deterministically derived, nonnegative, and bounded to `[0,1]`. Only `F` is available to graph-geodesic support and future Spill.
- Progressive recovery starts with five components and proposes deterministic splits instead of instantiating sixteen unrelated slots at once.
- Requested maximum and achieved active count are separate diagnostics.
- Per-component occupancy, matting energy, correlation matrix, effective rank, overlap entropy, near-hard fraction, and eigenspace projection error are saved by the standalone harness.
- Y/AB support extent no longer depends on Luma/Chroma Chunk Scale. `Chroma Support Ratio` controls the independent chroma support multiplier.

## Photographic measurements

The final global progressive runs used a requested maximum of 16 latent components.

| Fixture | Active | Effective/pixel | Effective rank | Near-hard | Projection error | A1 max residual |
|---|---:|---:|---:|---:|---:|---:|
| Cheek | 15 | 2.98 | 1.45 | 0.91% | 0.00280 | 2.94e-13 |
| Shoulder | 16 | 3.55 | 2.19 | 10.41% | 0.00241 | 7.33e-14 |
| Knee | 16 | 3.65 | 4.76 | 0.05% | 0.00169 | 1.10e-13 |
| Low-light/chroma | 15 | 2.59 | 3.56 | 0.25% | 0.00671 | 4.43e-13 |

The native-resolution shoulder comparison with stronger analysis conditioning produced 15 active components, approximately 2.07 effective components per pixel, effective rank 2.95, 1.46% near-hard pixels, and A1 maximum residual `3.00e-14`. It still showed visible cell structure.

The standalone local-window baseline used the accessible reference-style closed-form matting implementation, six components per overlapping window, and deterministic overlap-correlation matching. It produced seven stitched components, approximately 1.57 effective components per pixel, and a 22.08% near-hard fraction.

## Saved evidence

- `tests/visual/renders/phase4/gate-a2/shoulder-eigenmodes-native.png`
- `tests/visual/renders/phase4/gate-a2/shoulder-latents-native.png`
- `tests/visual/renders/phase4/gate-a2/shoulder-reference-local-window-stitched.png`
- `tests/visual/renders/phase4/gate-a2/cheek-latents.png`
- `tests/visual/renders/phase4/gate-a2/knee-latents.png`
- `tests/visual/renders/phase4/gate-a2/lowlight-latents.png`
- `tests/visual/renders/phase4/gate-a2/shoulder-component-occupancy.csv`
- `tests/visual/renders/phase4/gate-a2/shoulder-component-correlation.csv`

## Smallest next research change

The approved progressive/global and local-window recovery escalations are exhausted for this formulation. The smallest next change is a compression/noise-aware matting affinity, validated independently before changing recovery again. Concretely, replace single-pixel local covariance evidence with a patch-distribution or explicit sensor/compression-noise model while preserving:

- the symmetric Laplacian and validated eigensolver;
- signed `W_CMF` versus nonnegative `F` separation;
- progressive component splitting;
- sum-to-one fractional mattes;
- the existing photographic diagnostics.

This is a new research change, not a downstream cleanup filter. Smoothing recovered masks, proceeding to hierarchy construction, or relying on later synthesis would hide the failed foundation and is explicitly rejected.
