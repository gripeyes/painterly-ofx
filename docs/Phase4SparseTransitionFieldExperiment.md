# Gate C — first sparse broad-transition value-field experiment

Date: 2026-10-01. Resumed at `4fea793`; the remaining boundary-diagnostic
images/fields were committed unchanged as **`0bdc9c2`** before this experiment.

## Decision

**The first value-only raster curve prototype fails the photographic gate.**
It materially changes the field, but does not deliver continuous convincing
volume with the required information reduction. Fashion still has patch-like
cloth, disrupted facial modelling, and flattened background areas. Cheek and
shoulder lose broad modelling as description falls. Knee volume changes
unevenly. Low-light regions flatten without becoming useful pictorial masses.

Primary diagnosis: **automatic curve extraction/selection is not representing
the required broad interior form topology**. Its sparse level traces often
follow appearance outlines or local folds instead of providing a useful
vocabulary of broad volume transitions. Constant two-sided values along long
curves are a secondary representational limitation. Harmonic reconstruction is
numerically valid, but cannot infer missing interior form from those constraints.

This is a diagnosis supported by the saved curves, coverage, fields and solve
residuals, not a causal proof that side functions or harmonic fields are always
adequate. No controlled side-function/solver replacement was performed. The
result does **not** establish failure of diffusion curves or Jeschke GDCI.
Do not automatically increase curve density, add derivative curves, or promote
the prototype to production. Gate C remains failed.

A1/A2, corrected A3, public alpha/support/appearance and production Gate B are
unchanged. No Spectral Matting work, moments, eigenmode sweep, boundary-value
fitting, hierarchy ablation, source-gradient survival, Spill, Metal port, OFX
installation, or Nuke launch occurred.

## Independent isolated implementation

New files: `src/core/SparseTransitionField.h/.cpp`,
`tests/SparseTransitionFieldTests.cpp`, and
`tests/phase4_sparse_transition_diagnostics.py`. The standalone harness branch
`--sparse-transition-experiment` calls this module and returns before all legacy
Gate-C machinery. It does not call `RegionalEigenField` or reuse its fitted
boundary values. There is no OFX selector or production UI for this experiment.

The result stores separately:

- each plate's broad reconstructed YAB field;
- Y/AB transition-locus maps;
- Y/AB two-sided value-constraint maps;
- the explicit polylines and independent side values;
- per-domain solve/constraint/coverage diagnostics;
- public composite using the original frozen alpha.

## Automatic sparse extraction — one curve family

Independently for each plate and channel family:

1. Sample corrected A3 plate appearance on an 8-pixel Y / 16-pixel AB analysis
   lattice. Each sample is the channel median in a radius 4/8 pixel footprint,
   admitting only support >=0.1. Require at least radius^2 samples. This is
   robust geometry analysis only: no interpolated/filtered target image enters
   the reconstruction. It is not Gaussian, bilateral, guided, WLS, or TGV.
2. Y uses its scalar appearance. AB uses a deterministic principal direction
   of the measured AB covariance, with a fixed sign. Both actual AB channels
   remain independent values carried by the resulting chroma curves; only
   their extraction coordinate is one-dimensional.
3. Extract four Y quantile levels at 1/5,2/5,3/5,4/5 and two AB levels at 1/3,
   2/3 with marching-square edge crossings. Omit ambiguous saddle cells rather
   than invent connectivity. Trace connected polylines deterministically.
4. Reject traces shorter than 48 pixels Y / 96 pixels AB or fewer than four
   vertices. Douglas–Peucker geometry simplification uses tolerance 2/4 pixels.
   No mask smoothing or morphological cleanup occurs.
5. Rank by length; retain at most twelve Y / six AB candidates per plate.
   This is one fixed research configuration, not a density sweep. Rejected
   candidates are not replenished to manufacture a curve count.
6. Require supported independent side samples and appearance change that
   survives doubling a 6-pixel Y / 12-pixel AB normal baseline. The projected
   changes must have the same sign; the near change must exceed 3% of the
   plate's robust 10–90% extraction range, and the broad change at least 25%
   of the near change. This limits oscillation-driven loci. It is a modest
   persistence test, not a claim of full multi-scale contour recognition.

This does not turn the Gate-B contour map into curves. Curves come from plate
appearance; the existing hierarchy only clips admissible constraints and
provides barriers. Some appearance-derived loci still align with silhouettes
or folds. That observed limitation is part of the extraction failure.

## Side-specific values and explicit transition geometry

A locus carries two independent constant YAB side values. Sample corrected A3
appearance every four raster steps at the two normal offsets; admit support
>=0.1. The center-to-side path must remain supported and inside the same
selected Gate-B chunk. Require at least eight samples per side. Use independent
channel medians, not one global plate centroid or a blurred source field.

The two normal-offset rails are raster **value constraints**, separated by
12 pixels for Y and 24 for AB. They define explicit transition geometry:
continuous harmonic interpolation occurs between them. They do not convolve,
feather, expand or change alpha/support masks. No generalized edge-blur profile,
blur operator, derivative condition, curvature moment or second curve family
is implemented. The rails are not an artist-facing blur-radius control.

At overlapping rail pixels, combine value proposals with the original support
weights. The constraint values remain unclipped. Long curves carry constant
side values in this intentionally minimal prototype; this can underrepresent
along-curve volume and is explicitly not hidden by higher-order fitting.

## Harmonic field, barriers and safe fallbacks

Every frozen selected Y/AB chunk edge remains excluded from the solve graph.
No region coalescence or hierarchy topology change occurs. Retained-contour
pixels with source persistence >=0.75 keep their exact automatic appearance as
hard value constraints. Those structural constraints take precedence over rails.
Other retained edges remain barriers with natural zero-flux boundaries rather
than being converted into a dense collection of diffusion curves. This is a
new field formulation, not another barrier-ablation trial.

On each supported connected domain (support >=0.02), minimize:

    E(u) = sum over allowed free grid edges (u[p]-u[q])^2
    u = side value on value rails
    u = exact A3 appearance at required structural contour pixels

Use a four-neighbor geometry-only Laplacian and sparse double LDLT. There is
no data-fidelity target over the interior, source-gradient RHS, eigenbasis,
statistical moment penalty or creative plate adjustment. The Laplace solve is
the curve-constrained field representation, not diffusion of source pixels.

Negligible support retains automatic appearance exactly. A supported domain
without any admissible value constraint also retains automatic appearance,
with that fallback recorded explicitly. Such survival is not an artistic
success, and no confidence/reconstruction error modulates occupancy. Y and AB
solve independently, then composite through the frozen public alpha.

## Same five photographic fixtures, Spill zero

Six public plates, sixteen frozen vocabulary entries, selected Y/AB cuts 24/64.
The legacy Gradient Complexity argument is unused. Same original-dimension
cheek/shoulder/knee crops and 512-square fashion/low-light inputs as preceding
experiments. No full-HD, proxy/PAR or production color-pipeline certification
is implied.

| Fixture | Accepted Y/AB curves, all plates | Domains touched by curves, Y/AB pixel fraction | Medium Y energy/source | Broad normalized Y error |
|---|---:|---:|---:|---:|
| Cheek | 33 / 13 | 0.624 / 0.590 | 0.5724 | 0.0955 |
| Shoulder | 30 / 9 | 0.646 / 0.533 | 0.4547 | 0.0697 |
| Knee | 41 / 16 | 0.736 / 0.589 | 0.5139 | 0.1791 |
| Fashion | 33 / 12 | 0.488 / 0.518 | 0.5681 | 0.1709 |
| Low-light/chroma | 11 / 4 | 0.570 / 0.506 | 0.7595 | 0.1601 |

Domain coverage counts pixels in a domain with at least one accepted rail;
it is not an alpha-weighted transport/spill statistic or proof of useful form.
All band/direction/curvature metrics are measurement only. Gaussian bands in
the metrics helper never enter the reconstruction.

About 95–99% of supported domain pixels belong to solved domains, but only
roughly 49–74% belong to domains touched by internal curves. Many remaining
domains are driven solely by the structural boundary constraints. On fashion,
Y has 9,799 rail constraints against 100,422 structural constraints; AB has
4,741 against 65,244. The sparse extracted form vocabulary does not control
enough of the actual interior organization, and geometric harmonic interpolation
continues to expose uneven boundary/domain character.

AB has fewer loci and a wider fixed transition geometry as designed. A useful
photographic AB-versus-Y organizational advantage is **not** established by
those settings alone; the whole visual gate fails.

## Preservation, numerical validity and saved evidence

- Ten test suites pass, including legacy and Phase-4 regressions and the new
  sparse-transition reference suite. Existing Metal smoke remains a regression
  test only; this model is not ported.
- New tests cover automatic transitions, fixed sparse budgets, deterministic
  fields, independent side values, neutral chroma, supported fallback holes,
  frozen high-confidence barrier appearance and cancellation. Fine checker
  oscillation alone does not manufacture curves. Synthetic validation is not
  photographic acceptance.
- 330 upstream diagnostic files are byte-identical to the frozen prior run.
- Maximum harmonic residual across photographs is 7.42e-15. No numerical
  instability explains the visible failures; residual improvement is not a goal.
- Saved checks report zero changes at conservative high-confidence retained
  markers and zero extrapolation at zero-support pixels. Quantized supports
  cannot certify every in-memory threshold, so these checks are not overstated.
- No contour coordinate, source geometry, alpha, support, or hierarchy changes.
  Appearance can still read flat or soft between fixed contours.

Saved evidence:

`tests/visual/renders/phase4/gate-c-sparse-transition/{cheek,shoulder,knee,fashion,lowlight}`

Includes extracted Y/AB loci, side-value rails, side-value images/PFMs, per-plate
fields/PFMs, composite/source comparison and unclipped PFMs, public/support/
retained views, signed difference, direction maps, metrics, curve vertices and
two side values in CSV, per-domain solves, and preservation/coverage JSON.

    build/pigment_phase4_gate build/phase4-interior-fixtures/fashion.ppm \
      build/phase4-c-sparse-transition/fashion \
      16 6 24 64 .1 0 48 .15 .75 --sparse-transition-experiment

Repeat with the five fixture names. The diagnostics helper takes that directory,
a saved-output directory and `build/phase4-c-regional-eigen/fashion` for upstream
preservation comparison. No existing validated outputs were regenerated.

## Relation to diffusion-curve research and next decision

This is a minimal raster, value-only, paired-side harmonic field. It is
conceptually related to diffusion curves, not a complete vector-curve model.
Jeschke GDCI additionally uses a particular Laplace-function blending model and
generalized edge-blur formulation; neither is implemented here.
[Primary publication record](https://diglib.eg.org/items/6bdf87f2-0e19-4563-b4bf-db0bac182de2).

The smallest next hypothesis would replace the **selection objective**, not
increase density: select a similarly sparse vocabulary that explicitly explains
interior volume/extrema and broad directional transitions, rather than ranking
quantile traces mainly by length. Side functions can then be judged against
those useful geometries. Constant side values may also need an explicit
along-curve representation, but that should not be assumed to fix deficient
geometry. Neither change is implemented automatically after this failed first
prototype. No derivative constraints, more curve families, Spill or Metal follow.
