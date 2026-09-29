"""Focused CPU-first Phase 3.3 artistic gate renders."""

import os
import time
import nuke

root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
output = os.environ.get("PIGMENT_NUKE_OUTPUT", "/tmp/pigment-phase33-gate")
width = int(os.environ.get("PIGMENT_GATE_WIDTH", os.environ.get("PIGMENT_GATE_SIZE", "512")))
height = int(os.environ.get("PIGMENT_GATE_HEIGHT", os.environ.get("PIGMENT_GATE_SIZE", "512")))
only_spatial = os.environ.get("PIGMENT_GATE_SPATIAL_ONLY", "0") == "1"
plate_names = tuple(os.environ.get("PIGMENT_GATE_PLATES", "skin-fabric,low-light-chroma").split(","))
os.makedirs(output, exist_ok=True)
node = nuke.createNode("Pigment", inpanel=False)
node["comparisonMode"].setValue(5)
node["phase33ComputeBackend"].setValue(2)
node["phase33AutomaticOccupancy"].setValue(1.0)
node["phase33AutomaticPlaneStrength"].setValue(1.0)
node["phase33TransitionSolver"].setValue(1)
node["fineExtinction"].setValue(0.9)
node["mediumExtinction"].setValue(0.8)
node["veilAmount"].setValue(0.0)
node["localSoftness"].setValue(0.0)


def render(label):
    write = nuke.nodes.Write(inputs=[node])
    write["file"].setValue(os.path.join(output, label + ".png"))
    write["file_type"].setValue("png")
    write["channels"].setValue("rgba")
    start = time.perf_counter()
    nuke.execute(write, 1, 1)
    print("PHASE33_GATE_RENDER", label, round(time.perf_counter() - start, 3))
    nuke.delete(write)


for plate in plate_names:
    read = nuke.nodes.Read(file=os.path.join(root, "tests", "visual", "inputs", plate + ".png"))
    reformat = nuke.nodes.Reformat(inputs=[read])
    reformat["type"].setValue("to box")
    reformat["box_width"].setValue(width)
    reformat["box_height"].setValue(height)
    reformat["resize"].setValue("fit")
    node.setInput(0, reformat)
    node["comparisonMode"].setValue(0)
    node["debugView"].setValue(0)
    render(plate + "-original")
    node["comparisonMode"].setValue(5)
    variants = ((2, "spatial"),) if only_spatial else ((1, "convex"), (2, "spatial"))
    for plane_source, source_name in variants:
        node["phase33PlaneSource"].setValue(plane_source)
        for shading, shading_name in ((1, "source-smooth"), (2, "tgv")):
            node["phase33ShadingModel"].setValue(shading)
            node["debugView"].setValue(0)
            render("%s-%s-%s" % (plate, source_name, shading_name))
        node["phase33ShadingModel"].setValue(1)
        node["debugView"].setValue(46)
        render("%s-%s-memberships" % (plate, source_name))
    node.setInput(0, None)
    nuke.delete(reformat)
    nuke.delete(read)

print("PHASE33_GATE_OUTPUT", output)

# Leave a normal interactive 1080p scene using the installed OFX node.
scene_read = nuke.nodes.Read(
    file=os.path.join(root, "tests", "visual", "inputs", "skin-fabric.png")
)
scene_reformat = nuke.nodes.Reformat(inputs=[scene_read])
scene_reformat["type"].setValue("to box")
scene_reformat["box_width"].setValue(1920)
scene_reformat["box_height"].setValue(1080)
scene_reformat["resize"].setValue("fit")
node.setInput(0, scene_reformat)
node["comparisonMode"].setValue(5)
node["phase33PlaneSource"].setValue(1)
node["phase33ShadingModel"].setValue(1)
node["debugView"].setValue(0)
viewer = nuke.nodes.Viewer(inputs=[node])
scene_read.setXYpos(200, 0)
scene_reformat.setXYpos(200, 100)
node.setXYpos(200, 200)
viewer.setXYpos(200, 300)
scene_path = os.path.join(root, "tests", "visual", "PigmentPhase33Validation.nk")
nuke.scriptSaveAs(scene_path, overwrite=1)
print("PHASE33_GATE_SCRIPT", scene_path)
