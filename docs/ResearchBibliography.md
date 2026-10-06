# Pigment Research Bibliography

Updated 2026-10-07. Companion to the [original 58-row research map](PigmentResearchMap.md).

This index preserves the supplied row numbers, groups the 56 distinct main entries, and adds primary author, publisher or institutional links. Row 57 repeats row 50; row 58 supplies Disney project material for row 29. A linked source is not a claim that its complete formulation has been implemented, validated or fully read during this documentation update. The links were checked against source metadata; full-paper reviews and implementation evidence belong in the stage reports.

The original P0–P3 labels describe the supplied historical Phase-3.2 map. They do not override the current paused Gate-C research, frozen upstream stages, or installed hands-on baseline. No algorithms, defaults, UI or execution modes are changed by adding this bibliography.

## Reading routes

- **Structure, broad shading, description and micro residual:** 1–6, 8, 17–18, 40–42, 51. Read these for distinctions and measurements before choosing a renderer.
- **Fuzzy vocabulary and spatial appearance:** 26–33, plus Information Flow below. Compare with the frozen visual-basis and corrected-A3 reports rather than restarting matting work.
- **Geometry and bounded fields:** 24, 43–49, plus the hierarchy references below. Retained contour location and contour appearance are separate questions.
- **Overlapping broad layers:** 54–56, with 17 and 48 as supporting references. C5 already tested an adaptation; its photographic failure evidence must remain visible.
- **Scene-linear color interaction:** 33 and 53, then the separate [Color Interaction design note](Phase4ColorInteractionDesign.md) and [Spectral diagnosis](Phase4SpectralDiagnosis.md). Do not mix spatial transport changes with chemistry repairs.

## 1–19. Information scale, structure and decomposition

| Row | Paper / resource | Primary source | Pigment use and boundary |
|---:|---|---|---|
| 1 | Farbman et al. - Edge-Preserving Decompositions for Multi-Scale Tone and Detail Manipulation | [Paper / project](https://www.microsoft.com/en-us/research/wp-content/uploads/2008/08/Farbman-EPD-small-SG08.pdf) | Multiscale coarse/detail analysis; not permission to restore WLS as the Phase-4 appearance generator. |
| 2 | Xu et al. - Structure Extraction from Texture via Relative Total Variation (RTV) | [Paper / project](https://www.cse.cuhk.edu.hk/~leojia/projects/texturesep/texturesep12.pdf) | RTV structure/texture evidence; distinguish analysis from the paper’s smoothing output. |
| 3 | Structure-Preserving Texture Smoothing with Scale-Aware Intensity Aggregation Structure Measurement (2023) | [Paper / project](https://doi.org/10.1016/j.dsp.2023.103991) | Scale-aware intensity aggregation as a structure measurement; guided smoothing is not the active renderer. |
| 4 | Liu et al. - A Generalized Framework for Edge-Preserving and Structure-Preserving Image Smoothing (2020) | [Paper / project](https://ojs.aaai.org/index.php/AAAI/article/view/6830) | AAAI 2020 formulation; continuous preservation/extinction concepts, not a new hero filter. |
| 5 | Subr, Soler, Durand - Edge-Preserving Multiscale Image Decomposition Based on Local Extrema | [Paper / project](https://doi.org/10.1145/1618452.1618493) | Spatial oscillation/local extrema offer scale evidence separate from contrast. |
| 6 | Paris, Hasinoff, Kautz - Local Laplacian Filters | [Paper / project](https://jankautz.com/publications/LocalLaplacianFiltersSIG11_lowres.pdf) | 2011 Local Laplacian paper; do not conflate with later Fast Local Laplacian work. |
| 7 | Bi, Han, Yu - An L1 Image Transform for Edge-Preserving Smoothing and Scene-Level Intrinsic Decomposition | [Paper / project](https://hub.hku.hk/handle/10722/215521) | L1 simplification/intrinsic-decomposition comparison; not an accepted replacement field. |
| 8 | Bredies, Kunisch, Pock - Total Generalized Variation (TGV) | [Paper / project](https://epubs.siam.org/doi/10.1137/090769521) | Continuous-ramp regularization reference; Pigment’s TGV-output path is not reopened by this catalog. |
| 9 | Long & Zhang - Image Smoothing Combining Edge-Consistency with Region-Piecewise Flatting (2024) | [Paper / project](https://www.sciencedirect.com/science/article/abs/pii/S009784932300290X) | Region-dependent simplification criteria; compare against photographic failure evidence. |
| 10 | Cho et al. - Bilateral Texture Filtering | [Paper / project](https://cg.postech.ac.kr/papers/05_Bilateral-Texture-Filtering.pdf) | Patch texture evidence; bilateral output remains a comparison, not an active-path shortcut. |
| 11 | Jeon et al. - Scale-Aware Structure-Preserving Texture Filtering | [Paper / project](https://cg.postech.ac.kr/papers/safiltering.pdf) | Spatially varying characteristic scale and structure analysis. |
| 12 | Mishiba - Intensity and Scale Adjustable Edge-Preserving Smoothing Filter (2024) | [Paper / project](https://ieeexplore.ieee.org/abstract/document/10579746) | Independent intensity/scale control semantics; does not imply adopting its filtering law. |
| 13 | Edge-Preserving Image Smoothing Combining Edge Guidance and Texture Complexity Perception (2025/2026) | [Paper / project](https://doi.org/10.1016/j.dsp.2025.105710) | Texture-complexity guidance. Full title includes “algorithm”; DOI year and issue year differ—see notes. |
| 14 | Zhu et al. - Structure-Preserving Image Smoothing via Contrastive Learning | [Paper / project](https://repository.eduhk.hk/en/publications/structure-preserving-image-smoothing-via-contrastive-learning/) | Learned benchmark only; no neural dependency is implied. |
| 15 | Zhang et al. - Rolling Guidance Filter | [Paper / project](https://jiaya.me/file/archive/projects/rollguidance/) | Scale-removal baseline; legacy Rolling implementation remains separate. |
| 16 | He, Sun, Tang - Guided Image Filtering | [Paper / project](https://people.csail.mit.edu/kaiming/eccv10/index.html) | Guidance/propagation utility. Current Guided execution must be assessed by its own implementation and parity. |
| 17 | He & Liu - Euler's Elastica-Based Cartoon-Smooth-Texture Image Decomposition (2025) | [Paper / project](https://epubs.siam.org/doi/abs/10.1137/24M167411X) | Structure/smooth/texture interpretation; Pigment does not claim an Euler CST solver. |
| 18 | Curvature-Guided Cartoon-Texture Decomposition of a Color Image (2025) | [Paper / project](https://doi.org/10.1016/j.cam.2025.117075) | Curvature-aware decomposition reference, not an implemented solver. |
| 19 | Jiang et al. - Self-supervised Texture Filtering (2025) | [Paper / project](https://doi.org/10.1145/3744899) | Self-supervised benchmark; no learned model added. |

## 20–25. Optical, tonal and defocus references

| Row | Paper / resource | Primary source | Pigment use and boundary |
|---:|---|---|---|
| 20 | Talvala et al. - Veiling Glare in High Dynamic Range Imaging | [Paper / project](https://graphics.stanford.edu/papers/glare_removal/) | Optical veil is separate from spatial organization; not a rescue for failed Gate C. |
| 21 | He, Sun, Tang - Dark Channel / Haze Image-Formation Model | [Paper / project](https://doi.org/10.1109/TPAMI.2010.168) | The named resource corresponds to Single Image Haze Removal Using Dark Channel Prior. Image-formation reference, not a haze stage. |
| 22 | Lischinski et al. - Interactive Local Adjustment of Tonal Values | [Paper / project](https://www.cs.huji.ac.il/~danix/itm/itm.pdf) | Constraint propagation/local tonal editing; separate from automatic plate ownership. |
| 23 | Reinhard et al. - Photographic Tone Reproduction for Digital Images | [Paper / project](https://research-information.bris.ac.uk/en/publications/photographic-tone-reproduction-for-digital-images/) | Exposure/contrast reference only; no tone mapper or DRT is inserted into the scene-linear node. |
| 24 | Fattal, Lischinski, Werman - Gradient Domain High Dynamic Range Compression | [Paper / project](https://web.tecgraf.puc-rio.br/~scuri/inf1378/pub/lischinski.pdf) | Modify gradients then reconstruct; Pigment does not implement this paper’s HDR compression objective. |
| 25 | Galetto & Deng - Single Image Defocus Map Estimation through Patch Blurriness Classification and Its Applications | [Paper / project](https://link.springer.com/article/10.1007/s00371-022-02609-9) | Optional optical-defocus analysis; not broad-field synthesis. |

## 26–34. Matting, soft segmentation and layer representation

| Row | Paper / resource | Primary source | Pigment use and boundary |
|---:|---|---|---|
| 26 | Levin, Lischinski, Weiss - A Closed-Form Solution to Natural Image Matting | [Paper / project](https://www.cs.jhu.edu/~misha/ReadingSeminar/Papers/Levin06.pdf) | Closed-form matting operator foundation; CVPR 2006 version, later PAMI journal version. |
| 27 | Levin, Rav-Acha, Lischinski - Spectral Matting | [Paper / project](https://people.csail.mit.edu/alevin/papers/spectral-matting-levin-etal-cvpr07.pdf) | Frozen eigenspace/latent vocabulary reference. Pigment’s recovery is an adaptation, not a literal reproduction. |
| 28 | Aksoy et al. - Semantic Soft Segmentation (2018) | [Paper / project](https://yaksoy.github.io/sss/) | Semantic soft-segmentation comparison; semantic cutouts alone do not satisfy Pigment’s plate representation. |
| 29 | Aksoy et al. - Unmixing-Based Soft Color Segmentation for Image Manipulation (2017) | [Paper / project](https://yaksoy.github.io/scs/) | Soft contribution + spatial appearance + reconstruction constraints. See A3 baseline report for actual comparison and reproduction provenance. |
| 30 | Akimoto et al. - Fast Soft Color Segmentation (CVPR 2020) | [Paper / project](https://openaccess.thecvf.com/content_CVPR_2020/html/Akimoto_Fast_Soft_Color_Segmentation_CVPR_2020_paper.html) | Soft color segmentation efficiency reference; not a silent replacement for frozen A2/A3. |
| 31 | Tan, Lien, Gingold - Decomposing Images into Layers via RGB-Space Geometry | [Paper / project](https://cragl.cs.gmu.edu/singleimage/) | RGB-space convex layering; palette layers must not erase spatial appearance diversity. |
| 32 | Tan, Echevarria, Gingold - Efficient Palette-Based Decomposition and Recoloring via RGBXY-Space Geometry | [Paper / project](https://cragl.cs.gmu.edu/fastlayers/) | Two-level spatial/color decomposition; visible RGBXY cells are a rejected Pigment failure mode. |
| 33 | Koyama & Goto - Decomposing Images into Layers with Advanced Color Blending | [Paper / project](https://www.koyama.xyz/project/color_unblending/download/preprint.pdf) | Layer decomposition with blending models; keep appearance law separate from spatial transport. |
| 34 | Wang et al. - DiffDecompose: Layer-Wise Decomposition of Alpha-Composited Images via Diffusion Transformers (CVPR 2026) | [Paper / project](https://arxiv.org/abs/2505.21541) | Diffusion-transformer comparison only; arXiv 2025 / CVPR 2026, not a current runtime dependency. |

## 35–42. Solvers, guidance and directional geometry

| Row | Paper / resource | Primary source | Pigment use and boundary |
|---:|---|---|---|
| 35 | Barron & Poole - The Fast Bilateral Solver | [Paper / project](https://research.google/pubs/the-fast-bilateral-solver/) | Solver infrastructure/affinity reference; no bilateral hero reconstruction. |
| 36 | Gharbi et al. - Deep Bilateral Learning for Real-Time Image Enhancement | [Paper / project](https://arxiv.org/abs/1707.02880) | Low-resolution coefficients/full-resolution guidance architecture reference; current Guided is not claimed to implement HDRnet. |
| 37 | Gastal & Oliveira - Domain Transform for Edge-Aware Image and Video Processing | [Paper / project](https://doi.org/10.1145/2010324.1964964) | Domain Transform comparison and legacy infrastructure; no obsolete smoothing restored. |
| 38 | Chen, Paris, Durand - Real-Time Edge-Aware Image Processing with the Bilateral Grid | [Paper / project](https://groups.csail.mit.edu/graphics/bilagrid/) | Acceleration/data-layout reference; formulation parity must be checked separately. |
| 39 | Liang et al. - Multiple Facial Image Editing Using Edge-Aware PDE Learning | [Paper / project](https://onlinelibrary.wiley.com/doi/abs/10.1111/cgf.12759) | Learned PDE editing comparison, not a new facial/semantic processing dependency. |
| 40 | Tschumperlé & Deriche - Vector-Valued Image Regularization with PDEs | [Paper / project](https://tschumperle.users.greyc.fr/publications/tschumperle_pami05.pdf) | Separate driving geometry from vector-valued evolution; not isotropic appearance blur. |
| 41 | Weickert - Coherence-Enhancing Diffusion of Colour Images | [Paper / project](https://www.mia.uni-saarland.de/weickert/Abstracts/ced-col.html) | Shared structural coherence for color propagation; conceptual support, not an implemented CED claim. |
| 42 | An Anisotropic Diffusion System with Nonlinear Time-Delay Structure Tensor (2022) | [Paper / project](https://www.sciencedirect.com/science/article/pii/S0898122121004314) | Nonlinear time-delay structure tensor; full title includes “for image enhancement and segmentation.” |

## 43–56. Fields, gradient reconstruction and overlapping layers

| Row | Paper / resource | Primary source | Pigment use and boundary |
|---:|---|---|---|
| 43 | Orzan et al. - Diffusion Curves: A Vector Representation for Smooth-Shaded Images | [Paper / project](https://research.adobe.com/publication/diffusion-curves-a-vector-representation-for-smooth-shaded-images/) | Sparse transition geometry and side values; C4’s raster prototype is narrower than full Diffusion Curves. |
| 44 | Jeschke - Generalized Diffusion Curves: An Improved Vector Representation for Smooth-Shaded Images (2016) | [Paper / project](https://pub.ista.ac.at/group_wojtan/projects/2016_Jeschke_GDCI/) | Interior control beyond ordinary curve boundaries; neither C4 nor C5 implements full GDCI. |
| 45 | Levin, Lischinski, Weiss - Colorization Using Optimization | [Paper / project](https://homepages.inf.ed.ac.uk/ksubr/Files/Papers/p689-levin.pdf) | Affinity-constrained color propagation; ownership, support and directed transport remain distinct. |
| 46 | Pérez, Gangnet, Blake - Poisson Image Editing | [Paper / project](https://www.cs.princeton.edu/courses/archive/fall18/cos526/papers/perez03.pdf) | Gradient-domain solve foundations; retained geometry and support-aware constraints are Pigment adaptations. |
| 47 | Xiong & Pulli - Gradient Domain Image Blending | [Paper / project](https://eudl.eu/pdf/10.1007/978-3-642-12607-9_19) | Full title: Gradient Domain Image Blending and Implementation on Mobile Devices. Solver/implementation reference. |
| 48 | Chakraborty et al. - Image Vectorization via Gradient Reconstruction (Eurographics 2025) | [Paper / project](https://research.adobe.com/publication/image-vectorization-via-gradient-reconstruction/) | Solid/linear/radial vocabulary; no assumption that one primitive must explain an entire plate or chunk. |
| 49 | Tian & Günther - A Survey of Smooth Vector Graphics: Recent Advances in Representation, Creation, Rasterization, and Image Vectorization | [Paper / project](https://vc.tf.uni-erlangen.de/publications/Tian22TVCG/) | Representation survey; useful for locating alternatives, not itself a specific implemented method. |
| 50 | Sunkavalli et al. - Multi-Scale Image Harmonization | [Paper / project](https://people.csail.mit.edu/kimo/publications/harmonization/) | Multiscale harmonization comparison; not downstream finishing to hide upstream failure. |
| 51 | Hanika, Dammertz, Lensch - Edge-Optimized À-Trous Wavelets for Local Contrast Enhancement with Robust Denoising | [Paper / project](https://jo.dreggn.org/home/2011_atrous.pdf) | Robust scale-band analysis; denoising output is not an artifact-hiding step. |
| 52 | Osher & Rudin - Feature-Oriented Image Enhancement Using Shock Filters | [Paper / project](https://epubs.siam.org/doi/10.1137/0727053) | Shock-filter edge behavior comparison; sharpening is not evidence of preserved source geometry. |
| 53 | Tan et al. - Pigmento: Pigment-Based Image Analysis and Editing | [Paper / project](https://cragl.cs.gmu.edu/pigmento/) | Pigment-based analysis/editing; chemistry must not compensate for failed spatial organization. |
| 54 | Richardt et al. - Vectorising Bitmaps into Semi-Transparent Gradient Layers (2014) | [Paper / project](https://richardt.name/publications/layered-vectorisation/) | Primary C5 layered-field reference: overlapping semi-transparent gradients, not one field per chunk. |
| 55 | Du et al. - Image Vectorization and Editing via Linear Gradient Layer Decomposition (SIGGRAPH/TOG 2023) | [Paper / project](https://cragl.cs.gmu.edu/gradientlayers/) | Primary C5 linear-gradient layer reference; Pigment is not a general-purpose vectorizer. |
| 56 | Favreau, Lafarge, Bousseau - Photo2ClipArt: Image Abstraction and Vectorization Using Layered Linear Gradients (2017) | [Paper / project](https://www-sop.inria.fr/reves/Basilic/2017/FLB17/photo2clipart.pdf) | Layered linear-gradient abstraction comparator; clip-art appearance remains a Pigment failure criterion. |

## Bibliographic clarifications and additional resources

These notes correct or disambiguate metadata without rewriting the supplied table.

- **4 — generalized smoothing:** the [AAAI 2020 paper](https://ojs.aaai.org/index.php/AAAI/article/view/6830) and [expanded 2021 preprint](https://arxiv.org/abs/2107.07058) are distinct versions; the latter has an expanded author list. [Author repository](https://github.com/wliusjtu/Generalized-Smoothing-Framework).
- **8 — TGV:** the original Bredies/Kunisch/Pock paper is from 2010; an [author-hosted manuscript](https://static.uni-graz.at/fileadmin/_files/_homepages/_karl_kunisch/Papers/224_KK.pdf) is available alongside the SIAM record.
- **13 — edge guidance / texture complexity:** full title is *Edge-preserving image smoothing algorithm combining edge guidance and texture complexity perception*, by Jianwu Long, Qi Luo, Shuang Chen and Yuanqin Liu. The [DOI contains 2025](https://doi.org/10.1016/j.dsp.2025.105710); the Digital Signal Processing issue is volume 169, February 2026. The supplied “2025/2026” is retained rather than treated as two papers.
- **17 — Euler CST:** [open preprint](https://arxiv.org/html/2407.02794v2) complements the SIAM journal version. Its information split influenced C5; that is not implementation of its complete solver.
- **26 — closed-form matting:** the linked PDF is the CVPR 2006 paper; the expanded PAMI journal paper is 2008. Do not confuse it with Spectral Matting.
- **27 — Spectral Matting:** CVPR 2007 and PAMI 2008 versions exist; an [author technical report](https://people.csail.mit.edu/alevin/papers/spectral-matting-TR-levin-etal-o.pdf) provides the longer formulation. Pigment’s exact operator/eigensolve and its component recovery have separate validation histories.
- **28 — semantic soft segmentation:** [author implementation](https://github.com/yaksoy/SemanticSoftSegmentation).
- **29 / 58 — SCU:** [Disney’s project page](https://studios.disneyresearch.com/2017/04/21/unmixing-based-soft-color-segmentation-for-image-manipulation/) is additional material for the same 2017 paper, not a new method. The [V-Sense implementation](https://github.com/V-Sense/soft_segmentation) used for standalone comparisons is a reproduction, not Aksoy’s original implementation. Actual use and limitations are documented in [A3 Reference Comparison](Phase4A3ReferenceComparison.md).
- **31 — RGB-space layers:** [author implementation](https://github.com/CraGL/Decompose-Single-Image-Into-Layers).
- **33 — advanced blending:** [author implementation](https://github.com/yuki-koyama/unblending). A compositing model does not define a spatial Spill topology.
- **34 — DiffDecompose:** [2025 arXiv preprint](https://arxiv.org/abs/2505.21541) and [CVPR 2026 paper](https://openaccess.thecvf.com/content/CVPR2026/papers/Wang_DiffDecompose_Layer-Wise_Decomposition_of_Alpha-Composited_Images_via_Diffusion_Transformers_CVPR_2026_paper.pdf). “Diffusion” here denotes a learned generative model, not the PDE field reconstruction of 43–44.
- **36 — deep bilateral learning:** [author implementation](https://github.com/mgharbi/hdrnet_legacy), provided as a research reference, not a Pigment dependency.
- **42 — time-delay tensor:** the full title ends *for image enhancement and segmentation*. Keep this separate from Weickert’s coherence-enhancing diffusion.
- **44 — generalized curves:** [author preprint](https://pub.ista.ac.at/group_wojtan/projects/2016_Jeschke_GDCI/paper_preprint.pdf). C4 is a limited raster value-curve/harmonic experiment, not the full Jeschke GDCI representation.
- **47 — mobile gradient blending:** the complete title is *Gradient Domain Image Blending and Implementation on Mobile Devices*.
- **50 / 57 — harmonization:** one paper, one canonical entry; row 57 is an intentional cross-reference.
- **53 — Pigmento:** [author implementation](https://github.com/JianchaoTan/Pigmento-PaintingAnalysis) and [preprint](https://arxiv.org/abs/1707.08323) complement the project page. Publication and preprint dates should not be conflated.
- **54 — layered vectorisation:** [paper PDF](https://richardt.name/layered-vectorisation/LayeredImageVectorisation-paper.pdf).
- **55 — gradient layers:** [paper PDF](https://zhengjun-du.github.io/papers/202305/Image_vectorization.pdf) and [TOG DOI](https://doi.org/10.1145/3592128).

## Supplemental Phase-4 foundations (not additional rows in the supplied table)

| Reference | Primary source | Precise relevance |
|---|---|---|
| Aksoy, Aydin, Pollefeys — Designing Effective Inter-Pixel Information Flow for Natural Image Matting | [Paper / project](https://www.microsoft.com/en-us/research/publication/designing-effective-inter-pixel-information-flow-for-natural-image-matting/) | Affine color-mixture reconstruction is distinct from nonnegative directed transport affinity. Signed W_CMF must not be interpreted as graph probabilities or Spill capacities. |
| Arbeláez et al. — Contour Detection and Hierarchical Image Segmentation | [Paper](https://vision.ics.uci.edu/papers/contour-detection-and-hierarchical-image-segmentation-2011/) | Contour/region hierarchy reference. Pigment’s source-cue watershed hierarchy is UCM-style, not a complete gPb-OWT-UCM implementation. |
| He, Kang, Morel — A Formalization of Image Vectorization by Region Merging | [Preprint](https://arxiv.org/abs/2409.15940) | Explicit region merging, scale gain and boundary removal; production Gate B remains plate-conditioned and frozen. |

These sources complement the table; they do not authorize new implementation work. Pigment-mixing and spectral-upsampling references—Sochorová/Jamriška, Haase/Meyer, Jakob/Hanika, Smits, Burns and RealPigment—remain catalogued in the [Color Interaction design note](Phase4ColorInteractionDesign.md).

## Implementation evidence and negative results

Start with the [visual-basis checkpoint](Phase4VisualBasisCheckpoint.md), [Gate-A report](Phase4GateAPass.md) and [A3 reference comparison](Phase4A3ReferenceComparison.md). The bibliography cannot replace those photographs and diagnostics.

The [comparative pipeline](Phase4ComparativePipeline.md) preserves C0–C5 as research evidence rather than treating each failed path as a new production default. Read the [second-moment/barrier decision](Phase4SecondMomentAndBarrierDecision.md), [regional eigenfield experiment](Phase4RegionalEigenFieldExperiment.md), [boundary appearance diagnostic](Phase4BoundaryAppearanceDiagnostic.md), [sparse transition experiment](Phase4SparseTransitionFieldExperiment.md) and [layered broad fields report](Phase4LayeredBroadFields.md) before proposing another Gate-C family.

The [host UI repair report](Phase4HostUIRepair.md) records the hands-on baseline. [Spectral diagnosis](Phase4SpectralDiagnosis.md) records the distinct color-law instability. Neither a paper citation nor a passing unit test constitutes photographic or host acceptance.

## Storage and update policy

Keep this catalog as text links, not vendored PDF collections or generated render archives. Do not add large paper binaries or diagnostic renders to Git merely to complete the bibliography. Record a future full-paper review with the version read, the exact equations adopted, the approximation made and the photographic result in the appropriate stage report.

