# Phase 3 Integrated Pigment prototype

## Status

The persistent `org.painterlyofx.Pigment` factory is implemented and renders through
the normal Pigment OFX bundle. It is technically usable and interactive in Nuke, but
this first prototype **does not pass the visual acceptance gate**. Its soft-region
result remains recognizably broad edge-aware smoothing: it changes the image more
than Guided DetailCollapse, but does not reliably reorganize fruit, material events,
or cloth into fewer convincing pictorial masses. Work stopped at that gate instead
of polishing or selecting this algorithm for production.

## Implemented graph

The node performs one GPU-resident RGB → YAB → RGB graph containing multiscale
structure analysis, a deterministic three-octave Veil, four independently derived
control fields, three soft region iterations, a distinct boundary-extinction pass,
optional reuse of the Guided Metal cleanup, independent AB migration, and
scale-separated residual reintegration.

The soft-region estimator uses 81 samples arranged as a fixed 9×9 stratified budget.
The grid covers approximately `[-Mass Scale, +Mass Scale]` in image space after PAR
and render-scale correction; it is not a fixed 9×9-pixel footprint. Mode coordinates
move between iterations. Estimation occurs at half resolution, with a continuous
half-to-quarter-resolution crossfade from 20 to 28 full-resolution pixels.

`Mass Scale`, `Structure Scale`, and advanced `Boundary Scale` are independent.
Processing strength, boundary protection/extinction, chroma migration, and detail
retention remain separate scalar fields. Negative/HDR YAB values are not clipped.
Alpha is copied exactly and premultiplied images are processed in straight RGB.

## Performance

Warm CPU-backed no-copy measurements on the Apple M2 at 1920×1080, one warm-up and
five samples, were:

| Mass Scale | Median | p95 | Last GPU time |
|---:|---:|---:|---:|
| 2.5 px | 60.0 ms | 63.8 ms | 58.3 ms |
| 18 px | 59.8 ms | 60.2 ms | 57.7 ms |
| 24 px | 59.7 ms | 61.2 ms | 56.7 ms |

The target of under 150 ms is met. The prototype's important performance defect is
scratch memory: it reports about 1.04 GB at 1080p and about 1.17 GB total Metal device
allocation. This is deliberately not optimized after the visual gate failed. Most of
that allocation can be removed later by lifetime-based aliasing if the algorithm is
revised and accepted.

The development harness command is:

```sh
PIGMENT_METAL_RESOURCE_DIR="$PWD/build" \
  ./build/pigment_metal_harness --integrated --width 1920 --height 1080 \
  --mass-scale 18 --warmups 1 --rounds 5
```

## Visual assessment

The saved grape result softens local grape/specular structure but leaves most grape
contours organized as before. The laundry result creates broader color areas, yet its
transition character still reads as blur rather than selective consolidation. The
boundary-protection field is spatially broad and the mean-shift update converges
toward weighted neighborhood averages rather than stable, perceptually meaningful
modes. Boundary extinction then adds another averaging step.

Consequently the prototype does not yet satisfy the required organizational change:
small events are attenuated, but fewer coherent masses do not emerge reliably, and
the distinction from a strong edge-aware smoother is insufficient.

Principal references:

- `tests/visual/renders/pigment/fruit-grapes-original.png`
- `tests/visual/renders/pigment/fruit-grapes-integrated.png`
- `tests/visual/renders/pigment/fruit-grapes-region-mode.png`
- `tests/visual/renders/pigment/fruit-grapes-boundary-protection.png`
- `tests/visual/renders/pigment/laundry-cloth-original.png`
- `tests/visual/renders/pigment/laundry-cloth-integrated.png`
- `tests/visual/renders/pigment/laundry-cloth-pre-boundary-mass.png`

## Generated fixture provenance

Both fixed plates were made with OpenAI's built-in image-generation model in normal
generation mode (not an edit), then copied into `tests/visual/inputs` without further
image manipulation.

Fruit/grape prompt:

> A high-detail photorealistic studio still life designed as an image-processing test
> plate: several dense clusters of red, purple, and green grapes with many small
> specular highlights, translucent skins, stems, subtle bloom, a few sliced fruit
> accents, natural imperfections, varied local color, soft neutral daylight, broad
> readable silhouette, dark simple background, no text, no stylization, square frame.

Laundry prompt:

> A documentary-style photorealistic image-processing test plate of colorful laundry
> and layered woven fabrics hanging outdoors, rich red, blue, yellow, green, violet,
> and neutral cloth, visible weave and folds, one broad human silhouette partly behind
> the fabric, natural overcast daylight, deep chromatic shadows, broad color fields
> plus fine textile detail, no text, no stylization, square frame.

## Recommended next visual iteration

Do not optimize this exact graph first. Revisit the region estimator so it seeks a
stable density mode or representative sample instead of repeatedly averaging YAB.
The next experiment should preserve the same physical Mass Scale contract and
independent fields, compare mode/medoid attraction against this frozen output, and
prove that it reduces region count while retaining broad silhouettes before Metal
memory work or UI refinement resumes. This is a recommendation only; no follow-on
algorithm was started in this phase.
