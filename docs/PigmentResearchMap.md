# Pigment Research Map — supplied Phase 3.2 table

Added 2026-10-07. This document preserves the user's 58-row research table verbatim as a historical research map. Its priorities, manual-plane language and Phase 3.2 roles are not current implementation claims or new instructions to change Pigment.

Read it alongside the [deduplicated paper index](ResearchBibliography.md), which adds primary-source links, bibliographic clarifications and current architectural boundaries. Rows 57 and 58 are cross-references to 50 and 29, not additional papers: there are 56 distinct main entries.

## How to use this map now

- **Representation:** distinguish contribution alpha, Y/AB support, spatial appearance, transition geometry and transport. A useful paper may inform one of these without defining the whole effect.
- **Information analysis:** structure, broad shading, medium description and micro residual are different information classes. Gradient magnitude alone is insufficient.
- **Current evidence:** A1/A2, corrected A3 and production Gate B remain frozen. C0–C5 are comparisons; C5 remains an unaccepted prototype. Inclusion here does not reopen an upstream stage or promote a failed path.
- **Analysis versus appearance:** a smoothing paper may supply measurements, solver techniques or comparison fixtures; it does not authorize Gaussian/WLS/TGV/bilateral/guided hero reconstruction in the active Phase-4 path.
- **Scene-linear contract:** tone mapping, haze, optical softness and harmonization are references or separate operations, not an implicit DRT, finishing transform or rescue for failed spatial organization.
- **Verification:** bibliography links identify research sources; not every full paper was read in this documentation update. Prior detailed reviews and tested approximations are recorded in the stage reports below.

## Current evidence and reading route

Start with the [visual-basis checkpoint](Phase4VisualBasisCheckpoint.md),
[corrected A3 comparison](Phase4A3ReferenceComparison.md) and
[comparative pipeline](Phase4ComparativePipeline.md). For overlapping broad
layers, the [C5 report](Phase4LayeredBroadFields.md) records the actual reviewed
references, prototype and photographic limitations. The
[paper index](ResearchBibliography.md) supplies links for all distinct table
entries and separates source concepts from implemented approximations.

The [Color Interaction design note](Phase4ColorInteractionDesign.md) and
[Spectral diagnosis](Phase4SpectralDiagnosis.md) remain a separate research
track. This map does not authorize chemistry, new Gate-C work or changes to
the installed hands-on baseline.

## Supplied table (unchanged)

| # | Pri. | Area | Paper / resource | What Pigment takes from it | Phase 3.2 role |
|---:|:---:|---|---|---|---|
| 1 | **P1** | Multiscale decomposition | **Farbman et al. - Edge-Preserving Decompositions for Multi-Scale Tone and Detail Manipulation** | Edge-preserving coarse/detail hierarchy | Broad / Medium / Fine source-information bands |
| 2 | **P1** | Structure vs texture | **Xu et al. - Structure Extraction from Texture via Relative Total Variation (RTV)** | Separates meaningful structure from irregular/repetitive texture | `structureConfidence`, `textureLikelihood`; extinction guidance |
| 3 | **P1** | Modern structure analysis | **Structure-Preserving Texture Smoothing with Scale-Aware Intensity Aggregation Structure Measurement (2023)** | Adaptive scale-aware structure metric | Modern RTV companion; distinguish large texture from meaningful structure |
| 4 | **P1** | Controllable preservation | **Liu et al. - A Generalized Framework for Edge-Preserving and Structure-Preserving Image Smoothing (2020)** | Continuously variable preservation/extinction behaviour via truncated-Huber formulation | Mathematical reference for information survival rather than a hero filter |
| 5 | **P1** | Detail scale | **Subr, Soler, Durand - Edge-Preserving Multiscale Image Decomposition Based on Local Extrema** | Treats detail as spatial oscillation and separates scale from contrast | `characteristicScale`; decide which source variations can disappear |
| 6 | **P1** | Local scale/detail | **Paris, Hasinoff, Kautz - Local Laplacian Filters** | Scale-selective tone/detail manipulation with good edge behaviour | Source residual decomposition and controlled reintegration |
| 7 | **P2** | Flattening target | **Bi, Han, Yu - An L1 Image Transform for Edge-Preserving Smoothing and Scene-Level Intrinsic Decomposition** | Strong piecewise simplification while retaining discontinuities | Possible analysis/target field, not direct output |
| 8 | **P0** | Smooth information extinction | **Bredies, Kunisch, Pock - Total Generalized Variation (TGV)** | Higher-order regularization that retains continuous ramps rather than TV staircasing | Major candidate for turning detailed cheek/knee/shoulder shading into simple smooth tonal fields |
| 9 | **P1** | Region-conditioned flattening | **Long & Zhang - Image Smoothing Combining Edge-Consistency with Region-Piecewise Flatting (2024)** | Different simplification criteria in different regions | Relevant to plane-dependent extinction |
| 10 | **P2** | Texture analysis | **Cho et al. - Bilateral Texture Filtering** | Patch-based distinction between texture and meaningful boundaries | Lightweight texture-likelihood reference |
| 11 | **P1** | Adaptive scale | **Jeon et al. - Scale-Aware Structure-Preserving Texture Filtering** | Spatially varying processing scale | Per-pixel `characteristicScale` |
| 12 | **P2** | Artist controls | **Mishiba - Intensity and Scale Adjustable Edge-Preserving Smoothing Filter (2024)** | Separates processing intensity from spatial scale | Reference for independent artist-facing scale/amount controls |
| 13 | **P2** | Texture-complexity guidance | **Edge-Preserving Image Smoothing Combining Edge Guidance and Texture Complexity Perception (2025/2026)** | Spatial texture-complexity map rather than edge-strength-only decisions | Contemporary comparison for extinction control |
| 14 | **P3** | Learned smoothing | **Zhu et al. - Structure-Preserving Image Smoothing via Contrastive Learning** | Learned meaningful-edge-aware smoothing | Benchmark only |
| 15 | **P3** | Scale-removal baseline | **Zhang et al. - Rolling Guidance Filter** | Suppresses structures below a chosen scale and recovers larger boundaries | Baseline/comparison |
| 16 | **P2** | Guidance utility | **He, Sun, Tang - Guided Image Filtering** | Cheap edge-aware field propagation | Utility, not visual engine |
| 17 | **P0** | Structure / shading / texture separation | **He & Liu - Euler's Elastica-Based Cartoon-Smooth-Texture Image Decomposition (2025)** | Explicitly decomposes grayscale imagery into structural, smooth-shading and oscillatory texture/noise components | Very strong conceptual model for **geometry vs broad shading vs expendable description**; especially useful on Y |
| 18 | **P1** | Color structure/texture decomposition | **Curvature-Guided Cartoon-Texture Decomposition of a Color Image (2025)** | Preserves curvature/fine geometry while separating color image texture | Modern reference for preserving form geometry during information extinction |
| 19 | **P3** | Learned texture separation | **Jiang et al. - Self-supervised Texture Filtering (2025)** | Contemporary self-supervised texture/structure separation | Benchmark against deterministic Pigment analysis |
| 20 | **P1** | Veiling | **Talvala et al. - Veiling Glare in High Dynamic Range Imaging** | Wide-support contrast contamination from scattered light | Physical/perceptual basis for Veil rather than blur |
| 21 | **P1** | Visibility/transmission | **He, Sun, Tang - Dark Channel / Haze Image-Formation Model** | Source visibility controlled by a transmission field mixed with a broad contaminating field | Conceptual basis for Pigment's `T(x)` / visibility |
| 22 | **P1** | Tonal propagation | **Lischinski et al. - Interactive Local Adjustment of Tonal Values** | Smooth structure-respecting propagation of tonal edits | Plane tonal target propagation |
| 23 | **P1** | Photographic tonality | **Reinhard et al. - Photographic Tone Reproduction for Digital Images** | Broad exposure/adaptation/dodge-burn organization | Keep reconstructed planes photographically tonal |
| 24 | **P0** | Gradient-domain extinction | **Fattal, Lischinski, Werman - Gradient Domain High Dynamic Range Compression** | Manipulate gradient magnitudes and reconstruct afterwards | Key model for removing medium/fine tonal description without spatially blurring source geometry |
| 25 | **P1** | Explicit optical softness | **Galetto & Deng - Single Image Defocus Map Estimation through Patch Blurriness Classification and Its Applications** | Continuous per-pixel softness field | Model for **Local Softness**, which must remain independent of the central effect |
| 26 | **P2** | Soft plane membership | **Levin, Lischinski, Weiss - A Closed-Form Solution to Natural Image Matting** | Continuous alpha ownership instead of hard segmentation | Future automatic/assisted plane membership |
| 27 | **P2** | Fuzzy plane decomposition | **Levin, Rav-Acha, Lischinski - Spectral Matting** | Multiple overlapping fuzzy components | Future automatic pictorial-plane decomposition |
| 28 | **P2** | Semantic soft layers | **Aksoy et al. - Semantic Soft Segmentation (2018)** | Soft layers with good transitions using low/high-level information | Future assisted plane discovery; **not needed for manual Phase 3.2** |
| 29 | **P2** | Soft color unmixing | **Aksoy et al. - Unmixing-Based Soft Color Segmentation for Image Manipulation (2017)** | Compact homogeneous soft-color layers analogous to RGBA color layers | Probably one of the best eventual automatic color-plane approaches, but manual planes come first |
| 30 | **P2** | Fast soft color layers | **Akimoto et al. - Fast Soft Color Segmentation (CVPR 2020)** | Neural single-pass decomposition into homogeneous-color RGBA layers | Fast future automatic-plane reference |
| 31 | **P1** | Paint-like translucent layers | **Tan, Lien, Gingold - Decomposing Images into Layers via RGB-Space Geometry** | Represents imagery as ordered single-color paint coats with varying opacity | Strong conceptual reference for pictorial color masses |
| 32 | **P1** | Spatial palette layers | **Tan, Echevarria, Gingold - Efficient Palette-Based Decomposition and Recoloring via RGBXY-Space Geometry** | Spatial + chromatic sparse mixing layers | Useful model of color masses with spatial coherence |
| 33 | **P3** | Nonlinear layer interaction | **Koyama & Goto - Decomposing Images into Layers with Advanced Color Blending** | Inverse decomposition under multiply/screen/burn-like blend modes | Later blending experiments only |
| 34 | **P3** | Generative layer inversion | **Wang et al. - DiffDecompose: Layer-Wise Decomposition of Alpha-Composited Images via Diffusion Transformers (CVPR 2026)** | Modern recovery of constituent alpha-composited layers | Reference only; not appropriate for deterministic Phase 3.2 |
| 35 | **P1** | Field refinement | **Barron & Poole - The Fast Bilateral Solver** | Fast edge-aware optimization of noisy/sparse continuous fields | Refine manually authored memberships or operation fields without making it the visual engine |
| 36 | **P2** | Low-res → full-res architecture | **Gharbi et al. - Deep Bilateral Learning for Real-Time Image Enhancement** | Low-resolution content analysis followed by edge-aware slicing of full-resolution transforms | Architecture reference for eventual performance work |
| 37 | **P2** | Fast field propagation | **Gastal & Oliveira - Domain Transform for Edge-Aware Image and Video Processing** | Fast edge-aware filtering over large scales | Metal-friendly field propagation utility |
| 38 | **P2** | Bilateral-space processing | **Chen, Paris, Durand - Real-Time Edge-Aware Image Processing with the Bilateral Grid** | Efficient representation of spatial/range-dependent transforms | GPU architecture utility |
| 39 | **P2** | Layers + PDE transitions | **Liang et al. - Multiple Facial Image Editing Using Edge-Aware PDE Learning** | Layer decomposition + continuous masks + PDE diffusion + recombination | Architecturally relevant precedent |
| 40 | **P0** | Plane-conditioned diffusion | **Tschumperlé & Deriche - Vector-Valued Image Regularization with PDEs** | Geometry-aware anisotropic diffusion for vector-valued imagery | Candidate mathematical machinery for controlled within-plane Y/AB evolution |
| 41 | **P0** | Directional color/value diffusion | **Weickert - Coherence-Enhancing Diffusion of Colour Images** | Shared color structure tensor steers anisotropic diffusion along coherent form | Central reference for **Y/AB flow that follows form instead of isotropic blur** |
| 42 | **P2** | Modern anisotropic diffusion | **An Anisotropic Diffusion System with Nonlinear Time-Delay Structure Tensor (2022)** | Modern space/direction-varying structure-tensor diffusion | Stability/adaptation reference |
| 43 | **P0** | Smooth transition synthesis | **Orzan et al. - Diffusion Curves: A Vector Representation for Smooth-Shaded Images** | Boundary values diffuse into continuous smooth-shaded fields | Fundamental conceptual reference for broad pictorial Y/AB transitions |
| 44 | **P0** | Improved smooth transitions | **Jeschke - Generalized Diffusion Curves: An Improved Vector Representation for Smooth-Shaded Images (2016)** | Adds greater control away from boundaries plus blurred-edge formulation | One of the strongest papers for the exact **“blur/layering/transition”** behaviour in the references |
| 45 | **P0** | Chroma propagation | **Levin, Lischinski, Weiss - Colorization Using Optimization** | Propagates chroma across luminance-compatible areas | Critical reference for letting **AB travel farther than Y** |
| 46 | **P1** | Gradient reconstruction | **Pérez, Gangnet, Blake - Poisson Image Editing** | Reconstruct image from prescribed gradient behaviour | General reconstruction utility |
| 47 | **P2** | Gradient-domain layer blending | **Xiong & Pulli - Gradient Domain Image Blending** | Builds transition gradient fields and solves for seamless reconstruction | Recombination/seam utility |
| 48 | **P0** | Modern smooth-field reconstruction | **Chakraborty et al. - Image Vectorization via Gradient Reconstruction (Eurographics 2025)** | Detects smooth-shaded regions and represents them as solid, linear or radial gradients | Extremely relevant to replacing complex source shading with **minimal broad gradient fields** |
| 49 | **P1** | Research map | **Tian & Günther - A Survey of Smooth Vector Graphics: Recent Advances in Representation, Creation, Rasterization, and Image Vectorization** | Survey of diffusion curves, gradient meshes and related smooth representations | Map for choosing representations without reinventing the field |
| 50 | **P1** | Final layer coherence | **Sunkavalli et al. - Multi-Scale Image Harmonization** | Matches contrast, texture, noise and blur across composited elements at multiple scales | Later reconstruction stage so altered planes read as **one photograph**, not stacked filters |
| 51 | **P2** | Halo-safe multiscale recombination | **Hanika, Dammertz, Lensch - Edge-Optimized À-Trous Wavelets for Local Contrast Enhancement with Robust Denoising** | Multiscale processing designed to avoid halos / gradient reversals | Safe residual reintegration |
| 52 | **P3** | Boundary recovery | **Osher & Rudin - Feature-Oriented Image Enhancement Using Shock Filters** | Steepens selected discontinuities instead of smoothing them | Optional restrained contour recovery only |
| 53 | **P3** | Physical pigment mixing | **Tan et al. - Pigmento: Pigment-Based Image Analysis and Editing** | Per-pixel mixtures of pigments with multispectral absorption/scattering | Later spectral/pigment phase, not current spatial organization problem |
| 54 | **P0** | Semi-transparent gradient planes | **Richardt et al. - Vectorising Bitmaps into Semi-Transparent Gradient Layers (2014)** | Interactively decomposes bitmap drawings **and studio photographs** into opaque/semi-transparent vector layers represented with linear or radial color gradients | **Extremely relevant now that planes are manual.** Direct precedent for turning selected photographic regions into layered smooth gradient fields |
| 55 | **P0** | Linear gradient layer decomposition | **Du et al. - Image Vectorization and Editing via Linear Gradient Layer Decomposition (SIGGRAPH/TOG 2023)** | Given segmented regions, decomposes imagery into opaque and semi-transparent **linear-gradient layers** | Almost directly matches `manual plane masks → broad editable gradient layers`; very high-priority Phase 3.2 reference |
| 56 | **P1** | Photograph abstraction via gradient layers | **Favreau, Lafarge, Bousseau - Photo2ClipArt: Image Abstraction and Vectorization Using Layered Linear Gradients (2017)** | Uses layered linear gradients to produce simplified representations of photographic regions | Study the **representation**, not its clip-art aesthetic; useful evidence that photographic complexity can be collapsed into few gradient layers |
| 57 | **P1** | Multiscale appearance harmonization | **Sunkavalli et al. - Multi-Scale Image Harmonization, implementation/application perspective** | Explicitly coordinates contrast, texture, noise and blur during pyramid reconstruction | Same paper as #50, but **do not duplicate this in Codex's bibliography**; retain #50 only |
| 58 | **P1** | Soft-color segmentation implementation reference | **Disney Research project material for Unmixing-Based Soft Color Segmentation** | Project page, examples and implementation framing around the Aksoy et al. paper | Same research as #29, so **do not duplicate as a separate paper** |
