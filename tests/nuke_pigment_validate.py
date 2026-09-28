"""Nuke discovery and visual validation for the integrated Pigment node."""

import os
import sys
import time

import nuke


all_types = nuke.nodeTypes(force_plugin_load=True)
print("PAINTERLY_NODE_TYPES", [node_type for node_type in all_types
      if any(token in node_type.lower() for token in ("painterly", "pigment", "detailcollapse", "chromadiffusion"))])
matches = [node_type for node_type in all_types
           if node_type.lower() == "pigment" or "org.painterlyofx.pigment_v1" in node_type.lower()]
print("PIGMENT_OFX_PATHS", nuke.ofxPluginPath())
print("INTEGRATED_PIGMENT_NODE_TYPES", matches)
if not matches:
    raise RuntimeError("org.painterlyofx.Pigment was not discovered")

node = nuke.createNode(matches[0], inpanel=False)
required = {
    "amount", "massScale", "massStrength", "toneSimilarity", "chromaSimilarity",
    "lumaAttraction", "chromaAttraction", "structureScale", "structurePreserve",
    "boundaryPreserve", "boundaryExtinction", "boundarySoftness", "veilAmount",
    "veilScale", "veilIrregularity", "veilContrast", "veilSeed", "detailCleanup",
    "fineDetail", "mediumDetail", "internalVariation", "chromaMigration",
    "chromaScale", "chromaEdgeRespect", "regionSoftness", "boundaryScale",
    "modeSelectivity", "veilTonalBias", "chromaLumaCoupling", "workingGamut", "comparisonMode",
    "debugView", "mix",
}
missing = required.difference(node.knobs())
if missing:
    raise RuntimeError("Missing integrated Pigment controls: " + ", ".join(sorted(missing)))
print("INTEGRATED_PIGMENT_DISCOVERY_OK", node.Class())

root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
output_dir = os.environ.get(
    "PIGMENT_NUKE_OUTPUT", os.path.join(root, "tests", "visual", "renders", "pigment")
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
    print("INTEGRATED_PIGMENT_RENDER", filename, "seconds", round(elapsed, 3))
    return elapsed


plates = {
    "fruit-grapes": "fruit-grapes.png",
    "laundry-cloth": "laundry-cloth.png",
    "low-light-chroma": "low-light-chroma.png",
    "cgi-specular": "cgi-specular.png",
}
timings = []
for knob, value in {
    "amount": 0.9, "massScale": 30.0, "massStrength": 0.78,
    "toneSimilarity": 0.42, "chromaSimilarity": 0.30,
    "lumaAttraction": 0.82, "chromaAttraction": 0.95,
    "boundaryPreserve": 0.58, "boundaryExtinction": 0.48,
    "modeSelectivity": 0.88, "detailCleanup": 0.0,
    "fineDetail": 0.02, "mediumDetail": 0.08, "internalVariation": 0.22,
}.items():
    node[knob].setValue(value)
for label, filename in plates.items():
    read = nuke.nodes.Read(file=os.path.join(root, "tests", "visual", "inputs", filename))
    reformat = nuke.nodes.Reformat(inputs=[read])
    reformat["type"].setValue("to box")
    reformat["box_width"].setValue(512)
    reformat["box_height"].setValue(512)
    reformat["resize"].setValue("fit")
    node.setInput(0, reformat)
    for comparison, suffix in ((0, "original"), (1, "guided"),
                               (2, "weighted-mean"), (3, "representative-mode")):
        node["comparisonMode"].setValue(comparison)
        node["debugView"].setValue(0)
        timings.append(render_png(node, label + "-" + suffix + ".png"))
    if label in ("fruit-grapes", "laundry-cloth"):
        node["comparisonMode"].setValue(3)
        for value, suffix in {
            1: "coarse-structure", 2: "veil", 3: "mass-field",
            4: "boundary-extinction-field", 6: "detail-retention-field",
            7: "region-mode", 8: "attraction", 11: "pre-boundary-mass",
            12: "boundary-protection", 14: "chroma-migration",
            18: "pre-reintegration", 19: "difference", 20: "local-density",
            21: "dominant-mode", 22: "mode-confidence",
            23: "representative-distance", 24: "candidate-competition",
            25: "legacy-debug-result", 26: "representative-debug-result",
        }.items():
            node["debugView"].setValue(value)
            timings.append(render_png(node, label + "-" + suffix + ".png"))
    nuke.delete(reformat)
    nuke.delete(read)

node["comparisonMode"].setValue(3)
node["debugView"].setValue(0)
validation_read = nuke.nodes.Read(
    file=os.path.join(root, "tests", "visual", "inputs", "laundry-cloth.png")
)
validation_reformat = nuke.nodes.Reformat(inputs=[validation_read])
validation_reformat["type"].setValue("to box")
validation_reformat["box_width"].setValue(1920)
validation_reformat["box_height"].setValue(1080)
validation_reformat["resize"].setValue("fit")
node.setInput(0, validation_reformat)
viewer = nuke.nodes.Viewer(inputs=[node])
validation_read.setXYpos(200, -20)
validation_reformat.setXYpos(200, 80)
node.setXYpos(200, 180)
viewer.setXYpos(200, 280)
script_path = os.path.join(root, "tests", "visual", "PigmentValidation.nk")
nuke.scriptSaveAs(script_path, overwrite=1)
print("INTEGRATED_PIGMENT_SCRIPT", script_path)
print("INTEGRATED_PIGMENT_RENDER_OK", output_dir)
print("INTEGRATED_PIGMENT_TIMING_MEAN", round(sum(timings) / len(timings), 3))
sys.exit(0)
