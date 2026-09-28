"""Nuke discovery and visual-render validation for DetailCollapse research."""

import os
import sys
import time

import nuke


matches = [
    node_type
    for node_type in nuke.nodeTypes(force_plugin_load=True)
    if "detailcollapse" in node_type.lower()
]
print("PIGMENT_OFX_PATHS", nuke.ofxPluginPath())
print("DETAIL_COLLAPSE_NODE_TYPES", matches)
if not matches:
    raise RuntimeError("Pigment DetailCollapse was not discovered")

node = nuke.createNode(matches[0], inpanel=False)
required = {
    "backend", "amount", "massScale", "structureScale", "massStrength",
    "toneSimilarity", "chromaSimilarity", "boundaryPreserve",
    "boundarySoftness", "structurePreserve", "internalVariation",
    "lumaMassing", "chromaMassing", "rangeEnabled", "rangeMinimum",
    "rangeMaximum", "rangeSoftness", "workingGamut", "mix", "debugView",
}
missing = required.difference(node.knobs())
if missing:
    raise RuntimeError("Missing DetailCollapse controls: " + ", ".join(sorted(missing)))
print("DETAIL_COLLAPSE_DISCOVERY_OK", node.Class())

node["backend"].setValue(3)  # Guided Metal; CPU images still use explicit Metal staging.
node["amount"].setValue(0.65)
node["massScale"].setValue(2.5)
node["structureScale"].setValue(5.0)
node["massStrength"].setValue(0.55)
node["toneSimilarity"].setValue(0.12)
node["chromaSimilarity"].setValue(0.18)
node["boundaryPreserve"].setValue(0.95)
node["boundarySoftness"].setValue(0.05)
node["structurePreserve"].setValue(1.0)
node["internalVariation"].setValue(0.4)
node["lumaMassing"].setValue(0.3)
node["chromaMassing"].setValue(1.0)

root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
output_dir = os.environ.get(
    "PIGMENT_NUKE_OUTPUT", os.path.join(root, "tests", "visual", "renders")
)
os.makedirs(output_dir, exist_ok=True)


def render_png(source, filename):
    writer = nuke.nodes.Write(inputs=[source])
    writer["file"].setValue(os.path.join(output_dir, filename))
    writer["file_type"].setValue("png")
    writer["channels"].setValue("rgba")
    start = time.perf_counter()
    nuke.execute(writer, 1, 1)
    elapsed = time.perf_counter() - start
    nuke.delete(writer)
    print("DETAIL_COLLAPSE_RENDER", filename, "seconds", round(elapsed, 3))
    return elapsed


plates = {
    "cgi-specular": "cgi-specular.png",
    "low-light-chroma": "low-light-chroma.png",
    "skin-fabric": "skin-fabric.png",
    "broad-color-fields": "broad-color-fields.png",
}
timings = []
for label, filename in plates.items():
    read = nuke.nodes.Read(file=os.path.join(root, "tests", "visual", "inputs", filename))
    reformat = nuke.nodes.Reformat(inputs=[read])
    reformat["type"].setValue("to box")
    reformat["box_width"].setValue(256)
    reformat["box_height"].setValue(256)
    reformat["resize"].setValue("fit")
    node.setInput(0, reformat)
    node["debugView"].setValue(0)
    timings.append(render_png(node, label + "-final.png"))
    if label == "cgi-specular":
        debug_views = {
            2: "consolidation-seed",
            3: "structure-guide",
            4: "processing-strength",
            5: "boundary-protection",
            6: "rolling-iteration-1",
            9: "rolling-iteration-4",
            10: "y-mass-result",
            11: "ab-mass-result",
            12: "internal-variation-residual",
            13: "pre-reintegration-mass",
            14: "difference-from-original",
        }
        for value, suffix in debug_views.items():
            node["debugView"].setValue(value)
            timings.append(render_png(node, "cgi-specular-" + suffix + ".png"))
    nuke.delete(reformat)
    nuke.delete(read)

node["debugView"].setValue(0)
validation_read = nuke.nodes.Read(
    file=os.path.join(root, "tests", "visual", "inputs", "cgi-specular.png")
)
validation_reformat = nuke.nodes.Reformat(inputs=[validation_read])
validation_reformat["type"].setValue("to box")
validation_reformat["box_width"].setValue(256)
validation_reformat["box_height"].setValue(256)
validation_reformat["resize"].setValue("fit")
node.setInput(0, validation_reformat)
viewer = nuke.nodes.Viewer(inputs=[node])
node.setXYpos(200, 180)
validation_reformat.setXYpos(200, 80)
validation_read.setXYpos(200, -20)
viewer.setXYpos(200, 280)
script_path = os.path.join(root, "tests", "visual", "DetailCollapseValidation.nk")
if os.path.exists(script_path):
    os.unlink(script_path)
nuke.scriptSaveAs(script_path, overwrite=1)
print("DETAIL_COLLAPSE_SCRIPT", script_path)
print("DETAIL_COLLAPSE_RENDER_OK", output_dir)
print("DETAIL_COLLAPSE_TIMING_MEAN", round(sum(timings) / len(timings), 3))
sys.exit(0)
