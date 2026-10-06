# Isolated Color Interaction CPU experiment

Design written before implementation, 2026-10-01. Frozen spatial architecture and C0–C4 remain unchanged. All laws consume the same plate appearances and directed Spill weights. Linear YAB stays the default; no automatic promotion or Metal port follows numerical success.

## Reference comparison and choice

- [Sochorová/Jamriška 2021](https://dcgi.fel.cvut.cz/wp-content/wpallimport-dist/publications/pdf/publications-2021-sochorova-tog-pigments-paper.pdf): pigment concentrations plus additive RGB residuals enable RGB-compatible K–M mixing. Measured/surrogate primary pigments and inverse concentration fitting are more machinery than this initial comparison needs. Adopt the residual principle and K–M foundation, **not** Mixbox's pigment primaries/LUT or claims of equivalent behavior.
- [Jakob/Hanika 2019](https://rgl.epfl.ch/publications/Jakob2019Spectral): a sigmoid of a quadratic wavelength polynomial provides a smooth bounded three-coefficient spectrum. Choose this function space, fitted deterministically per bounded material color; omit their precomputed upsampling table/full optimization pipeline. No spectral image buffers are required.
- [Smits 1999](https://doi.org/10.1080/10867651.1999.10487511) uses representative reflectance basis spectra; [Burns 2017](https://arxiv.org/pdf/1710.06364) reconstructs representative spectra and mixes using weighted geometric means. These are useful simpler baselines, but neither identifies actual pigments from RGB. Prefer the smooth three-coefficient model over shipping seven primary spectra or a full smoothness-constrained spectral optimizer. Density supplies a nonspectral log-absorbance comparison, not Burns' spectral method.
- [Haase/Meyer 1992](https://doi.org/10.1145/146443.146452) supplies the absorption/scattering foundation. For this prototype, infer K/S=(1−R)^2/(2R), assume S=1 for every material, average K using the **existing** interaction weights, then use the infinite-thickness K–M inversion. This equal-scattering representative model is not measured paint, finite-thickness compositing, or pigment identification.
- [RealPigment, Lu et al. 2014](https://oar.princeton.edu/handle/88435/pr1n53d?mode=full) compares example-driven compositing and K–M models. No example charts are available here; retain its lesson that physical sophistication does not establish artistic usefulness. Accessible author abstracts/source and the practical 2021 K–M derivation were reviewed; inaccessible full manuscripts are not claimed to have been reproduced.

## Reversible scene/material split

For scene-linear working RGB c, set m=max(0,c_R,c_G,c_B)/.9. If m>0, b=max(c,0)/m; otherwise b=0. Thus b is bounded in [0,.9], while m carries arbitrary positive scene magnitude, including HDR. Negative RGB is retained outside the model.

For Density, material decode s=b (with a tiny log floor only inside absorption), residual r=c−m·s. For Spectral, fit R(λ)=.5+.5 p(λ)/sqrt(1+p(λ)^2), with p quadratic over normalized wavelength. Integrate to decoded working RGB s; residual r=c−m·s captures negative values **and** fit/out-of-reflectance-gamut error. Therefore c=m·s+r. Residual is not a source-confidence fallback: it is an explicit reversible color representation. Zero interaction returns the original appearance exactly.

Use 21 transient quadrature wavelengths, 380–780 nm at 20 nm, [CIE 1931 observer](https://cie.co.at/datatable/cie-1931-colour-matching-functions-2-degree-observer) and D65. Normalize quadrature to D65 white; Bradford-adapt to the configured working white (D60 for ACEScg), then XYZ→configured linear RGB. This is a material reference-illuminant convention, not a display transform, DRT or relighting pipeline. No transfer function is applied to node input/output.

For normalized existing interaction weights t_j, mix magnitude and residual linearly: m̄=Σt_j m_j, r̄=Σt_j r_j. Density uses exp(Σt_j log(max(b_j,ε))). Spectral uses equal-S K–M on evaluated compact spectra. Decode nonlinear scene result n=m̄·materialMix+r̄. Keep measured fit errors and residual magnitudes available to the standalone comparison.

## Independent Y/AB and Pigment Density

Use two weight sets, precisely the existing receiver+donor Y and AB weights. Neither alpha nor support nor F changes. For each set, let l be its linear mixture and n its nonlinear candidate. Pigment Density D∈[0,1] sets Y*=Y_l+D(Y_n−Y_l), leaving n's A/B unchanged. Equivalently add neutral scene light (1−D)(Y_l−Y_n) to decoded RGB; this is a scene-linear residual, not a display transform. Take final Y from the Y-weight set and A/B from the AB-weight set. D=0 preserves linear scene luminance; D=1 admits the model's density change. AB-only Spill leaves composite Y bit-exact even at D=1 because its Y-weight set has no donor. Density therefore affects only the independent Y interaction, never transport. Linear YAB ignores Density and retains its exact historical arithmetic.

The first diagnostic used chroma scaling by Y*/Y_n. It was rejected after dark/negative residuals produced near-zero signed Y and excessive chroma amplification. Those outputs are retained under `color-interaction-initial-normalization`. The corrected version never divides by scene luminance and never scales the signed residual.

## Controlled test

Add Research Color Interaction (Linear YAB / Density / Spectral Pigment) and Pigment Density, shared across Pigment and Research interfaces without changing macros. Compare fashion, knee and lowlight using saved upstream snapshots and C0/C1/C4 fields, one prepared transport per fixture. Export pre/post, signed law differences, float results, donor→receiver trajectories, density endpoints and numerical topology checks. No A1–B reruns, geometry edits, source filters, finishing or Metal work. Select a law only from photographic evidence; uncertain results remain explicit comparisons.
