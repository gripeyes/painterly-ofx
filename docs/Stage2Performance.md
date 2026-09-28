# Stage 2 Performance Prototype Report

Validation date: 2026-09-28. Machine: Apple M2 MacBook Air, 8 GPU cores,
16 GB unified memory, macOS deployment target 13.0. Times are research measurements,
not release guarantees.

## Implemented backends

- **Reference Bilateral CPU** preserves the original direct joint-bilateral source
  and output. A fixed deterministic fixture protects it with a golden hash.
- **Guided CPU** performs four weighted local-linear rolling reconstructions. The
  independent boundary field supplies regression confidence; processing strength is
  applied only to the update gain.
- **Domain Transform CPU** performs symmetric forward/reverse horizontal and vertical
  recursive passes. Axis order alternates across four rolling updates. Domain distance
  includes normalized YAB difference and an independent permeability cost.
- **Guided Metal** uses independently written Metal kernels and box/Gaussian MPS
  primitives. Conversion, masks, structure/boundary fields, weighted statistics,
  four iterations, reintegration, debug selection, and RGB output stay GPU-resident.
- **Domain Transform Metal** is a visible reserved choice. CPU-backed renders explicitly
  warn and use Domain Transform CPU; native-buffer renders ask the host for a CPU retry.

CPU-buffer Guided Metal first attempts no-copy shared-buffer wrapping. Strided and
offset storage is packed into shared staging, synchronized once, and unpacked. The
strided/premultiplied path is covered by the Metal harness. CPU/Metal normalized YAB
RMSE on the deterministic guided-reconstruction fixture is `1.58e-6`.

## Timing summary

Guided Metal values are five measured iterations after one warmup. CPU 1080p/4K values
are single runs because allocation and execution cost dominate. Reference Mass Scale
24 was stopped after exceeding the 60-second research cap.

| Backend / resolution | Mass 2.5 | Mass 8 | Mass 24 |
|---|---:|---:|---:|
| Guided Metal 256² median | 3.9 ms | 5.7 ms | 4.0 ms |
| Guided CPU 256² median | 39.2 ms | 36.1 ms | 40.1 ms |
| Domain CPU 256² median | 34.7 ms | 35.1 ms | 35.2 ms |
| Reference CPU 256² median | 596.7 ms | 7966.3 ms | >60 s cap |
| Guided Metal 1920×1080 median | 86.8 ms | 89.1 ms | 89.2 ms |
| Guided CPU 1920×1080 | 1007.9 ms | 1012.0 ms | 1019.2 ms |
| Domain CPU 1920×1080 | 1198.0 ms | 1215.9 ms | 1213.4 ms |
| Guided Metal 3840×2160 median | 335.3 ms | 349.8 ms | 348.3 ms |
| Guided CPU 3840×2160 | 7042.4 ms | 6801.2 ms | 6714.6 ms |
| Domain CPU 3840×2160 | 5620.3 ms | 5610.9 ms | 5583.4 ms |

The current 1080p Guided Metal path meets the sub-second interactive target. Its
private scratch footprint is approximately 727 MB at 1080p and 2.91 GB at 4K. The
per-instance CPU-render pool reuses matching scratch textures; native asynchronous
renders retain their transient resources through command-buffer completion.

## Visual comparison

Nuke generated matched final renders for detailed CGI/specular breakup, low-light
chroma, skin/fabric, and broad color fields, plus all requested CGI debug views under
`tests/visual/renders`.

Observed prototype characteristics:

- Guided is the stronger mass-forming approximation on the synthetic breakup metric;
  fine-energy ratios were about 0.77 at Mass Scale 8 versus 0.91 for Domain Transform.
- Domain Transform retains more small events and can expose scan-direction character,
  despite symmetric passes and alternating axis order. It is not currently the closer
  replacement for the bilateral mass placement.
- Guided can become locally plastic when Tone/Chroma Similarity are wide and Internal
  Variation is low. Boundary Preserve can also produce soft halos around coherent
  high-contrast silhouettes.
- The Metal structure guide uses an MPS Gaussian approximation. Full stress-fixture
  RGB drift from Guided CPU is about `5.43e-4` RMSE; the isolated guided solve satisfies
  the stricter normalized-YAB parity gate.
- Neither accelerated backend reproduces the bilateral reference exactly. Backend
  selection therefore remains an artistic decision, not an automatic choice.

## Host validation

Nuke 17.0v1 discovered the installed `/Library/OFX/Plugins/Pigment.ofx.bundle`, created
the real `OFXorg.painterlyofx.DetailCollapse_v1` node, rendered all saved views, opened
the connected validation scene normally, and recalculated live changes to Amount,
Mass Scale, and Structure Scale with Guided Metal selected. The run used Nuke's
CPU-backed OFX images and Pigment's explicit Metal no-copy/staging path.

Resolve Studio 21 loaded `org.painterlyofx.DetailCollapse` and displayed **Pigment
DetailCollapse (Research)** in Fusion's tool search. The first attempted instance in
the existing validation project caused the Resolve UI to stop responding before
queue/buffer negotiation could be logged. This is a specific unresolved host blocker;
native-buffer rendering is not claimed as validated. The CPU reference and Nuke path
remain available while this is investigated.
