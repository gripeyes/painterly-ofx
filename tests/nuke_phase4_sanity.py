"""One-frame Phase 4 host sanity check for major CPU-gate milestones."""

import os
import nuke


matches = [
    node_type
    for node_type in nuke.nodeTypes(force_plugin_load=True)
    if node_type.lower() == "pigment"
    or "org.painterlyofx.pigment_v1" in node_type.lower()
]
if not matches:
    raise RuntimeError("org.painterlyofx.Pigment was not discovered")

sanity_format = nuke.addFormat("64 64 0 0 64 64 1 Phase4Sanity")
source = nuke.nodes.Constant()
source["format"].setValue(sanity_format.name())
source["color"].setValue((0.18, 0.32, 0.62, 1.0))
node = nuke.createNode(matches[0], inpanel=False)
node.setInput(0, source)

required = {
    "comparisonMode",
    "phase4LatentCount",
    "phase4PlateCount",
    "phase4LumaChunkScale",
    "phase4ChromaChunkScale",
    "phase4SpillAmount",
}
missing = required.difference(node.knobs())
if missing:
    raise RuntimeError("Missing Phase 4 controls: " + ", ".join(sorted(missing)))

node["comparisonMode"].setValue(6)
node["phase4LatentCount"].setValue(12)
node["phase4PlateCount"].setValue(4)
node["phase4LumaChunkScale"].setValue(0.0)
node["phase4ChromaChunkScale"].setValue(0.0)
node["phase4SpillAmount"].setValue(0.0)

output = os.environ.get("PIGMENT_PHASE4_SANITY", "/tmp/pigment-phase4-sanity.exr")
write = nuke.nodes.Write(inputs=[node])
write["file"].setValue(output)
write["file_type"].setValue("exr")
nuke.execute(write, 1, 1)
if not os.path.exists(output) or os.path.getsize(output) == 0:
    raise RuntimeError("Phase 4 sanity render was not written")

print("PHASE4_SANITY_NODE", matches[0])
print("PHASE4_SANITY_OUTPUT", output)
