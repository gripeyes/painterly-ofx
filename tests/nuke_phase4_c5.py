"""Functional C5 host smoke test; not photographic Gate-C acceptance.

Launch a fresh Nuke process against the signed build with OFX_PLUGIN_PATH.
Do not replace the installed bundle while another Nuke process is using it.
"""
import json
import os
import time
import nuke

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.environ.get("PIGMENT_C5_OUTPUT", os.path.join(ROOT, "build", "phase4-c5-node-smoke"))
os.makedirs(OUT, exist_ok=True)
kind = next(t for t in nuke.nodeTypes(force_plugin_load=True)
            if t.lower() == "pigment" or "org.painterlyofx.pigment_v1" in t.lower())
node = nuke.createNode(kind, inpanel=False)
assert node["pigmentInterface"].value() == "Pigment"
assert node["phase4ExecutionClass"].value() == "Interactive / Guided"
assert node["phase4PreviewQuality"].value() == "Balanced"
choices = node["phase4Representation"].values()
assert len(choices) == 6 and choices[5] == "C5 Layered Broad Fields (Experimental)", choices
source = nuke.nodes.Read(file=os.path.join(ROOT, "tests", "visual", "inputs", "skin-fabric.png"))
full = nuke.nodes.Reformat(inputs=[source], type="to box", box_width=1920, box_height=1080, resize="fit")
node.setInput(0, full)
node["pigmentInterface"].setValue("Research")
node["phase4Representation"].setValue(5)
node["phase4ColorInteraction"].setValue("Linear YAB")
node["phase4SpillAmount"].setValue(0)
node["phase4ComputeBackend"].setValue(1)  # CPU Reference backend, Guided execution.
timings = {}

def render(effect, stem):
    writer = nuke.nodes.Write(inputs=[effect], file=os.path.join(OUT, stem + ".exr"),
                              file_type="exr", channels="rgba")
    writer["datatype"].setValue("32 bit float")
    start = time.monotonic()
    nuke.execute(writer, 1, 1)
    timings[stem] = time.monotonic() - start
    result = nuke.nodes.Read(file=writer["file"].value())
    nuke.delete(writer)
    assert result.width() == 1920 and result.height() == 1080
    return result

original = render(full, "source")
c5 = render(node, "C5-guided")
node["amount"].setValue(0)
bypass = render(node, "C5-amount-zero")
for x, y in [(100, 100), (960, 540), (1500, 900)]:
    for channel in ["red", "green", "blue", "alpha"]:
        assert nuke.sample(original, channel, x, y) == nuke.sample(bypass, channel, x, y)
    assert nuke.sample(original, "alpha", x, y) == nuke.sample(c5, "alpha", x, y)
node["amount"].setValue(1)
debug = node["debugView"]
assert "C5 Broad Target Y" in debug.values()
assert "C5 Sublayer AB Membership" in debug.values()
assert "C5 Combined Broad AB" in debug.values()
debug.setValue("C5 Sublayer Y Membership")
render(node, "C5-layer-Y-membership")
debug.setValue(0)
node["label"].setValue("C5 CPU prototype — not accepted\nLinear YAB / Spill 0 / Guided Balanced")
nuke.scriptSaveAs(os.path.join(OUT, "PigmentC5Research.nk"), overwrite=1)
with open(os.path.join(OUT, "validation.json"), "w") as stream:
    json.dump({"node": kind, "representations": choices, "timings_seconds": timings,
               "resolution": [1920, 1080], "Gate_C": "not accepted"}, stream, indent=2)
print("PIGMENT_C5_HOST_SMOKE_OK", OUT)
