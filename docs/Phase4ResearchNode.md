# Phase 4 research node and repaired Spill reach

## Milestone status

The CPU research implementation is built, ad-hoc signed, and installed in the existing `/Library/OFX/Plugins/Pigment.ofx.bundle`. Persistent identity remains `org.painterlyofx.Pigment`; Representative remains the new-node comparison default. A1/A2/A3/B and C0–C4 research baselines are preserved. Gate C is still **not accepted**.

**Host verification is blocked, not passed.** On 2026-10-01, both render-license (`-t`) and interactive-license (`-ti`) Nuke 17.0v1 launches exited with status 100 before executing `tests/nuke_phase4_research.py`: no Nuke license was available, with a license-server communication error and no applicable login token. Bundle compilation/signature and CPU tests do not substitute for this host test. Metal acceleration has not begun: it is conditional on verifying the CPU research node in Nuke.

The native GUI independently confirms **“No License found for Nuke 17.0”** in its Licensing dialog. No trial, non-commercial substitution, login, license installation or account change was attempted.

## Reach semantics

Old transport used `-Reach*log(seed)` as initial distance and rejected traversal beyond Reach. This silently prevented weak supports from moving.

The repaired CPU reference is:

```
T(p) = max_q seed(q) * exp(-D_F(q,p) / Reach)
```

For each directed positive-capacity edge:

```
cost(p,q) = physicalDistance(p,q)
            * (1 - log(F_pq) + 6*StructureRespect*boundary_pq)
```

`D_F` is the minimum cumulative cost along directed graph paths. Seed amplitude does not enter that distance. Reach is an **e-fold attenuation distance**, not a hard maximum radius: larger Reach weakens distance attenuation, including for weak seeds. A deterministic max-priority queue computes the maximum attenuated contribution. No source convolution, mask dilation, or local color-mixing strength increase was introduced.

Zero F blocks an edge. Full Structure Respect with a fully locked boundary blocks that edge. Partial barriers raise travel cost. Signed W remains exclusively an affine reconstruction operator and never enters transport capacities. Y and AB solve their own seed fields.

Reach zero returns intrinsic support exactly. At full resolution, only nonnegative **transported gain** is lifted from the smaller graph:

```
fullTransport = min(1, intrinsicFullSupport
                      + max(0, graphTransport - graphSeed))
```

This keeps the intrinsic full-resolution support intact instead of replacing it by nearest graph cells. At zero reach, transported gain is exactly zero. Alpha, hierarchy, source coordinates and appearance mixing law remain unchanged.

## Controlled evidence

Saved repaired comparisons: `tests/visual/renders/phase4/repaired-reach/{fashion,knee,lowlight}`. Each run loads the exact shared snapshot from the accepted comparative pipeline; it does not rerun A1–B. C0 is tested first, followed by matched C1 and preserved C4. All settings except Reach are fixed: Spill .8, Y 0, AB 1, asymmetry .5, respect .8. Reach sweep: 0, 12, 48, 128.

Each fixture exports pre-existing ownership/support provenance, float composites, all Y/AB transport maps, a reach contact sheet, signed reach differences, CSV statistics and measured AB RMS changes relative to zero reach. C1/C4 use their preserved float fields, not differently extracted plates.

For C0, total positive full-resolution AB transport gain grows with Reach:

| Fixture | Reach 0 | Reach 12 | Reach 48 | Reach 128 |
|---|---:|---:|---:|---:|
| Fashion | 0 | 1812 | 6218 | 72214 |
| Knee | 0 | 434 | 7981 | 57205 |
| Lowlight | 0 | 349 | 2109 | 22343 |

These are sums of amplitude gain, not physical travel distances. The graph-chain tests separately verify distance attenuation and weak-seed amplitude independence. The photographic CSV also counts weak supported samples (`0 < intrinsic AB support < .2`) receiving gain above .001. Reach now affects the spatial transport rather than merely changing a seed cutoff.

Those weak supported plate/pixel counts rise from zero at Reach 0 to 521536 / 820223 (fashion), 452227 / 885212 (knee), and 219958 / 561625 (lowlight) at Reach 48 / 128 respectively. C0's AB RMS change relative to zero reach is 8.49e-6 / 2.78e-4, 5.50e-5 / 2.68e-4, and 2.39e-5 / 5.11e-4 respectively. Reach 48 remains visually modest on fashion, despite measurable transport; larger reach gives a clearly larger spatial effect without changing local Spill settings. These measurements do not establish desired pictorial contamination by themselves.

All 36 photographic mode/reach cases retain composite Y bit-exact in AB-only Spill. Numerical reach correction is **not** Gate-C/Gate-D artistic certification: the existing C1/C4 haze/patch problems remain. The repaired transport is now suitable for hands-on spatial research without claiming it repairs those fields.

## Hands-on controls

Select Comparison Mode **Automatic Plate Graph (Phase 4)**. The temporary Research Representation selector offers:

- C1 Bounded Poisson (unchanged descriptor default for existing Phase-4 behavior).
- C0 A3 Passthrough.
- C3 Regional Eigen, fixed representative Y2/AB1 budget; no new mode-count sweep.
- C4 Sparse Curve / Field, unchanged value-only prototype.

C2 remains a preserved standalone comparison, not an interactive acceleration target.

Shared controls include Y/AB Chunk Scale, Boundary Lock, Spill Amount, Y/AB Spill, Reach, Structure Respect, Asymmetry, Amount and Mix. New **Y Support Strength / AB Support Strength** are explicitly participation-amplitude multipliers (0–2, default 1), not spatial radii. Intrinsic graph extent is still controlled by Plate Scale/Overlap and Chroma Support Ratio. Neither support strength nor Spill changes reconstruction alpha. Support edits may affect support-conditioned chunk organization; creative Tone/bias/Weight and Spill edits do not rebuild topology.

Spill Structure Respect no longer implicitly rebuilds intrinsic support. Intrinsic support keeps the frozen .8 structural context; the interactive respect control belongs to Spill transport. At default .8 this reproduces the approved upstream context.

Source, public reconstruction, alpha, Y/AB support, Y/AB chunks, retained contours, selected Pre-Spill, Post-Spill, signed Spill difference, Y/AB influence and transport are available in Debug View. Existing debug indices are unchanged; missing views are appended. Select a plate with Debug Plate. Poisson-only fit/gradient debug fields display zero when unavailable in C0/C3/C4 rather than indexing missing arrays.

## Isolation and cache

- Source debug is an exact source copy.
- Amount zero / Mix zero retain existing exact identity behavior.
- Spill Amount zero bypasses interaction.
- Reach zero is exact intrinsic support transport, but existing local overlap mixing may still run if Spill Amount is nonzero. Use Spill Amount zero to disable all Spill.
- Y or AB Chunk Scale zero copies automatic appearance exactly for that family in every representation.
- Premultiplication, alpha, HDR/negative handling, Mask and final Mix keep the existing CPU path.

Each node owns one serialized source/upstream cache, one support/hierarchy state and one selected representation. Source/geometry/extraction edits invalidate upstream; chunk/support edits invalidate appropriate downstream states; representation edits replace only the synthesis cache. Creative controls, Spill and debug edits reuse all upstream states. Explicit host purge or node destruction releases the cache. Viewer sequence boundaries retain the bounded CPU mathematical cache (no host images are retained); legacy Metal transient cleanup is unchanged. A cancelled or failed build is not installed as a valid cache entry. Cold analysis and changing reconstruction modes remain research-slow; warm Spill/creative edits avoid the spectral solve. No production performance claim is made.

## Validation and use

All 11 regression suites pass. Added tests cover weak .02 seeds, identical relative attenuation for strong/weak seeds, reach growth, zero F, locked boundaries, signed-W independence, exact full-resolution zero-reach support, AB-only Y, alpha preservation, cache invalidation/reuse, all four research representations and safe debug views.

Bundle signature verifies; the installed native module is arm64 and matches the current signed build. Nuke validation still requires an available license.

`tests/visual/PigmentPhase4Research.nk` is a **prepared, not host-verified** four-branch scene using the previously verified OFX class name. It starts at 128 pixels to limit cold CPU cost; replace the Read image or raise the Reformat size for arbitrary plates. `tests/nuke_phase4_research.py` generates a host-verified scene, four renders and fourteen debug renders once licensing is available:

```sh
/Applications/Nuke17.0v1/Nuke17.0v1.app/Contents/MacOS/Nuke17.0 -t tests/nuke_phase4_research.py
```

Successful output is saved under `build/phase4-nuke-research`. Do not treat the prepared scene or successful compilation as equivalent to that run.

## Next step

Restore Nuke license availability and run the focused host validation. Checkpoint the verified CPU node before accelerating the active interactive representations and repaired Spill. Preserve CPU behavior and parity; do not port C2/moment/barrier experiments for completeness or start a new Gate-C family in place of the requested hands-on milestone.
