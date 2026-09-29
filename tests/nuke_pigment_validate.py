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
    "debugView", "mix", "debugPlane", "fineExtinction", "mediumExtinction",
    "broadRetention", "detailStructurePreserve", "yTransitionWidth",
    "abTransitionWidth", "transitionStructureRespect", "localSoftness",
    "phase33PlaneSource", "phase33AutomaticOccupancy",
    "phase33AutomaticPlaneStrength", "phase33SpatialCoherence",
    "phase33ColorCoherence", "phase33HybridGuidance", "phase33ShadingModel",
    "phase33SmoothSimplification", "phase33ShadingStructurePreserve",
    "phase33ChromaShadingRetention", "phase33StructurePreserve",
    "phase33TransitionSolver", "phase33ComputeBackend",
    "planeAEnable", "planeAAmount", "planeASourceMix", "planeAManualTarget",
    "planeAToneInfluence", "planeAChromaInfluence",
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


def make_plane_map(source, width, height):
    """Create four editable, overlapping ownership regions in raw RGBA channels."""
    plane_map = nuke.nodes.Expression(inputs=[source])
    plane_map["label"].setValue("EDITABLE RGBA PLANE MAP\nR=A G=B B=C A=D")
    plane_map["expr0"].setValue("(x < %g) * (y < %g)" % (width * 0.53, height * 0.62))
    plane_map["expr1"].setValue("(x >= %g) * (y < %g)" % (width * 0.41, height * 0.68))
    plane_map["expr2"].setValue("(x < %g) * (y >= %g)" % (width * 0.66, height * 0.48))
    plane_map["expr3"].setValue("(x >= %g) * (y >= %g)" % (width * 0.55, height * 0.44))
    return plane_map


plates = {
    "fruit-grapes": "fruit-grapes.png",
    "laundry-cloth": "laundry-cloth.png",
    "low-light-chroma": "low-light-chroma.png",
    "cgi-specular": "cgi-specular.png",
    "skin-fabric": "skin-fabric.png",
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
for knob, value in {
    "phase33PlaneSource": 1,
    "phase33AutomaticOccupancy": 1.0,
    "phase33AutomaticPlaneStrength": 1.0,
    "phase33SpatialCoherence": 0.5,
    "phase33ColorCoherence": 0.5,
    "phase33HybridGuidance": 0.75,
    "phase33ShadingModel": 1,
    "phase33SmoothSimplification": 0.55,
    "phase33ShadingStructurePreserve": 0.8,
    "phase33ChromaShadingRetention": 0.65,
    "phase33StructurePreserve": 0.9,
    "phase33TransitionSolver": 1,
    "phase33ComputeBackend": 2,
}.items():
    node[knob].setValue(value)
for knob, value in {
    "fineExtinction": 0.9,
    "mediumExtinction": 0.8,
    "broadRetention": 0.2,
    "detailStructurePreserve": 0.7,
    "yTransitionWidth": 12.0,
    "abTransitionWidth": 48.0,
    "transitionStructureRespect": 0.7,
    "veilAmount": 0.0,
    "localSoftness": 0.0,
}.items():
    node[knob].setValue(value)

plane_input = node.maxInputs() - 1
print("PIGMENT_INPUT_COUNT", node.maxInputs(), "PLANE_MAP_INPUT", plane_input)
for label, filename in plates.items():
    read = nuke.nodes.Read(file=os.path.join(root, "tests", "visual", "inputs", filename))
    reformat = nuke.nodes.Reformat(inputs=[read])
    reformat["type"].setValue("to box")
    reformat["box_width"].setValue(512)
    reformat["box_height"].setValue(512)
    reformat["resize"].setValue("fit")
    plane_map = make_plane_map(reformat, 512, 512)
    node.setInput(0, reformat)
    node.setInput(plane_input, plane_map)
    for comparison, suffix in ((0, "original"), (1, "guided"),
                               (2, "weighted-mean"), (3, "representative-mode"),
                               (4, "pictorial-planes"), (5, "soft-pictorial-plates")):
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
    if label in ("skin-fabric", "low-light-chroma"):
        node["comparisonMode"].setValue(4)
        for value, suffix in {
            27: "plane-map", 28: "plane-memberships", 29: "base-membership",
            30: "y-transition", 31: "ab-transition", 32: "plane-y-target",
            33: "plane-ab-target", 34: "combined-y-target",
            35: "combined-ab-target", 36: "broad-component",
            37: "fine-residual", 38: "medium-residual", 39: "extinction",
            40: "structure-protection", 41: "pre-veil", 42: "pre-softness",
            43: "fit-error", 44: "plane-difference",
        }.items():
            node["debugView"].setValue(value)
            timings.append(render_png(node, label + "-" + suffix + ".png"))
        node["comparisonMode"].setValue(5)
        for value, suffix in {
            45: "auto-raw-membership", 46: "auto-membership",
            47: "auto-base", 48: "auto-palette", 50: "auto-confidence",
            51: "auto-reconstruction-error", 56: "photographic-structure",
            57: "smooth-shading", 58: "medium-description",
            59: "fine-description", 60: "conditional-y", 61: "conditional-ab",
            62: "phase33-y", 63: "phase33-ab", 64: "phase33-pre-veil",
            65: "phase33-pre-softness", 66: "phase33-difference",
        }.items():
            node["debugView"].setValue(value)
            timings.append(render_png(node, label + "-" + suffix + ".png"))
    node.setInput(plane_input, None)
    nuke.delete(plane_map)
    nuke.delete(reformat)
    nuke.delete(read)

node["comparisonMode"].setValue(4)
node["debugView"].setValue(0)
validation_read = nuke.nodes.Read(
    file=os.path.join(root, "tests", "visual", "inputs", "laundry-cloth.png")
)
validation_reformat = nuke.nodes.Reformat(inputs=[validation_read])
validation_reformat["type"].setValue("to box")
validation_reformat["box_width"].setValue(1920)
validation_reformat["box_height"].setValue(1080)
validation_reformat["resize"].setValue("fit")
validation_plane_map = make_plane_map(validation_reformat, 1920, 1080)
node.setInput(0, validation_reformat)
node.setInput(plane_input, validation_plane_map)
viewer = nuke.nodes.Viewer(inputs=[node])
validation_read.setXYpos(200, -20)
validation_reformat.setXYpos(200, 80)
validation_plane_map.setXYpos(500, 80)
node.setXYpos(300, 180)
viewer.setXYpos(200, 280)
script_path = os.path.join(root, "tests", "visual", "PigmentValidation.nk")
nuke.scriptSaveAs(script_path, overwrite=1)
print("INTEGRATED_PIGMENT_SCRIPT", script_path)
print("INTEGRATED_PIGMENT_RENDER_OK", output_dir)
print("INTEGRATED_PIGMENT_TIMING_MEAN", round(sum(timings) / len(timings), 3))
sys.exit(0)
