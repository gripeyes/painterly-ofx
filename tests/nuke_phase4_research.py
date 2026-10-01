"""CPU research-node host validation and reusable hands-on scene. Not Gate C certification."""
import json
import os
import time
import nuke

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.environ.get("PIGMENT_RESEARCH_OUTPUT", os.path.join(ROOT, "build", "phase4-nuke-research"))
os.makedirs(OUT, exist_ok=True)
types = nuke.nodeTypes(force_plugin_load=True)
kind = next(t for t in types if t.lower() == "pigment" or "org.painterlyofx.pigment_v1" in t.lower())
source = nuke.nodes.Read(file=os.path.join(ROOT, "tests/visual/renders/phase4/comparative-pipeline/fashion/source.png"))
small = nuke.nodes.Reformat(inputs=[source], type="to box", box_width=128, box_height=128, resize="fit")
small["label"].setValue("CPU research size: raise for hands-on evaluation")
records = []
nodes = []
for value, label in [(1,"C0 A3 Passthrough"),(0,"C1 Bounded Poisson"),(2,"C3 Regional Eigen"),(3,"C4 Sparse Curve")]:
    node = nuke.createNode(kind, inpanel=False)
    node.setInput(0, small)
    required = {"phase4Representation", "phase4YSupport", "phase4ABSupport", "phase4SpillReach", "phase4StructureRespect"}
    assert required.issubset(node.knobs()), required.difference(node.knobs())
    # Existing-node default comparison is not changed by descriptor additions.
    assert node["comparisonMode"].value() != "Automatic Plate Graph (Phase 4)"
    node["comparisonMode"].setValue(6)
    node["phase4Representation"].setValue(value)
    node["phase4SpillAmount"].setValue(.8)
    node["phase4LumaSpill"].setValue(0)
    node["phase4ChromaSpill"].setValue(1)
    node["phase4SpillReach"].setValue(128)
    node["label"].setValue(label + "\nResearch only — Gate C not accepted")
    nodes.append(node)
    write = nuke.nodes.Write(inputs=[node], file=os.path.join(OUT, "representation-%d.exr" % value), file_type="exr", channels="rgba")
    start = time.perf_counter()
    nuke.execute(write, 1, 1)
    records.append({"representation":label,"seconds":time.perf_counter()-start})
    nuke.delete(write)

debug = nodes[0]["debugView"]
labels = debug.values()
required_debug = ["Phase 4 Source", "Phase 4 Public Reconstruction", "Phase 4 Artist Plate Alpha", "Phase 4 Artist Plate Y Support", "Phase 4 Artist Plate AB Support", "Phase 4 Y Chunks", "Phase 4 AB Chunks", "Phase 4 Retained Boundaries", "Phase 4 Pre-Spill Result", "Phase 4 Post-Spill Result", "Phase 4 Spill Difference x16", "Phase 4 Y Influence", "Phase 4 Y Transport", "Phase 4 AB Transport"]
for i, label in enumerate(required_debug):
    assert label in labels, label
    debug.setValue(label)
    write = nuke.nodes.Write(inputs=[nodes[0]], file=os.path.join(OUT, "debug-%02d.exr" % i), file_type="exr", channels="rgba")
    nuke.execute(write, 1, 1)
    nuke.delete(write)
debug.setValue(0)
viewer = nuke.nodes.Viewer(inputs=nodes)
viewer["label"].setValue("C0 / C1 / C3 / C4 — identical source, independent research nodes")
nuke.scriptSaveAs(os.path.join(OUT, "PigmentPhase4Research.nk"), overwrite=1)
with open(os.path.join(OUT, "host-validation.json"), "w") as f:
    json.dump({"node":kind,"comparisons":records,"debug_views":required_debug,"gate_C":"not accepted"}, f, indent=2)
print("PHASE4_RESEARCH_VALIDATED", OUT)
