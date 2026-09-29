# Phase 3.3 — Soft Pictorial Plates checkpoint

## Implementation status

The CPU-first research gate is implemented in the persistent
`org.painterlyofx.Pigment` node as comparison index 5. Existing comparison and
debug indices remain unchanged.

Implemented host-independent components:

- explicit-occupancy four-plate representation;
- deterministic Auto Convex Palette reference (not Disney SCU/PCU);
- RGBXY-inspired Auto Spatial Layers retaining a triangular control mesh;
- the approved single-equation Hybrid guidance behavior;
- full-resolution residual-correction multigrid;
- Source Smooth weighted-least-squares shading;
- actual second-order TGV with step sizes derived from a conservative discrete
  operator-norm bound;
- conditional per-plane Y/AB WLS without membership double weighting;
- complete CPU RGB/RGBA rendering, premultiplication handling, alpha
  preservation, masks, debug views, and Nuke integration.

`Compute Backend` exposes Auto, Metal, and CPU Reference. Phase 3.3 Metal is
intentionally unavailable at this checkpoint: forcing Metal reports an explicit
error. Auto and CPU Reference use the deterministic CPU path. Legacy Pigment and
DetailCollapse Metal paths are unchanged.

## Validation

- All seven CTest suites pass.
- The normal signed arm64 bundle installs at
  `/Library/OFX/Plugins/Pigment.ofx.bundle` and passes strict codesign
  verification.
- Nuke 17.0v1 discovers `org.painterlyofx.Pigment`, creates it through the normal
  node factory, exposes Source/Mask/PlaneMap and all Phase 3.3 controls, and
  completes real Write renders.
- The focused gate harness is `tests/nuke_phase33_gate.py`.
- Saved validation renders are under `/tmp/pigment-phase33-gate` for this run.

Observed 512×512 CPU times on the M2 development machine:

| Branch | Approximate Nuke Write time |
|---|---:|
| Auto Convex Palette + Source Smooth | 3.0–3.2 s |
| Auto Convex Palette + TGV | 3.3–3.5 s |
| Auto Spatial Layers + Source Smooth | 1.0 s |
| Auto Spatial Layers + TGV | 1.3 s |

At 1920×1080, Auto Spatial Layers measured approximately 4.6 s with Source
Smooth and 6.1 s with TGV. These are correctness-reference timings, not
interactive performance.

## Artistic gate result: failed

The isolation test used full automatic occupancy, Veil 0, Local Softness 0,
multigrid transitions, and no legacy mass/Guided/Representative processing.

Auto Convex Palette produces predominantly hard color ownership rather than the
required useful overlapping pictorial contributions. This is a failure of the
simplified convex four-color approximation only; it is not evidence against the
full Aksoy/Disney SCU/PCU method.

Auto Spatial Layers preserves a richer mesh internally, but its public membership
fields visibly organize into coarse triangular/polygonal blobs. The mesh topology
is too legible and does not provide the desired pictorial organization.

Source Smooth and TGV retain source geometry, but at useful extinction settings the
fashion/skin result reads as an airbrushed or broadly softened photograph. Neither
branch produces the requested small set of coherent photographic tonal/chromatic
planes on its own. TGV avoids the quadratic-saddle failure but does not rescue the
plate model.

These outcomes meet the approved hard-stop conditions: near-hard/arbitrary plate
construction and airbrush-like reconstruction. Metal implementation and parity
work were therefore not started. Porting the failed formulation would make visual
iteration faster without validating the artistic hypothesis.

## Recommended next experiment

If automatic soft-color decomposition remains the desired direction, escalate only
Auto Convex Palette toward a more faithful SCU/PCU formulation: per-layer color
distributions, alternating opacity/color optimization, explicit opacity compactness,
and projected-color initialization. Keep the fixed four public plates, full
occupancy isolation test, source-derived photographic shading, and CPU-first gate.

For the spatial branch, do not merely refine the current triangular mesh. Replace
visible coarse mesh interpolation with an adaptive spatial-color control complex or
another representation whose basis does not imprint its topology on the output.
