"""Host cold/warm stage profiling; run with PIGMENT_PHASE4_PROFILE=1."""
import nuke
import os
import time
import json
ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),".."))
SIZE=int(os.environ.get("PIGMENT_PROFILE_SIZE","256"))
OUT=os.path.join(ROOT,"build","phase4-host-performance"+("" if SIZE==256 else "-%d"%SIZE))
os.makedirs(OUT,exist_ok=True)
kind=next(t for t in nuke.nodeTypes(force_plugin_load=True) if t.lower()=="pigment" or "org.painterlyofx.pigment_v1" in t.lower())
records=[]
for fixture in os.environ.get("PIGMENT_PROFILE_FIXTURES","fashion,knee,lowlight").split(","):
    read=nuke.nodes.Read(file=os.path.join(ROOT,"tests/visual/renders/phase4/comparative-pipeline",fixture,"source.png"))
    size=SIZE
    small=nuke.nodes.Reformat(inputs=[read],type="to box",box_width=size,box_height=size,resize="fit")
    node=nuke.createNode(kind,inpanel=False)
    node.setInput(0,small)
    node["pigmentInterface"].setValue(2)
    node["comparisonMode"].setValue(6)
    node["amount"].setValue(1)
    node["phase4SpillAmount"].setValue(.8)
    node["phase4LumaSpill"].setValue(.05)
    node["phase4ChromaSpill"].setValue(1)
    node["phase4PigmentDensity"].setValue(.5)
    node["phase4SpillReach"].setValue(128)
    write=nuke.nodes.Write(inputs=[node],file_type="exr",channels="rgba")
    write["datatype"].setValue("32 bit float")
    for mode in [1,0,3]:
        representation={1:"C0",0:"C1",3:"C4"}[mode]
        node["phase4Representation"].setValue(mode)
        for law in [0,1]:
            node["phase4ColorInteraction"].setValue(law)
            for backend in [1,0]:
                node["phase4ComputeBackend"].setValue(backend)
                for iteration in range(3):
                    # Actual creative edit forces a fresh OFX render, not
                    # merely Write of Nuke's existing image-cache entry.
                    node["phase4PlateATone"].setValue(iteration*.001)
                    label="%s-%s-law%d-backend%d-run%d"%(fixture,representation,law,backend,iteration)
                    write["file"].setValue(os.path.join(OUT,label+".exr"))
                    start=time.perf_counter();nuke.execute(write,1,1)
                    records.append({"case":label,"representation_choice":mode,"representation":representation,"seconds":time.perf_counter()-start,"size":size})
    # Plate-scale edit must reuse the source-only validated A1/A2/A3 cache.
    node["phase4PlateScale"].setValue(64)
    write["file"].setValue(os.path.join(OUT,fixture+"-scale-edit.exr"))
    start=time.perf_counter();nuke.execute(write,1,1)
    records.append({"case":fixture+"-scale-edit","seconds":time.perf_counter()-start,"size":size})
with open(os.path.join(OUT,"performance.json"),"w") as f:
    json.dump(records,f,indent=2)
print("PHASE4_PERFORMANCE_COMPLETE",OUT)
