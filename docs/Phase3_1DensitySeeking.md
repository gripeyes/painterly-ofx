# Phase 3.1 Density-Seeking Mass Formation

## Outcome

Representative Mode is implemented beside the preserved **Weighted Mean (legacy
research)** backend. On the fixed grape and laundry plates it creates a materially
different organization rather than merely increasing softness. The first visual gate
passes, with contour ringing and locally melted/cellular transitions retained as
known research failures.

No DetailCollapse, memory optimization, spectral mixing, Kuwahara, RTV,
TemporalSmear, DensityVeil product, or Resolve work was performed.

## Estimator

Each of the three rolling iterations retains the existing 81-sample stratified
support over physical Mass Scale. A first Metal pass estimates density for every
coarse sample from spatial and normalized YAB kernel agreement. A second pass:

1. scores the 81 candidates using their density, compatibility with the current
   mode, spatial support, and effective boundary permeability;
2. retains the four strongest candidates;
3. softly blends those representatives using **Mode Selectivity** as temperature;
4. moves YAB and mode position continuously according to the existing independent
   mass-strength field.

Boundary extinction weakens protection while scoring candidates; it is not folded
into processing strength. Representative colors therefore remain near actual source
populations instead of the arithmetic mean of all compatible samples.

The CPU semantic reference and unit fixture verify that Representative Mode remains
closer to a dominant source population than Weighted Mean, and that Mode Selectivity
changes this attraction continuously.

## Interactive comparison and diagnostics

Comparison Mode now contains:

- Original
- Current Guided DetailCollapse
- Weighted Mean (legacy research)
- Representative Mode

The added diagnostics are Local Density, Winning / Dominant Mode, Mode Confidence,
Representative Distance, Candidate Competition, Legacy Weighted Mean, and
Representative Mode Result. The existing Y Mass, AB Mass, Veil, boundary, residual,
and difference views are unchanged.

Matched renders are under `tests/visual/renders/pigment`. The critical references are:

- `fruit-grapes-weighted-mean.png`
- `fruit-grapes-representative-mode.png`
- `fruit-grapes-dominant-mode.png`
- `laundry-cloth-weighted-mean.png`
- `laundry-cloth-representative-mode.png`

## Visual findings

At Amount 0.9, Mass Scale 30, Mass Strength 0.78, Mode Selectivity 0.88, and reduced
detail reintegration:

- green/yellow, burgundy, and blue-black grapes consolidate toward separate plausible
  source populations instead of muddy intermediate color;
- numerous grape interiors and small boundaries disappear while the three large
  cluster organizations remain legible;
- laundry cloth interiors form broader blue, red, yellow, and green families;
- wall and grass simplify substantially while the principal cloth silhouettes remain;
- the result is clearly distinct from the softer legacy output and does not form a
  fixed palette or rectilinear/Voronoi grid.

Known failures at these aggressive settings:

- protection/mode transitions can form bright or dark contour ringing;
- some boundaries look melted or cellular;
- dominant-mode debug output is intentionally much flatter than the final result;
- high selectivity can approach hard candidate switching, especially around sparse
  highlights and narrow gaps;
- no temporal-stability claim has been made.

## Performance

On the Apple M2 at 1920×1080, Mass Scale 18, one warm-up and five measured frames,
Representative Mode measured approximately 160 ms median and 167 ms p95; the matched
legacy graph measured 77 ms median and 82 ms p95. Representative Mode remains below
the 300 ms research limit.
Scratch allocation intentionally remains unoptimized and is approximately 1.13 GB at
1080p.

## Completion gate

The representative-medoid experiment demonstrates fewer coherent pictorial masses
and plausible population colors, so the optional true-KDE experiment was not started.
The next decision should be based on artist evaluation of the saved Nuke scene and
renders. No subsequent phase has been begun automatically.
