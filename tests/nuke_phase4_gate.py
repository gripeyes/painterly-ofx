"""Focused Phase 4 Gate-A renders through the native Pigment OFX node.

This script deliberately renders only the automatic latent/public-plate stage.
Chunk synthesis and spill are not evaluated until Gate A has passed.
"""

import os
import time

import nuke


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUTPUT = os.environ.get("PIGMENT_NUKE_OUTPUT", "/tmp/pigment-phase4-gate-a")
SIZE = int(os.environ.get("PIGMENT_GATE_SIZE", "256"))
PLATES = tuple(
    name for name in os.environ.get(
        "PIGMENT_GATE_PLATES", "skin-fabric,low-light-chroma"
    ).split(",") if name
)
os.makedirs(OUTPUT, exist_ok=True)

all_types = nuke.nodeTypes(force_plugin_load=True)
matches = [
    node_type for node_type in all_types
    if node_type.lower() == "pigment"
    or "org.painterlyofx.pigment_v1" in node_type.lower()
]
if not matches:
    raise RuntimeError("org.painterlyofx.Pigment was not discovered")

node = nuke.createNode(matches[0], inpanel=False)
required = {
    "comparisonMode", "debugView", "phase4PlateCount", "phase4LatentCount",
    "phase4PlateScale", "phase4PlateOverlap", "phase4DebugLatent",
    "phase4DebugPlate", "phase4LumaChunkScale", "phase4ChromaChunkScale",
    "phase4SpillAmount",
}
missing = required.difference(node.knobs())
if missing:
    raise RuntimeError("Missing Phase 4 controls: " + ", ".join(sorted(missing)))

node["comparisonMode"].setValue(6)
node["phase4PlateCount"].setValue(6)
node["phase4LatentCount"].setValue(16)
node["phase4PlateScale"].setValue(48.0)
node["phase4PlateOverlap"].setValue(0.55)
# Gate A excludes all downstream Phase 4 stages.
node["phase4LumaChunkScale"].setValue(0.0)
node["phase4ChromaChunkScale"].setValue(0.0)
node["phase4SpillAmount"].setValue(0.0)


def render(label):
    write = nuke.nodes.Write(inputs=[node])
    write["file"].setValue(os.path.join(OUTPUT, label + ".png"))
    write["file_type"].setValue("png")
    write["channels"].setValue("rgba")
    started = time.perf_counter()
    nuke.execute(write, 1, 1)
    elapsed = time.perf_counter() - started
    print("PHASE4_GATE_A_RENDER", label, round(elapsed, 3))
    nuke.delete(write)


diagnostics = {
    72: "latent-composite",
    73: "latent-reconstruction-error",
    74: "spectral-residual",
    75: "component-recovery-error",
    76: "appearance-unmixing-error",
    77: "plate-alpha-composite",
    78: "plate-y-support-composite",
    79: "plate-ab-support-composite",
    82: "plate-overlap-composite",
}

for plate in PLATES:
    source_path = os.path.join(ROOT, "tests", "visual", "inputs", plate + ".png")
    read = nuke.nodes.Read(file=source_path)
    reformat = nuke.nodes.Reformat(inputs=[read])
    reformat["type"].setValue("to box")
    reformat["box_width"].setValue(SIZE)
    reformat["box_height"].setValue(SIZE)
    reformat["resize"].setValue("fit")
    node.setInput(0, reformat)

    node["comparisonMode"].setValue(0)
    node["debugView"].setValue(0)
    render(plate + "-source")

    node["comparisonMode"].setValue(6)
    node["debugView"].setValue(0)
    render(plate + "-automatic-reconstruction")
    for debug_index, suffix in diagnostics.items():
        node["debugView"].setValue(debug_index)
        render(plate + "-" + suffix)

    for latent in range(1, 17):
        node["phase4DebugLatent"].setValue(latent)
        node["debugView"].setValue(71)
        render("%s-latent-%02d" % (plate, latent))

    for public_plate in range(6):
        node["phase4DebugPlate"].setValue(public_plate)
        for debug_index, suffix in ((77, "alpha"), (80, "y"), (81, "ab")):
            node["debugView"].setValue(debug_index)
            render("%s-plate-%s-%s" % (plate, chr(ord("A") + public_plate), suffix))

    node.setInput(0, None)
    nuke.delete(reformat)
    nuke.delete(read)

print("PHASE4_GATE_A_OUTPUT", OUTPUT)

# Save an inspectable scene using the installed native OFX node.
scene_read = nuke.nodes.Read(
    file=os.path.join(ROOT, "tests", "visual", "inputs", "skin-fabric.png")
)
scene_reformat = nuke.nodes.Reformat(inputs=[scene_read])
scene_reformat["type"].setValue("to box")
scene_reformat["box_width"].setValue(512)
scene_reformat["box_height"].setValue(512)
scene_reformat["resize"].setValue("fit")
node.setInput(0, scene_reformat)
node["comparisonMode"].setValue(6)
node["debugView"].setValue(72)
viewer = nuke.nodes.Viewer(inputs=[node])
scene_read.setXYpos(100, 0)
scene_reformat.setXYpos(100, 100)
node.setXYpos(100, 200)
viewer.setXYpos(100, 300)
scene_path = os.path.join(ROOT, "tests", "visual", "PigmentPhase4GateA.nk")
nuke.scriptSaveAs(scene_path, overwrite=1)
print("PHASE4_GATE_A_SCRIPT", scene_path)
