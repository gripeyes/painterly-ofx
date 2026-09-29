# DetailCollapse Rolling YAB Mass visual reference

`DetailCollapseValidation.nk` is a ready-to-inspect Nuke 17 scene using the installed
Pigment OFX node. The four synthetic plates cover CGI material/specular breakup,
low-light chroma, skin and fabric, and broad color fields. They were generated as
neutral evaluation material, not as target looks.

The saved reference setting intentionally masses chroma more strongly than luminance:

- Amount 0.65, Mass Scale 2.5, Structure Scale 5
- Mass Strength 0.55, Tone Similarity 0.12, Chroma Similarity 0.18
- Boundary Preserve 0.95, Boundary Softness 0.05, Structure Preserve 1
- Internal Variation 0.4, Luma Massing 0.3, Chroma Massing 1

At 256 x 256 on the validation machine, 15 Nuke renders averaged about 0.22 seconds
each. This direct four-pass bilateral implementation is a visual CPU reference and
uses the full source RoD; it is not suitable as the eventual high-resolution backend.

The CGI debug renders show that AB can be consolidated more strongly than Y and that
the boundary field is independent of processing strength. The reference suppresses
small material events while retaining the dominant object silhouette, but stronger
luma settings can still look like conventional edge-aware smoothing. The Y path and
internal-variation reintegration are therefore the main targets for the next visual
iteration.

The input plates were created with the built-in image generator from prompts for:

1. A neutral studio CGI shader-ball scene with dense micro-specular breakup.
2. A low-light photographic scene with blue, green, violet, and umber-black regions.
3. A natural photographic portrait with visible skin and woven fabric detail.
4. Broad overlapping painted color fields with restrained fine texture.

## Integrated Pigment Phase 3

`PigmentValidation.nk` is the Nuke scene for the persistent
`org.painterlyofx.Pigment` node. `renders/pigment` contains matched Original, Guided,
Integrated, field, pre-boundary, pre-reintegration, and signed-difference views.
Scalar views are displayed as normalized grayscale. Residual and difference views
use neutral gray as zero, with a display gain of 0.45; that display transform is not
part of processing.

`fruit-grapes.png` and `laundry-cloth.png` were generated as fixed research fixtures
with the built-in image generator. Their generation prompts and the present visual
assessment are recorded in `docs/Phase3IntegratedPigment.md`.

Phase 3.1 preserves the `*-weighted-mean.png` files as the failed smoothing baseline
and adds `*-representative-mode.png`. Density, dominant-mode, confidence,
representative-distance, and candidate-competition views expose why a population was
selected. The validation scene defaults to Representative Mode and uses deliberately
aggressive continuous settings for the grape/laundry acceptance comparison.

## Pictorial Planes Phase 3.2

`renders/pictorial-planes` contains the Phase 3.2 engineering pass: Original and
legacy comparisons, raw and normalized Plane Map views, independent Y/AB transition
memberships, plane and combined targets, residual bands, extinction/protection,
Pre-Veil, Pre-Softness, Final, fit error, and signed difference. The saved Nuke scene
connects an editable procedural RGBA ownership map to the new PlaneMap input and sets
Veil and Local Softness to zero.

The procedural map validates the processing graph and diagnostics; it is not the
artistic acceptance map. Replace it with four aligned Roto/RotoPaint masks before
judging the cheek, shoulder, and knee isolation gate. See
`docs/Phase3_2PictorialPlanes.md` for implementation and current visual status.

## Soft Pictorial Plates Phase 3.3

`PigmentPhase33Validation.nk` is the focused 1080p CPU-reference scene. Regenerate
the automatic-plate and shading gate renders with `tests/nuke_phase33_gate.py`.
The current result did not pass the artistic isolation gate; retain this scene for
diagnosis rather than treating it as an approved preset. The implementation and
failure analysis are in `docs/Phase3_3SoftPictorialPlates.md`.
