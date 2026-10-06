# Isolated Spectral Pigment diagnosis — 2026-10-01

## Decision

**Spectral is not accepted and is not selected for Metal.** One isolated CPU
correction removes the dominant knee punctures, but remaining shadow/color
outliers and very steep interactions still prevent photographic acceptance.
Neither it nor the authors' Jakob/Hanika reference inversion demonstrates a
clear useful advantage over Density. Stop this Spectral development sequence;
keep Density and Linear unchanged. No installation, signing, UI, defaults,
Guided, Gate-C or upstream model changes were made.

The optional `spectralSceneMass` policy defaults to **false**. It is an explicit
standalone comparison, not an active-renderer or artist-control change.

## Frozen inputs and evidence

Baseline source checkpoint: `8b0104d`; installed hands-on baseline: `564d0b8`.
Fashion, knee and lowlight use existing `shared-upstream.snapshot` files with
C0/A3 appearances. No A1–B extraction or hierarchy rebuilding. Spill .8,
Reach 128, Y .05, AB 1, Pigment Density .5. All laws use the same prepared
transport, public alpha, supports and donor/receiver weights.
The frozen harness assigns its PPM fixture samples linear ACEScg values, as in
earlier research. This investigation does not reinterpret them through a new
transfer function or claim equivalence to a newly color-managed Nuke plate.

Fashion is also the source of the saved installed hands-on
`GuidedArtistBaseline.nk`. No running Nuke session or reproducible additional
ballerina source was available. This is **not** certification on a newly
supplied hands-on plate or in-host certification of a repair.

Full traces: `build/phase4-comparative/{fixture}/spectral-diagnosis/trace.f64.gz`.
Losslessly compressed native-endian IEEE754 doubles, 249 columns, one record
per pixel/receiver, ordered pixel then plate. `trace-schema.txt` names all
columns. Includes original layer scene RGB, magnitude, bounded material,
negative and fit residuals, coefficients, all 21 raw/safe reflectances and K/S,
both weight vectors, both mixtures' K/S/reflectance/XYZ/material RGB/magnitude
restoration/residual/nonlinear RGB/YAB, and recombined plate YAB. Final output
is saved as native float32 YAB/RGB PFM; no display transform is processing data.

Safety masks are per-record columns: grey clamp, reflectance floor, proposed
coefficient-bound count, final coefficient-bound masks, input/output invalid,
density clamp and bypass. Negative-channel material clipping is recoverable
exactly from the negative residual; zero-positive-magnitude is explicit.
All wavelength extrema and floor masks are recoverable from raw/safe samples.
There is **no gamut fallback or NaN repair branch** in the existing law; do not
invent a successful safeguard. None of the original photographic reflectances
hit the numerical floor. Floors are not the cause of the original punctures.

Initial traces left a skipped outer Spill call zero-initialized. The replay
explicitly distinguishes this *unexecuted branch* from a black spectral result.
The observer now marks it bypassed. Use `corrected-spectral-{Y,AB}-weights-RGB`
for the original pre-recombination views, not the early similarly named PPMs.
No expensive trace was regenerated just to correct this diagnostic omission.

Observer-on/off final Spectral output is bit-exact. Independent replay of the
original trace reproduces native YAB within 1.25e-7; the correction replay and
native candidate agree within 1.79e-7. Linear and Density PFMs are byte-identical
between the original and candidate runs; hashes are in saved `metrics.json`.

## Localization

1. A zero-positive-magnitude input was encoded as near-black reflectance
   (about 3.5e-5, K/S about 14,405), even though its scene contribution was
   entirely signed residual. Any positive neutral instead normalizes to .9
   reflectance. The first discontinuity is the undefined zero-magnitude
   **bounded-material state**, not eigenspace, chunks or transport.
2. K/S was mixed with normalized transport weights, independently of positive
   scene magnitude. Residual-only/very faint donors therefore supplied strong
   absorption to another plate's illuminated material. This is the principal
   knee black-puncture mechanism. Donor-weighted zero-magnitude contribution
   correlates with spectral-vs-Density RGB departures on knee (r=.493).
3. Punctures also exist in full nonlinear RGB using either transport family,
   before independent Y/AB recombination. Recombination changes their appearance
   but is not their first cause.
4. Very low reflectances of positive, highly saturated materials also produce
   extreme K/S. The warm/cyan sweep has donor K/S above 76,000. A donor weight
   of .0001 drops red from .80 to .57. Logarithmic endpoint tests prove this
   is steep **continuous** response, not a zero-weight branch jump. The residual
   split and underconstrained/limited reflectance fit remain relevant to the
   remaining outliers; the diagnosis does not exonerate them.

## Smallest isolated correction

Keep the scene decomposition and exact residual, but mix material absorption
by its positive scene mass:

`t_j = w_j / sum(w)`

`mbar = sum(t_j m_j)`; `rbar = sum(t_j residual_j)`

`KS_mix = sum(w_j m_j KS_j) / sum(w_j m_j)` when positive mass exists.

At zero mass, the material is irrelevant; choose canonical neutral .9 rather
than encoding residual-only RGB as black pigment. Return scene RGB as
`mbar * integrate(R_mix) + rbar`, then retain the existing Density/YAB policy.
No scene/output clamp, spatial cleanup, fit replacement or topology change.
Negative/HDR reconstruction remains reversible. This is a diagnostic correction
to the material/magnitude interaction, not a new reflectance representation.

Neutral black→HDR sweep's largest adjacent RGB difference falls from 1.33455
to .00130224 (the expected smooth ramp increment). Coefficients/reflectance/K/S
are constant in that neutral sweep. A colored path ending at exact black has
an undefined normalized chromaticity; internal coefficients need not share
one limit, but its rendered amplitude goes to zero continuously.

Residual concern: warm/cyan's .000488 donor step still changes RGB by .38025,
versus Density's .00108. The correction does not pretend this sensitivity is
resolved. Identical colors, zero interaction/Spill, neutral positive HDR,
finite negative/HDR and reversible residuals have deterministic unit tests.

## One reference inversion comparison

Used the [authors' rgb2spec implementation](https://github.com/mitsuba-renderer/rgb2spec)
of [Jakob/Hanika](https://rgl.epfl.ch/publications/Jakob2019Spectral), commit
`721145dedf2491851bd46ab8fd165955cb38ddaf`, unmodified source, 32³ ACES2065-1/D60
table and authors' one-step LM refinement. Standalone bridge only; not linked
into Pigment. AP1 material converts through XYZ to AP0. Tiny negative AP0
components are outside that reference's bounded domain and are retained in
the exact RGB residual, not discarded from the scene image. Count/max are
exported. No transfer function, DRT, display transform or final gamut clipping.

Validated reference fits against its **own** fine D60 integration separately
from Pigment's coarse 21-sample adapted-D65 quadrature. Native mean fit error
is 7.82e-5 fashion, 6.29e-4 knee (some large outliers), .02017 lowlight. Its
coarse-quadrature fit is worse; this difference must not be attributed entirely
to its optimizer. Reference spectra with unchanged KM interaction produce
purple knee punctures and more local outliers, not a successful replacement.
This is failure of this adaptation/comparison, **not** of the research family.
No Burns/Smits or Mixbox alternative was added.

## Photographic decision

Compact comparisons and signed/full-precision output:
`tests/visual/renders/phase4/spectral-diagnosis/{fixture}`.

Measurement-only localized RGB departure from Density above .02:

| Fixture | Original | Scene-mass correction | Authors' inversion + correction |
|---|---:|---:|---:|
| Fashion | 108 | 86 | 94 |
| Knee | 6415 | 1762 | 7602 |
| Lowlight | 1950 | 801 | 3166 |

These are supporting diagnostics, **not** an acceptance score or filtered
processing. Knee is visibly much cleaner under the correction. Fashion shows
no decisive pictorial improvement; lowlight retains problematic shadow/color
departures. Negative RGB below -.01 is not automatically an artifact in a
scene-linear contract, but remaining punctures, sensitivity and photographic
appearance prevent acceptance. Do not install/promote or Metal-port Spectral.

If future research explicitly reopens this subject, the remaining question is
bounded-material/out-of-model residual behavior under nonlinear absorption and
metamer sensitivity—not spatial smoothing or another Gate-C change.

## Reproduction

`pigment_phase4_gate <fixture.ppm> build/phase4-comparative/<fixture> 16 6 24 64 .1 .8 128 .05 1 --spectral-diagnosis`

Use `--spectral-mass-comparison` for the optional correction; it never changes
defaults. `pigment_spectral_continuity <csv> [--mass-weighted|--density]` records
the sweeps and logarithmic endpoint tests. Python scripts beside the harness
replay, compare the standalone reference, measure and produce contact sheets.
All 14 regression suites passed. Installed bundle signature verified unchanged.
