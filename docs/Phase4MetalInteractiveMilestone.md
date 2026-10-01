# Phase 4 hands-on Nuke / hybrid Metal checkpoint

## Status

Nuke 17.0v1 **arm64 host validation now runs successfully**; licensing is no longer the blocker recorded in the earlier report. The signed installed effect remains `org.painterlyofx.Pigment`. Representative and existing comparison indices/defaults are unchanged. Pigment and Research / Compare remain opt-in interfaces.

This is a working **hybrid research build**, not a fully GPU-resident or full-frame real-time production certification. Gate C remains **unaccepted**. C0/C1/C2/C3/C4 and all three CPU color laws are retained; Spectral Pigment is **not ported to Metal**. A1/A2/A3/B equations, plate identity, supports, contours and hierarchy are unchanged. Cache separation and independent-job scheduling do not redesign those representations.

Open `tests/visual/PigmentPhase4Interactive.nk` in Nuke. It contains the actual-host-generated Pigment and C0–C4 branches, starts at 128 pixels, and selects **Linear YAB / Auto (Hybrid)** for the artist branch. Replace the Read, then confirm Working Gamut matches the project's actual scene-linear primaries. Raise the preview resolution after the first render. No display transform, DRT, grain, or film response is introduced by the node.

## UI and backend behavior

Pigment retains Pictorial Scale, Structure Lock, independent Organization/Complexity, Chroma Spread, Spill, Reach, Asymmetry, Amount/Mix. Their macro equations and ranges remain those in `PigmentArtistMode.md`; they were reviewed rather than arbitrarily remapped. Scale coordinates graph support and hierarchy cuts, Lock coordinates boundary discrimination and transport capacity, and Spread remains independent of chunk cuts. Zero Organization bypasses that family's field/Spill coefficient. Copy to Research is a current-frame evaluated snapshot, not an animation bake.

The compact advanced Color Interaction section exposes Linear YAB, Density, Spectral Pigment, Pigment Density and Compute Backend:

- **Auto (Hybrid)**: CPU frozen source analysis, supports, B and C; faster cached CPU graph transport; Metal Linear/Density appearance interaction and alpha reconstruction. Automatic CPU fallback on Metal failure.
- **CPU Reference**: complete CPU renderer, including all historical fields and Spectral.
- **Metal Spill (Hybrid)**: also runs the converged GPU graph-transport comparison. Explicit Metal failure is reported, not disguised as CPU. Spectral always uses CPU, as stated in the knob hint.

Native Metal-host image invocations still request CPU-buffer retry. This is CPU-backed staged Metal acceleration, **not** native-host/full-node GPU support. Final gamut conversion, Amount/Mask/Mix, premultiplication and source-alpha copying stay in the common CPU finalizer. Output diagnostics use the same CPU data structures whichever backend supplies the appearance fields.

## Implementation and parity

`pigment_phase4_spill` implements the unchanged directed donor/receiver weights, Linear or Density mixture/decode and artist-adjusted alpha reconstruction. It consumes frozen alpha, independent supportY/supportAB and transported fields. Spectral is rejected before encoding. Compact material encoding for Density remains the double-precision CPU reference: an initial float GPU encoder exposed signed near-zero RGB cancellation at a channel floor. Encoding is an integration boundary, not a change to the absorption law. The packed material state preserves the original magnitude/residual strategy.

`pigment_phase4_transport` solves the same max-product graph fixed point by deterministic incoming-edge relaxation, with exact CPU attenuation factors. Zero Reach returns the seeds, zero F and locked structural capacities block edges, and convergence uses an exact change flag rather than an epsilon that discards weak influence. Tests compare its final fields **bit-exactly** with CPU Dijkstra. It is slower on the tested larger photographic graphs, so Auto deliberately retains the CPU method. No transport topology is changed to improve speed.

Per-node reusable shared Metal buffers are released on purge/lifecycle cleanup. Command completion precedes CPU readback. A failed GPU result never becomes a valid reference cache. Separate source-analysis and transport caches avoid rerunning frozen A1/A2/A3 for grouping/support edits and avoid repeating transport for color, density, tone, weight or complexity edits. Exact cached/fresh public outputs are tested. Independent plate jobs use bounded four-worker scheduling with no parallel reductions; each job retains reference arithmetic order. Chunk Scale zero now skips the unnecessary solver work and retains its exact appearance bypass.

`tests/Phase4MetalTests.cpp` tested 72 photographic combinations on **fashion, knee, lowlight**, loading the saved frozen upstream snapshot and C0/C1/C4 fields rather than regenerating A1–B. Coverage includes Linear/Density, Spill 0/1, Reach 0/128. Additional four-gamut synthetic tests cover negative/HDR RGB, unusual origins, signed W vs F, structural/zero-F blocking and AB-only bit-exact Y. Maximum photographic YAB error was **2.6077e-7**; maximum RMSE **2.95692e-8**. Influence errors were bounded separately; transported fields were exact. Alpha never enters a GPU output buffer, and ownership/supports/topology are read-only.

Saved photographic sheets and numeric tables: `tests/visual/renders/phase4/metal-interactive`. Differences are enlarged **10,000×** for presentation. No new aesthetic acceptance is inferred from parity: C1 haze and C4 patches remain visible and unaccepted.

## Actual Nuke validation

`nuke_phase4_research.py` passed creation, all five representation choices, fourteen stage/debug outputs, Color Interaction switching, Pigment Density, macro mappings and sampled **bit-exact** Copy to Research equality. The saved scene is an actual host artifact, not merely a hand-written prospective scene.

`nuke_phase4_interactive_validation.py` passed CPU/Metal comparisons, Spectral CPU fallback, Amount/Mix zero identity, source debug identity, alpha preservation including zero alpha, zero Mask identity, frame changes, odd 31×47 / 137×89 formats, offset source bounds and node serialization/recreation. HDR/negative synthetic values were written as 32-bit float EXR. Host comparisons use a sampled grid with exact alpha/endpoint equality and a 5e-5 RGB error bound; they are **not exhaustive per-pixel certification**. Core and photographic harness comparisons are exhaustive over their fixtures. Zero/narrow-mask handling additionally bounds-checks Mask RoD and copies source exactly at zero participation.

Host artifacts remain under `build/phase4-host-metal` and `build/phase4-nuke-research`. Evidence JSON is archived beside the photographic sheets. All 13 regression suites pass, including a real Metal test; legacy tests continue unchanged.

## Performance and remaining work

Stage measurements are emitted with `PIGMENT_PHASE4_PROFILE=1`. `nuke_phase4_performance.py` forces actual creative edits so warm timing is not merely a Write of Nuke's image cache. Separate reported stages are source/plate analysis, B, selected C, graph transport, fused appearance interaction/alpha reconstruction, and final RGB/Amount/Mask/Mix. The GPU kernel fuses Spill weights, color law and alpha reconstruction: an isolated color-law duration **cannot be honestly extracted** from its one dispatch. GPU, packing and readback timings are separately recorded by the standalone harness.

Detailed actual-host timings are archived in `host-256` / `host-512` under the evidence directory. First source/frame/resolution changes still invoke the expensive exact CPU spectral analysis. Organization, scale, support or boundary edits rebuild affected downstream states; creative/color/Spill-strength edits reuse them. This distinction is essential when judging interactivity.

The initial 512×512 fashion host run measured about **47 s cold**, **46 ms warm Linear Auto** and **98 ms warm Density Auto**, versus about **301 / 441 ms** CPU. A scale edit was about **3.6 s**, with source-analysis build count staying at one. Whole Nuke process maximum resident size was about **3.05 GB**, including Nuke and its other loaded plugins; this is not isolated Pigment scratch. Later independent-job scheduling measurements are recorded separately in the final timing artifacts, not folded into an unsupported real-time claim.

Final independent-job scheduling run (`host-512-parallel`) measured **29.4 ms Linear Auto / 47.7 ms Density Auto**, versus **298 / 439 ms CPU**. The scale edit fell to **2.40 s**: plate regroup/support 699 ms, B 523 ms, C4 566 ms, transport 561 ms, fused interaction 52 ms and final 1 ms. Source analysis stayed at one build. Cold source processing remained **47.2 s**. At the 256-pixel fixture previews, warm medians were **8.2 / 16.6 ms** Auto versus **61.9 / 106.2 ms** CPU. These medians pool warm creative edits over C0/C1/C4; they do not include rebuilding a changed hierarchy or field.

This checkpoint delivers the hands-on research node and verified active appearance acceleration. The broader performance milestone is **not fully complete**: A1/A2/A3/B and C1/C4 field construction remain CPU-bound, full-HD interactive rebuild rates have not been certified, and the GPU graph comparison is not a speed win. Future optimization should target measured support/hierarchy/field rebuild costs while preserving the exact CPU formulation—not start a new Gate-C family or port Spectral/moment/barrier research to hide those limitations.
