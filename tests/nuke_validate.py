"""Headless discovery smoke test for a staged Pigment OFX bundle."""
import os
import sys
import nuke

matches = [
    node_type
    for node_type in nuke.nodeTypes(force_plugin_load=True)
    if "painterlyofx" in node_type.lower() or "chromadiffusion" in node_type.lower()
]
print("PIGMENT_OFX_PATHS", nuke.ofxPluginPath())
print("PIGMENT_NODE_TYPES", matches)
if not matches:
    raise RuntimeError("Pigment ChromaDiffusion was not discovered")

node = nuke.createNode(matches[0], inpanel=False)
required = {
    "amount", "radius", "chromaXRadius", "chromaYRadius", "angle",
    "luminancePreservation", "edgeProtection", "edgeSoftness",
    "workingGamut", "mix",
}
missing = required.difference(node.knobs())
if missing:
    raise RuntimeError("Missing Pigment controls: " + ", ".join(sorted(missing)))
print("PIGMENT_DISCOVERY_OK", node.Class())

output_dir = os.environ.get("PIGMENT_NUKE_OUTPUT")
if output_dir:
    os.makedirs(output_dir, exist_ok=True)
    fmt = nuke.addFormat("37 23 0 0 37 23 1 PigmentValidation")
    checker = nuke.nodes.CheckerBoard2(format=fmt.name())
    checker["color1"].setValue((1.5, 0.05, -0.1, 0.25))
    checker["color2"].setValue((0.05, 0.8, 3.0, 0.75))
    checker["boxsize"].setValue((5, 7))
    node.setInput(0, checker)

    def render(source, filename):
        writer = nuke.nodes.Write(inputs=[source])
        writer["file"].setValue(os.path.join(output_dir, filename))
        writer["file_type"].setValue("exr")
        writer["channels"].setValue("rgba")
        if "datatype" in writer.knobs():
            writer["datatype"].setValue("32 bit float")
        nuke.execute(writer, 1, 1)
        nuke.delete(writer)

    render(checker, "source.exr")
    node["amount"].setValue(0.0)
    render(node, "identity.exr")
    node["amount"].setValue(1.0)
    node["radius"].setValue(2.0)
    node["edgeProtection"].setValue(0.0)
    render(node, "processed.exr")

    rgba = nuke.nodes.Constant(format=fmt.name())
    rgba["color"].setValue((1.25, -0.1, 2.0, 0.375))
    node.setInput(0, rgba)
    render(rgba, "source_rgba.exr")
    node["amount"].setValue(0.0)
    render(node, "identity_rgba.exr")
    node["amount"].setValue(1.0)
    render(node, "processed_rgba.exr")
    print("PIGMENT_RENDER_OK", output_dir)
sys.exit(0)
