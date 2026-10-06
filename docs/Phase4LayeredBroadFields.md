# C5: Layered Broad Fields — pre-implementation design

## Research-node exposure (2026-10-07)

The existing prototype is now available in **Interface → Research → Research
Representation → C5 Layered Broad Fields (Experimental)**. This exposes the
preserved formulation; it does not accept Gate C, alter A1–B, or change Pigment's
default C1 mapping. Existing representation indices 0–4 are unchanged; C5 is
appended at index 5.

Full Reference runs the CPU prototype. Interactive / Guided uses the existing
reduced-analysis, full-resolution effect-difference preview; C5 is not Metal-ported.
Prototype observation scales remain Y 32 / AB 64 canonical pixels (scaled for the
explicit proxy/Guided analysis grid). Complexity controls remain disabled for C5:
they belong to C1/C2 and do not secretly change the prototype's fixed layer budget.
Zero Y/AB Chunk Scale bypasses that family exactly; nonzero scale provides the
existing retained-structure context, not mandatory chunk reconstruction domains.

For isolation use Linear YAB, Spill 0, and the usual fixed public alpha. Existing
Pre-Spill and Gradient Reconstruction views inspect the composite and per-plate
final appearance. New C5 debug views expose broad targets, retained structure,
medium description, micro residual, combined broad Y/AB, and individual Y/AB
sublayer memberships/fields. Choose **Debug Plate** (0=A) and **Debug Latent**
(1-based sublayer within the selected Y/AB family). Missing sublayers display zero.
Signed residual maps are presentation-only neutral-gray encodings.

The earlier photographic failure evidence below remains authoritative. Hands-on
exposure is for comparison, not promotion or a new artistic acceptance claim.

Baseline: `564d0b8`. C5 is an isolated CPU experiment, not an accepted
representation or a change to installed Pigment/Guided defaults.

## Research reviewed before implementation

- [Richardt et al.](https://richardt.name/layered-vectorisation/LayeredImageVectorisation-paper.pdf): sections 3–4, recursive alpha-over, parametric linear/radial foregrounds, and underdetermined layer recovery. Its manual selections, background TV/smoothing and Poisson cleanup are **not** adopted.
- [Du et al.](https://zhengjun-du.github.io/papers/202305/Image_vectorization.pdf): sections 4.1–4.7, overlapping layers, supporting relationships, reconstruction/compactness objective. Its segmented input, tree enumeration, RGB-cube penalty and vector export are **not** adopted.
- [He/Liu](https://arxiv.org/html/2407.02794v2): section 2, structural/smooth/oscillatory components with distinct functionals. We use this distinction, not their elastica/FFT solver or a claim to reproduce CST.
- [Bredies/Kunisch/Pock](https://imsc.uni-graz.at/mobis/publications/SFB-Report-2009-038.pdf): ramp-preserving second-order regularization and affine nullspace. No TGV output or solver is used.
- [Fattal et al.](https://web.tecgraf.puc-rio.br/~scuri/inf1378/pub/lischinski.pdf): sections 3–5, nonintegrability after gradient changes and Poisson reconstruction. C5 does not use its HDR attenuation, pyramid or tone mapping.
- [Orzan et al.](https://www.cs.jhu.edu/~misha/ReadingSeminar/Papers/Orzan08.pdf): sections 3.2–4, side colors, gradient constraints, diffusion and reblurring. No diffusion-curve renderer or reblur is used here.
- [Jeschke](https://pub.ista.ac.at/group_wojtan/projects/2016_Jeschke_GDCI/paper_preprint.pdf): section 2, spatial blending of two Laplace fields and independent interior control. C5 uses explicit overlapping fields instead; it is not GDCI.
- [Tschumperlé/Deriche](https://tschumperle.users.greyc.fr/publications/tschumperle_pami05.pdf): sections 1–4, distinction between geometry and regularization, trace versus divergence tensors. No PDE appearance smoothing is used.
- [Weickert](https://www.mia.uni-saarland.de/weickert/Abstracts/ced-col.html), with the accessible [author's precursor](https://www.mia.uni-saarland.de/weickert/Papers/nspria97.ps.gz): common vector structure tensor and orientation coherence rather than gradient magnitude alone. The 1999 PDF was inaccessible; the six-page author precursor was read instead. No coherence-enhancing diffusion is executed.
- [Chakraborty et al.](https://techmatt.github.io/pdfs/imageVectorizationViaGradientReconstruction.pdf): sections 3–5, simple fill vocabulary and its limitations on complex photographs. No preprocessing filter, segmentation or one-fill-per-region assumption is adopted.

## Deliberately limited automatic adaptation

For each frozen public plate, independently for Y and joint AB, collect
support-weighted robust median observations on canonical macro cells (Y 32px,
AB 64px initially). Observation coordinates are support-weighted centroids.
Cells are fitting measurements only, never final output domains. Their median
values are a sparse broad-target diagnostic, not a filtered source image.

Fit a base affine field, then at most three Y / two AB semi-transparent layers.
Each added layer has a compact broad cosine opacity envelope and either an
affine or radial color field. Candidate centers come from the largest remaining
macro residual; envelope radii are tested at 36%, 55%, and 80% of the image
extent for Y, with a 56% lower bound for AB. Several fixed broad widths and
the two field types are tested. This deliberately fixed geometry is not a
faithful automatic layer-mask discovery algorithm.
No pixelwise free opacity variables, hard labels, dense basis or curve density
are introduced. Opacities overlap and do not sum to one.

With fixed opacity geometry, color coefficients minimize the support-weighted
macro-observation error of the complete recursive alpha-over stack, with a small
ridge on slopes. Two deterministic Cauchy reweighting passes reject local
descriptive outliers. A new layer is retained only if it reduces the observation
objective by at least 3%; requested maxima need not be reached. All trials,
accepted fields, coefficients, occupancy and objectives are recorded.

`L0 = base`; `Lj = aj Fj + (1-aj) L(j-1)`. Source YAB remains unclipped.
Macro fitting is not a claim of faithful automatic Richardt/Du decomposition.
It is the smallest bounded parametric overlapping-layer hypothesis to test.

## Observable information classes

Let `L` be the layered reconstruction of broad observations. Protection `P` uses
the frozen source contour cue together with sign/direction persistence across
1/4/8px plate-appearance differences; AB uses weaker protection. At a strongly
qualified retained contour pixel, protection is one. It never relocates a contour.

Micro evidence is the difference from a 3x3 median **analysis statistic**, admitted
only where opposite-neighbor second differences indicate oscillation and no
structural protection is present. It is not a hero target or output filter.

`B=L`, `K=P*(I-L)`, `F=(1-P)*micro`, `M=I-B-K-F`.
Thus `B+K+M+F=I` within arithmetic precision. Final isolated appearance is
`B+K+0.08F`; medium survival is zero. This explicit residual partition is a
heuristic, not a solved CST decomposition. Unqualified/zero-support areas retain
automatic appearance exactly. Y-only and AB-only bypass the other channels
bit-exactly. Public alpha, supports, hierarchy and geometry remain unchanged.

## Decision policy

Run identical frozen snapshots for fashion/knee/lowlight, Linear YAB, Spill zero.
Save C0, Y-only, AB-only, both, all fields/occupancies and residual diagnostics.
Inspect photos before integration/Guided/Spill/Density. Failed isolation is saved
as negative evidence, not rescued by more layers, filters or residual survival.
The installed node remains untouched during this experiment. Ballerina is used
only if an existing local scene/source is reproducible; no new source is acquired.

## First isolated result — failed, stop condition reached

Fashion, knee and lowlight were evaluated from the existing exact shared
snapshots. **C5 is not accepted.** No Spill, Density, Guided or Nuke integration
was attempted. No C0–C4 renderer, upstream source file, parameter layout,
production cache boundary or installed bundle changed.

The individual sublayer sheets prove that multiple overlapping cosine occupancy
fields and independently fitted linear/radial Y/AB appearances are actually
present. However, the complete photograph does not satisfy the hypothesis:

- Fashion: Y retains too many cloth/skin facts and introduces an uneven,
  embossed surface rather than coherent broad cloth masses and clean volume.
- Knee: some highlight-to-shadow form survives, but block/patch variations
  become more noticeable and the curved highlight becomes less convincing.
- Lowlight: chroma is reduced/reorganized, but coherent useful contamination
  is not established; Y description remains largely unchanged.

The measured Y medium-energy ratios against C0 are 0.919 (fashion), 0.942
(knee), 0.854 (lowlight). Y fine-energy ratios are 0.953, 1.215, 1.013.
AB medium-energy ratios are approximately 0.15–0.34: that suppression does
not by itself constitute useful pictorial organization. The diagnostic
decomposition recombines to A3 within 1.8e-7; Y-only AB and AB-only Y bypasses
are bit-exact at the composite. All 14 regression/unit suites passed.

### Failure source and limits of the conclusion

The C5 residual partition is the first demonstrated bottleneck: it calls too
much source description "structure", and the abrupt negligible-support fallback
can expose irregular boundaries between original and fitted appearance. The
macro fitting stage is also very restrictive: parametric envelopes are selected
greedily, not automatically recovered layered masks as in a faithful layered
decomposition. Local statistics are coarse, visible **only in target diagnostics**;
they are not rasterized into the output. Output block-like changes therefore
cannot be explained as direct macro-cell interpolation.

This experiment does **not** establish failure of overlapping layers,
Richardt/Du, CST or the frozen upstream basis. It establishes failure of this
specific limited automatic fitting + structure/residual partition. No upstream
stage is reopened. More layers, larger micro survival, Spill, Density and new
filters are not used to rescue it.

The smallest next research question, if authorized, is C5-specific: can a
scale/coherence-qualified structural component separate large geometry from
medium descriptive residual without creating a hard support splice? Freeze
the current layer geometry/fields while measuring that distinction first.
No automatic follow-up is implemented after this failed isolation.

### Reproduction and evidence

```
build/pigment_phase4_gate build/phase4-interior-fixtures/fashion.ppm build/phase4-comparative/fashion 16 6 24 64 .1 0 48 .05 1 --layered-broad-comparison
```

Substitute knee/lowlight. A missing/mismatched frozen snapshot is an error;
the harness never creates one for C5. Full-precision per-plate fields and all
sublayers are preserved in `build/phase4-comparative/{fixture}/C5-LayeredBroadFields`.
Presentation sheets, input hashes, coefficients and objective sequences are in
`tests/visual/renders/phase4/C5-layered-broad/{fixture}`. They include C0,
Y-only, AB-only and Y+AB, six per-plate information sheets and six sublayer
sheets per fixture. Single-channel gray maps are presentation-normalized only.

The existing saved validation scenes in the repository/build reference fashion,
not ballerina. No reproducible ballerina source/scene was identified there;
no external dependency was added. Full-HD host usability is not rebenchmarked
for a failed CPU isolation. `564d0b8` remains the hands-on baseline.
