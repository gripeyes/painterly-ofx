# DetailCollapse Mass Formation Research

Mass Formation is an internal processing mode reserved for the future
`DetailCollapse` effect. It is not an OFX effect, plug-in factory, or public node.
The current code is a CPU visual reference used to evaluate the Rolling YAB Mass
architecture before an optimized backend is selected.

## Architectural invariants

- **Mass Scale** controls the characteristic size of information consolidated.
- **Structure Scale** controls the scale at which boundaries become significant.
  The structure guide is simplified before its gradients are measured, so a small
  high-contrast mark can lose protection while a broad silhouette remains protected.
- `processingStrength` and `boundaryProtection` are independent scalar fields.
  Both accept constant or per-pixel data. Protection values are permeability:
  `1` crosses freely and `0` blocks propagation.
- DensityVeil can later modulate processing strength without coupling its generator
  to DetailCollapse. Selective boundary extinction can independently modulate
  boundary permeability.
- YAB, negative values, and HDR values are never clipped.
- The bilateral reference requests the full source RoD. Correctness takes priority
  over claiming tile support for a global iterative operation.

## Current reference

`detail::RollingYabMassOperator` starts with a Mass Scale Gaussian seed, then performs
four rolling joint-bilateral updates. Every update filters the original YAB signal
while the previous result supplies similarity guidance. Spatial distance, explicit
YAB similarity, and boundary-path permeability determine sample weights. Per-pixel
processing strength controls the continuous update gain.

The Gaussian is only a scale seed. The visible consolidation is produced by the
region-aware rolling operation. `Internal Variation` reinjects a lower-frequency
version of the source rather than raw fine detail.

The implementation is deliberately a reference, not the intended 4K production
backend. Once its visual behaviour is accepted, guided filtering and the domain
transform can be compared against its output.

## Research basis and prototype order

The first prototype follows the scale-removal and iterative-recovery idea in the
[Rolling Guidance Filter](https://www.cse.cuhk.edu.hk/~leojia/projects/rollguidance/paper/%5BECCV2014%5DRollingGuidanceFilter_5M.pdf),
using [bilateral weighting](https://projects.iq.harvard.edu/sites/projects.iq.harvard.edu/files/imagenesmedicas/files/tomasi1998kg.pdf)
as an explicit CPU reference.

The unchanged follow-up order is:

1. Rolling YAB Mass reference.
2. Soft spatial-YAB region-mode experiment based on the grouping principles of
   [mean shift](https://cs.brown.edu/people/pfelzens/engn1610/PAMIMeanshift.pdf).
3. [Relative Total Variation](https://lxu.me/mypapers/texturesep12.pdf) first as a
   texture-likelihood or boundary-significance diagnostic.
4. Compare a [guided filter](https://mlanthology.org/eccv/2010/he2010eccv-guided/)
   or [domain-transform](https://doi.org/10.1145/2010324.1964964) production backend
   against the accepted CPU reference.

L0 gradient minimization is not a primary candidate because its edge steepening and
gradient sparsification conflict with the requirement to avoid posterization and a
cartoon appearance. Perona–Malik diffusion remains a possible finishing regularizer,
not the definition of Mass Formation.

The [SpektraFilm repository](https://github.com/chaert-s/spektrafilm-ofx) was reviewed
only for general production OFX practices. Pigment contains no copied or derived GPL
implementation.

## Evaluation

`pigment_mass_tests` covers scale separation, constant and per-pixel fields, field
independence, full-RoD enforcement, HDR/negative values, cancellation, deterministic
row scheduling, and scratch ownership.

`pigment_mass_research` generates an HDR synthetic scene with small specular detail
and a broad silhouette. It exposes Mass Scale and Structure Scale separately and can
write source, result, and boundary-permeability PFM images for external inspection.
