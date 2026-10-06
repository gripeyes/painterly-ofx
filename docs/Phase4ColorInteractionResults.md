# Color Interaction comparison — CPU research

2026-10-01. Functioning CPU-reference Linear YAB, Density and Spectral Pigment are implemented and exposed in the existing node's Color Interaction group, in both Pigment and Research interfaces. Linear YAB remains default. Frozen A1/A2/A3/B, public alpha/supports, Gate-C baselines, repaired Spill graph and macro mappings are unchanged. Gate C remains unaccepted.

## Controlled inputs

Fashion, knee and lowlight replay the archived full-precision upstream snapshots. The harness refuses extraction when the snapshot is missing. C1 and C4 use saved float plate fields, validated against the same source/upstream/support/contour data. No spectral-matting solve or hierarchy rebuild was performed to populate the comparison.

Settings: Spill .8, Y .05, AB 1, Reach 128, Asymmetry .5, Respect .8, Pigment Density .5; C1 complexity .1. One prepared directed transport is reused across laws. Every case records **zero** maximum transport difference and **zero** interaction-influence difference from Linear. Changes therefore originate in the appearance law, not stronger local mixing or a changed graph. Alpha is never changed by these laws.

Evidence directories:

- `tests/visual/renders/phase4/comparative-pipeline/fashion/color-interaction`
- `tests/visual/renders/phase4/comparative-pipeline/knee/color-interaction`
- `tests/visual/renders/phase4/comparative-pipeline/lowlight/color-interaction`

Each contains C0/C1/C4 pre-Spill, all three post-Spill RGB previews and float YAB composites, signed float law differences, signed pre/post differences, compact comparison sheets, sampled material fits/residuals, and photograph-derived donor→receiver trajectory CSV/swatches at Density 0/.5/1 in both directions. PNG/PPM clipping is presentation only; signed PFM is authoritative for HDR/negative values. The established fixture pipeline assigns source bytes to linear ACEScg; this comparison deliberately preserves that convention, rather than claiming newly calibrated photographic input or reinterpreting old baselines.

## Numerical effect, not an acceptance metric

C0 opponent-chroma RMS change relative to Linear, at the identical spatial weights:

| Fixture | Density | Spectral Pigment |
|---|---:|---:|
| Fashion | .00117 | .00156 |
| Knee | .00363 | .00887 |
| Lowlight | .00365 | .00493 |

Typical unweighted sampled bounded-material spectral-fit error is about 1e-8 on fashion, 1.9e-4 on knee and .013 on lowlight. Out-of-model colors are retained through the explicit RGB residual, not clipped or replaced with source by confidence. This fit error is not an artistic score and is not evidence of measured-pigment accuracy.

Recorded CPU interaction times for these fixture sizes: Linear about .3 s, Density about .5 s, Spectral about 5–11 s. These are interaction-only runs with saved inputs; they exclude upstream extraction. They are not warm Nuke or Metal performance claims.

## Photographic judgment

Density provides modest, stable cool/neutralizing changes and darker mixtures on the sampled trajectories, without the spectral path's conspicuous speckles. It is a useful inexpensive comparison but has not demonstrated a decisive pictorial improvement across all three fixtures.

Spectral Pigment gives stronger cyan/dark interaction and nonlinear donor trajectories, but the knee introduces objectionable dark/color speckles, especially in already difficult shadow areas. Residual/out-of-gamut interaction and independent Y/AB recombination require further diagnosis before selecting it. A compact representative spectrum plus equal scattering does not identify actual paints; failure of this approximation is not failure of Mixbox, measured K–M, or Jakob/Hanika's validated upsampling implementation.

Fashion's C1 haze and C4 cloth patchiness remain under every law. Color interaction is not accepted as a repair for those spatial failures. **No nonlinear law is promoted or selected for acceleration from this run.** Linear remains the reference/default; Density and Spectral remain functioning hands-on research choices. Metal porting this new layer is withheld until photographic evidence identifies a worthwhile law.

## Rejected normalization diagnostic

The first implementation scaled chroma by a signed luminance ratio. Dark/negative residual cancellation caused division-driven chroma amplification. Those initial outputs are preserved in adjacent `color-interaction-initial-normalization` directories. The corrected implementation uses additive neutral scene-light residuals, never division by scene luminance. Linear float outputs are byte-identical between both runs. No source clipping, spectral-mask smoothing or topology change was used to fix that engineering defect.

## Node and validation

Pigment Density 0 preserves the linear Y interaction exactly; 1 allows the material law's luminance. AB-only Spill leaves composite Y bit-exact even at Density 1. Changing law/Density reuses the cached upstream and Gate-C fields. Gamut/white conversion returns through XYZ to the selected scene-linear primaries; there is no DRT, display transfer, film curve or grain.

All 12 regression suites pass, including reversible HDR/negative material encoding in all four working gamuts, endpoints, finite signed-luminance cancellation, law determinism, fixed alpha/transport/influence, AB-only Y preservation, zero-Spill identity and cache reuse. The Linear float baseline is byte-identical across the two normalization diagnostics. Synthetic tests do not select the photographic winner. The existing arm64 bundle was rebuilt, signed and installed; its signature verifies and its module hash matches the signed build.

Prepared three-law scene: `tests/visual/PigmentColorInteraction.nk`. The existing host validation script checks the new knobs and spectral-law preservation through Copy to Research. Nuke host execution remains blocked by the previously verified license failure. Compilation, CPU comparisons and signature verification do not certify host rendering.

Reproduce without recomputing upstream:

```sh
build/pigment_phase4_gate build/phase4-interior-fixtures/fashion.ppm tests/visual/renders/phase4/comparative-pipeline/fashion 16 6 24 64 .1 .8 128 .05 1 --color-interaction-comparison
```

Use knee/lowlight identically. `--color-material-diagnostics` only exports sampled compact coefficients/residuals from the same saved fields; it does not rerender the composites. `tests/phase4_color_diagnostics.py` produces contact sheets/measurement-only metrics from completed outputs.
