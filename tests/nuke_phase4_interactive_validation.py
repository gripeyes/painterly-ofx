"""Actual-host Phase 4 endpoint/layout/backend validation, not Gate-C acceptance."""
import json
import os
import time
import math
import nuke

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.environ.get("PIGMENT_HOST_OUTPUT", os.path.join(ROOT, "build", "phase4-host-metal"))
os.makedirs(OUT, exist_ok=True)
kind = next(t for t in nuke.nodeTypes(force_plugin_load=True) if t.lower() == "pigment" or "org.painterlyofx.pigment_v1" in t.lower())
records = []
channels = ("red", "green", "blue", "alpha")

def render(node, name, frame=1):
    path = os.path.join(OUT, name + ".exr")
    write = nuke.nodes.Write(inputs=[node], file=path, file_type="exr", channels="rgba")
    write["datatype"].setValue("32 bit float")
    start = time.perf_counter()
    nuke.execute(write, frame, frame)
    records.append({"name": name, "frame": frame, "seconds": time.perf_counter()-start})
    nuke.delete(write)
    return nuke.nodes.Read(file=path)

def difference(a, b, width, height, exact=False):
    error = 0.0
    for y in range(0, height, max(1,height//16)):
        for x in range(0, width, max(1,width//16)):
            for c in channels:
                av, bv = nuke.sample(a,c,x+.5,y+.5), nuke.sample(b,c,x+.5,y+.5)
                assert math.isfinite(av) and math.isfinite(bv)
                if c == "alpha" or exact:
                    assert av == bv, (c,x,y,av,bv)
                error = max(error, abs(av-bv))
    assert error < 5e-5, error
    return error

for width,height in [(31,47),(137,89)]:
    fmt = nuke.addFormat("%d %d 1 Phase4_%dx%d" % (width,height,width,height))
    constant = nuke.nodes.Constant(format=fmt.name())
    expression = nuke.nodes.Expression(inputs=[constant], expr0="-0.25+2.5*x/width+0.01*frame", expr1="0.03+1.7*y/height", expr2="0.5-0.8*x/width", expr3="x<width/4?0:0.2+0.8*y/height")
    node = nuke.createNode(kind,inpanel=False)
    node.setInput(0,expression)
    node["pigmentInterface"].setValue(2)
    node["comparisonMode"].setValue(6)
    node["phase4Representation"].setValue(1)
    node["amount"].setValue(1)
    node["mix"].setValue(1)
    node["phase4SpillAmount"].setValue(1)
    node["phase4SpillReach"].setValue(128)
    node["phase4LumaSpill"].setValue(.1)
    node["phase4ChromaSpill"].setValue(1)
    original = render(expression,"source-%dx%d" % (width,height))
    for law in [0,1,2]:
        node["phase4ColorInteraction"].setValue(law)
        node["phase4PigmentDensity"].setValue(.7)
        node["phase4ComputeBackend"].setValue(1)
        cpu = render(node,"cpu-%dx%d-law%d" % (width,height,law))
        node["phase4ComputeBackend"].setValue(2)
        metal = render(node,"metal-%dx%d-law%d" % (width,height,law))
        records.append({"law":law,"size":[width,height],"sample_max_error":difference(cpu,metal,width,height),"spectral_CPU_only":law==2})
    # Final node contracts remain CPU reference finalization in the hybrid.
    for knob in ["amount","mix"]:
        node[knob].setValue(0)
        bypass=render(node,"%s-zero-%dx%d" % (knob,width,height))
        difference(original,bypass,width,height,exact=True)
        node[knob].setValue(1)
    node["phase4ColorInteraction"].setValue(0)
    node["debugView"].setValue("Phase 4 Source")
    difference(original,render(node,"source-debug-%dx%d" % (width,height)),width,height,exact=True)
    node["debugView"].setValue(0)
    mask=nuke.nodes.Constant(format=fmt.name(),color=[0,0,0,0])
    node.setInput(1,mask)
    difference(original,render(node,"mask-zero-%dx%d"%(width,height)),width,height,exact=True)
    node.setInput(1,None)
    frame2=render(node,"frame2-%dx%d" % (width,height),2)
    node["phase4ComputeBackend"].setValue(1)
    difference(frame2,render(node,"frame2-cpu-%dx%d" % (width,height),2),width,height)
    # Offset bbox via Crop without reformat; coordinates must survive.
    crop=nuke.nodes.Crop(inputs=[expression],box=[-7,-5,width-3,height-4],reformat=False)
    node.setInput(0,crop)
    node["phase4ComputeBackend"].setValue(1)
    cpu=render(node,"origin-cpu-%dx%d" % (width,height))
    node["phase4ComputeBackend"].setValue(2)
    difference(cpu,render(node,"origin-metal-%dx%d" % (width,height)),width-3,height-4)
    node.setInput(0,expression)
    for mode in [1,0,2,3,4]:
        node["phase4Representation"].setValue(mode)
        node["phase4LumaChunkScale"].setValue(0)
        node["phase4ChromaChunkScale"].setValue(0)
        render(node,"zero-cut-%dx%d-C%d" % (width,height,mode))
    # Round-trip saved host knob state and node identity.
    for item in nuke.allNodes():
        item.setSelected(False)
    node.setSelected(True)
    path=os.path.join(OUT,"persist-node-%dx%d.nk"%(width,height))
    nuke.nodeCopy(path)
    nuke.nodePaste(path)
    copied=nuke.selectedNode()
    copied.setInput(0,expression)
    assert copied["phase4ComputeBackend"].value()==node["phase4ComputeBackend"].value()
    difference(render(node,"persist-before-%dx%d"%(width,height)),render(copied,"persist-after-%dx%d"%(width,height)),width,height,exact=True)

nuke.scriptSaveAs(os.path.join(OUT,"HostValidation.nk"),overwrite=1)
with open(os.path.join(OUT,"host-results.json"),"w") as f:
    json.dump({"records":records,"Gate_C":"unaccepted","sampling":"16x16 grid plus exact alpha/endpoint sample equality; not exhaustive pixel certification"},f,indent=2)
print("PHASE4_HOST_METAL_VALIDATED",OUT)
