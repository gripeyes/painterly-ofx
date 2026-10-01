# Pigment artist interface

An opt-in image-making interface over the current Phase-4 CPU implementation. Pigment uses C1 bounded Poisson, not a new algorithm. Gate C remains photographically unaccepted. No pigment chemistry or change to frozen A1/A2/A3/B algorithms is introduced.

Choose **Interface → Pigment** for compact controls or **Research / Compare** for representations, stage diagnostics and lower-level controls. Existing Comparisons remains the interface default and Representative remains the comparison default. Saved parameter IDs and indices are preserved.

## Exact macro mapping

Let P be Pictorial Scale in canonical pixels, L Structure Lock, Y/AB the corresponding Organization values, and S Chroma Spread. Unit controls are bounded to 0–1.

| Artist control | Evaluated research parameters |
|---|---|
| Pictorial Scale | Plate Scale = P; Y Chunk Scale = P·Y; AB Chunk Scale = 2P·AB |
| Structure Lock | Boundary Lock = L; Merge Selectivity = .3+.4L; Internal Variation = .6−.2L; Spill Structure Respect = .2+.8L |
| Luma Organization | Y hierarchy cut = P·Y; Y Spill coefficient = .3Y(1−.5L) |
| Chroma Organization | AB hierarchy cut = 2P·AB; AB Spill coefficient = AB(.5+.5S) |
| Luma Complexity | Independent Y interior source-gradient survival |
| Chroma Complexity | Independent AB interior source-gradient survival |
| Chroma Spread | Chroma Support Ratio = 1+3S; AB Support Strength = .75+.75S; AB Spill coefficient above |
| Spill | Overall directed interaction amplitude |
| Spill Reach | Graph-geodesic e-fold attenuation distance, independent of seed strength |
| Directionality / Asymmetry | Symmetric-to-directed plate interaction blend |
| Amount / Mix | Existing masked overall strength / final straight-RGB blend |

Pigment fixes Plate Overlap at .55, Luma/Chroma Coupling at .35 and Y Support Strength at 1. Intrinsic Y radius = .55P; AB radius = .55P[.35+.65(1+3S)]. These are affinity-graph budgets, not Euclidean blur radii. Organization cannot change these support radii. Support strengths are participation amplitudes, not radii.

Organization zero bypasses its family's field synthesis and Spill coefficient. Spill zero bypasses interaction. Reach zero retains intrinsic support; local overlap interaction may still occur with nonzero Spill. Amount/Mix zero are exact identity paths. Higher Complexity retains more internal source-gradient description. Y and AB affect separate fields.

Defaults: P=48, L=.75, Y=.5, AB=2/3, Y Complexity=.5, AB Complexity=.25, Spread=1/3, Spill=.25, Reach=48, Asymmetry=.5. These are research starting settings, not an accepted aesthetic preset. Expert plate count, latent budget and per-plate overrides are retained.

## Inspect and tune

**Copy to Research** copies this frame's evaluated macro values into the lower-level knobs and switches to Research with Final debug selected, independent Y/AB Complexity enabled, and C1 selected. Amount, Mix, gamut, mask and plate overrides remain unchanged. This is a current-frame snapshot, not an animation-range bake; changes are one undo action.

Plain interface switching does **not** overwrite prior Research settings and may change the image. Research preserves C0 passthrough, C1 Poisson, C2 second moments, C3 fixed regional eigen, and C4 sparse curve. C2 is preserved negative evidence, not an acceleration target. Independent Complexity affects C1/C2; C3/C4 retain their formulations. With Independent Complexity off, the historical shared control remains exact.

Research retains source, public reconstruction, alpha, separate supports, chunks, contours, pre/post-Spill, difference, influence and transport views. Pigment shows Final. The common Output group exposes Amount, Mix, Working Gamut and Invert Mask.

An isolated **Color Interaction — Experimental** group is available in both interfaces: Linear YAB (unchanged default), Density, Spectral Pigment and Pigment Density. Macros and Copy to Research retain these selections without changing transport. These are CPU comparisons, not spectral-chemistry certification; see `Phase4ColorInteractionDesign.md` and `Phase4ColorInteractionResults.md`.

## Verification

All 11 regression suites pass after a complete rebuild. Tests cover coordinated scaling, zero organization, support/chunk independence, structure mapping, bounds, retained expert settings, bit-exact shared-Complexity compatibility and independent Y/AB fields. These checks do not certify photographic acceptance. The installed arm64 bundle at `/Library/OFX/Plugins/Pigment.ofx.bundle` has a verified signature and a module hash matching the signed build.

Prepared scene: `tests/visual/PigmentPhase4Research.nk`. The host script `tests/nuke_phase4_research.py` checks macro availability, copied values and sampled float-render equality before/after copying. **Host testing remains blocked by Nuke 17.0 licensing.** Compilation/installation do not certify OFX UI behavior. CPU remains the reference; no Metal acceleration is claimed.
