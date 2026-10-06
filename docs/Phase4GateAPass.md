# Phase 4 Gate A checkpoint

Gate A is checkpointed after restoring and freezing the earlier Eigen/Spectral
visual vocabulary.  The public photographic representation, rather than raw
latent cosmetics, is the acceptance surface.

## Frozen upstream basis

- A1 uses the exact symmetric closed-form matting Laplacian and Eigen/Spectra.
- A2 uses the recovered simple constrained transform recorded in
  `Phase4VisualBasisCheckpoint.md`.
- Signed `W_CMF` and non-negative transport affinity `F` remain distinct.
- No compression-aware affinity, alpha blur, morphology, or local-window
  stitching is used.

## A3 and public plates

Each latent component now carries a local, spatially varying YAB distribution
estimated over non-negative local information-flow edges.  A per-pixel
reconstruction correction preserves the alpha-weighted source exactly without
reducing a component to a centroid color.

Canonical public anchors are held to their slots during latent grouping.  This
prevents the previous over-entropic drift into equal plates while retaining
soft alpha through the fractional latent fields.  Reconstruction alpha,
`supportY`, `supportAB`, spatial appearance, signed reconstruction flow, and
non-negative transport remain separate data.

Across cheek, shoulder, knee, low-light, and fashion fixtures:

- A3 reconstruction RMSE is approximately `5.5e-9` to `3.8e-8`.
- local appearance spatial variation is approximately `0.053` to `0.119`.
- public reconstruction RMSE is approximately `6.0e-9` to `4.0e-8`.
- public effective rank is approximately `1.86` to `3.91`.
- maximum public-alpha correlation is approximately `0.26` to `0.58`.

The saved contacts show distinct, overlapping face, fabric, illumination,
shadow, and background contributions.  AB support extends farther than Y
support while neither is normalized as reconstruction ownership.  The public
alpha/support diagnostics retain some source compression structure on the
noisy shoulder crop, but it cancels in the reconstructive composite and does
not materially survive in the Gate-A image.  Per the approved policy, this is
not grounds for reopening A1/A2 or adding compression-specific matting.

Gate A is therefore sufficient for the plate-conditioned hierarchy stage and
is frozen.  It may only be reopened if a downstream public/output defect is
demonstrably inherited from this basis and cannot be corrected in the stage
that exposes it.

