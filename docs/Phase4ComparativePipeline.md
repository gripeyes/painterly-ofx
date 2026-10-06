# Phase 4 — instrumented comparative CPU pipeline

## Status and scope

The first comparative run is complete on **fashion, knee, and lowlight**. Implementation progress is now separate from photographic acceptance. Gate C remains **not accepted**; Spill is a **diagnostic module**, not an accepted Gate D result. No production defaults, OFX identity, comparison indices, or frozen A1/A2/A3/B algorithms changed. No OFX reinstall, Nuke launch, or Metal port was performed.

The sparse value-curve prototype from `82775d2` is preserved unchanged. Its existing photographic failure is a comparison baseline, not a reason to discard the implementation.

## Named comparisons

| Mode | Representation | Input provenance |
|---|---|---|
| C0-A3 | Corrected public appearance passthrough | Current frozen full-precision public representation |
| C1-Poisson | Bounded source-gradient Poisson, no moment penalties | Re-rendered once against current cached upstream, gradient complexity 0.1 |
| C2-SecondMoments | Preserved mean/first/second-moment experiment | `build/phase4-c-second/{fixture}` |
| C3-RegionalEigen | Preserved isolated regional eigenfield, Y2/AB1 | `build/phase4-c-regional-eigen/{fixture}/modes-2-1` |
| C4-SparseCurve | Preserved isolated sparse value-curve harmonic field | `build/phase4-c-sparse-transition/{fixture}` |

All failed paths remain available. This run does not change their formal status.

## Shared-input discipline

Older outputs did not retain full-precision alpha/support or the directed graph. Therefore the unchanged frozen upstream was materialized **once per fixture**, then saved in `shared-upstream.snapshot`. Subsequent comparative runs reused it. This was data recovery for exact downstream comparisons, not spectral optimization.

The research-only native snapshot stores public float alpha, supportY/supportAB, automatic YAB appearance, signed W and nonnegative F graph data, shared atomic labels, per-plate chunk labels, disappearance levels, and retained barriers. Its key covers source YAB and upstream parameters. It is not a portable production format. Existing A1/A2/A3 diagnostic images are reused when the snapshot is loaded, keeping the spectral vocabulary permanently visible.

The old Poisson run failed the provenance check because its Y chunk labels differed. It was not silently mixed into the comparison: the old result remains intact, and C1 was solved once using current upstream data. C2/C3/C4 passed byte comparisons of source YAB, float automatic appearances, exported alpha/support diagnostics, chunk labels, and retained masks. Historical alpha/support exports are quantized; they are provenance checks, not the values used for current reconstruction or Spill. Every mode uses the same current **full-precision** alpha/support/F.

Manifests retain SHA-256 hashes of the snapshot, float source, and saved mode composites. No prior validated research output was deleted or overwritten.

## Spill comparison

Common research settings: Amount 0.8, Luma 0.05, Chroma 1.0, Reach 48, Asymmetry 0.5, Structure Respect 0.8; plate weights/enables/out/receive unchanged and artistic offsets zero. These are diagnostic stress settings, not new defaults.

Each C mode exports:

- Pre-Spill and mixed Y/AB Spill.
- AB-only Spill, with composite Y required to remain bit-exact.
- AB-only zero-reach comparison.
- Separate Y/AB influence and transport maps, plus pre/post per-plate appearance.

Transport is prepared once and reused across modes. Diagnostic generation verifies that interaction/transport fields are byte-identical across C0–C4. Signed W is not used as a probability or transport capacity. Zero nonnegative F and fully locked structural capacities block transport. Reconstruction alpha, support, chunk topology, and coordinates are not changed by Spill.

## Photographic findings

The first visible deterioration in this comparison occurs at **Gate-C reconstruction**, not at C0/public reconstruction. Fashion shows haze and disconnected cloth organization; the moment and eigenfield variants do not materially remove the tradeoff. Sparse curves remove description but introduce additional block-like/background organization. Knee and lowlight comparisons likewise do not establish an accepted broad-field model. Spill changes chroma without resolving these defects. C0 plus Spill preserves the photograph best but provides little pictorial reorganization.

No combination is accepted on photographic evidence. Lower descriptive energy is not sufficient:

| Fixture | Medium Y energy relative to source, C1 / C2 / C3 / C4 |
|---|---|
| Fashion | 0.608 / 0.617 / 0.638 / 0.568 |
| Knee | 0.511 / 0.561 / 0.508 / 0.514 |
| Lowlight | 0.804 / 0.805 / 0.839 / 0.759 |

These are measurement-only band diagnostics; no blurred measurement target enters rendering.

### Y/AB separation: verified, with limitations

AB support mass divided by Y support mass ranges across six plates:

| Fixture | AB/Y support mass | AB/Y occupied area at common threshold |
|---|---|---|
| Fashion | 1.013–1.125 | 1.023–1.187 |
| Knee | 1.002–1.005 | 1.005–1.012 |
| Lowlight | 1.001–1.408 | 1.000–1.490 |

AB support is broader, but **not substantially broader on knee**. Separate hierarchies also differ: plate A has Y/AB chunk counts 4954/4890 (fashion), 759/700 (knee), and 182/57 (lowlight). Full per-plate counts and retained-mask disagreement are in `separation.csv`; counts alone do not imply meaningful artistic organization.

AB-only Spill has **zero composite Y change in all 15 fixture/mode cases**. Thus this control cannot move or soften Y geometry through its output values. Mixed Spill produces larger AB than Y changes, but broad geodesic travel is not yet demonstrated.

Reach 48 versus reach 0 changes AB by only approximately 5.6e-7 (fashion), 4.8e-6–5.7e-6 (knee), and 1.7e-6–2.0e-6 (lowlight) RMS. Most visible Spill change comes from existing support overlap and same-pixel appearance mixing, not substantial additional transport.

The current transport seeds distance as `-reach * log(seed)` and stops expansion after total distance exceeds reach. Seeds at or below exp(-1) have no remaining traversal budget. Broader but weaker AB support can therefore contribute little additional graph reach. This is an identified **Spill transport limitation**, not evidence that the underlying research family or frozen latent basis fails. It has not been hidden by stronger color mixing or downstream finishing.

## Artifacts and reproduction

Saved evidence: `tests/visual/renders/phase4/comparative-pipeline/{fashion,knee,lowlight}`.

Important sheets:

- `Stage-by-stage-upstream.png`: source, A1, A2, A3, public reconstruction/alpha, Y/AB support, Y/AB chunks and retained contours.
- `Gate-C-comparison.png`: source and C0–C4.
- `Gate-C-Spill-matrix.png`: all five pre/post representations and signed differences.
- Per-mode appearance, influence, transport and Spill comparison sheets.
- `comparative-metrics.json`, provenance/settings/separation CSVs, manifests, float composites and exact shared snapshot.

Reproduce a cached comparison from the repository root:

```sh
build/pigment_phase4_gate build/phase4-interior-fixtures/fashion.ppm build/phase4-comparative/fashion 16 6 24 64 .1 .8 48 .05 1 --comparative-pipeline
```

Substitute `knee` or `lowlight`. `tests/phase4_comparative_diagnostics.py` produces presentation/measurement sheets using the bundled Python with NumPy/Pillow. The first comparative implementation deliberately limits fixtures to these three.

The new diagnostic regression test covers cached transport parity, AB-only Y identity, unchanged alpha, independence from signed W, zero F blocking, structural blocking, and snapshot round-trip semantics. All **11 test suites pass**.

## Research decision

Keep A1/A2/A3/B frozen and all C baselines visible. The comparative workflow is implemented; photographic Gate C and spatial Spill acceptance are not. The next relevant comparisons are public broad-field behavior and the diagnosed transport seed/reach limitation, not another spectral or moment optimization. Any future correction must remain a named comparison and demonstrate photographic improvement under the same upstream inputs. No Metal or finishing is justified by these results.
