# Pigment Phase 4: Automatic Plate Graph

Phase 4 is an isolated research comparison inside the persistent
`org.painterlyofx.Pigment` OFX effect. It starts from straight source RGB and does
not execute the Phase 3 massing, quadratic-plane, Source Smooth, TGV, Veil, or
PlaneMap paths.

## Gate policy

Development is deliberately sequential:

1. Gate A validates the automatic fuzzy latent basis, spatially varying latent
   appearance, and grouping into overlapping public plates.
2. Gate B adds the explicit source-boundary and region-merging hierarchies.
3. Gate C adds contour-bounded gradient synthesis.
4. Gate D adds directed graph spill.
5. Metal work starts only after all four CPU gates pass.

If one gate fails artistically, later stages are not implemented to disguise the
failure. A failure in the reduced spectral/component-recovery approximation is not
reported as a failure of Spectral Matting or SCU as research families.

## Current Gate-A implementation

The host-independent CPU branch provides:

- a half-resolution, long-edge-capped source-affinity graph;
- deterministic low-energy graph coordinates;
- non-negative, sum-to-one fuzzy latent components;
- spatially varying per-component YAB appearance and reconstruction diagnostics;
- soft latent-to-public assignment for four to eight public plates;
- independent reconstruction alpha, Y support, and AB support fields;
- graph-geodesic support propagation controlled by Plate Scale and Plate Overlap;
- exact Amount/Mix identity, mask gating, alpha preservation, straight/premultiplied
  handling, HDR/negative values, unusual origins, render scale, and PAR behavior.

The current spectral solve and simplex recovery are deliberately compact Gate-A
approximations. They must pass the saved visual diagnostics before the region
hierarchy is added.

## Reproducing Gate A in Nuke

Build and sign the normal bundle, then run:

```sh
cmake --build build --target sign-local
PIGMENT_NUKE_OUTPUT=/tmp/pigment-phase4-gate-a \
  /Applications/Nuke17.0v1/Nuke17.0v1.app/Contents/MacOS/Nuke17.0 \
  -t tests/nuke_phase4_gate.py
```

The script creates the normal `Pigment` OFX node, selects comparison index 6,
disables chunk synthesis and spill, saves per-latent and per-public-plate images,
and writes `tests/visual/PigmentPhase4GateA.nk` for interactive inspection.

## Gate-A decision record

This section is updated after the native OFX diagnostic renders have been
inspected. Gate B must not begin before it records a pass.
