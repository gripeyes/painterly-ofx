"""Render/profile actual artist knobs and test the unified state editor."""
import json, os, time
import nuke
ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),".."))
SIZE=int(os.environ.get("PIGMENT_ARTIST_SIZE","512"))
HEIGHT=1080 if SIZE==1920 else SIZE
EXECUTION=int(os.environ.get("PIGMENT_ARTIST_EXECUTION","1"))
OUT=os.environ.get("PIGMENT_ARTIST_OUT",os.path.join(ROOT,"build","phase4-artist-%d-%s"%(SIZE,"guided" if EXECUTION else "full")))
os.makedirs(OUT,exist_ok=True)
kind=next(t for t in nuke.nodeTypes(force_plugin_load=True) if t.lower()=="pigment" or "org.painterlyofx.pigment_v1" in t.lower())
FIXTURE=os.environ.get("PIGMENT_ARTIST_FIXTURE","fashion")
read=nuke.nodes.Read(file=os.path.join(ROOT,"tests/visual/renders/phase4/comparative-pipeline",FIXTURE,"source.png"))
fmt=nuke.addFormat("%d %d 1 ArtistPerformance"%(SIZE,HEIGHT))
image=nuke.nodes.Reformat(inputs=[read],type="to format",format=fmt.name(),resize="distort")
node=nuke.createNode(kind,inpanel=False);node.setInput(0,image)
assert node["pigmentInterface"].value()=="Pigment",node["pigmentInterface"].value()
assert node["phase4ExecutionClass"].value()=="Interactive / Guided"
node["phase4ExecutionClass"].setValue(EXECUTION)
node["phase4PreviewQuality"].setValue(int(os.environ.get("PIGMENT_PREVIEW_QUALITY","1")))
node["phase4ComputeBackend"].setValue(int(os.environ.get("PIGMENT_ARTIST_BACKEND","0")))
assert node["pigmentPlate"].values()==["All","A","B","C","D","E","F"]
write=nuke.nodes.Write(inputs=[node],file_type="exr",channels="rgba");write["datatype"].setValue("32 bit float")
records=[]
def render(label):
    write["file"].setValue(os.path.join(OUT,label+".exr"))
    print("ARTIST_BEGIN",label,flush=True)
    start=time.perf_counter();nuke.execute(write,1,1)
    records.append({"control":label,"seconds":time.perf_counter()-start})
    print("ARTIST_END",label,records[-1]["seconds"],flush=True)
render("cold");render("unchanged")
if not os.environ.get("PIGMENT_COMPARE_ONLY"):
    # Scripted Nuke setValue does not invoke OFX InstanceChanged; the UI proxy
    # is exercised separately by real GUI events and its saved backing state.
    # Python automation must address the preserved animated plate parameters.
    node["phase4PlateBTone"].setValue(.012);render("edit-B-backing")
    assert abs(node["phase4PlateBTone"].value()-.012)<1e-9,node["phase4PlateBTone"].value()
    assert node["phase4PlateATone"].value()==0
    for p in "ABCDEF":node["phase4Plate%sSpillOut"%p].setValue(.8)
    render("edit-All-backing")
    assert all(abs(node["phase4Plate%sSpillOut"%p].value()-.8)<1e-9 for p in "ABCDEF")
    assert node["phase4PlateGSpillOut"].value()==1
    for knob,value in [("pigmentPictorialScale",56),("pigmentStructureLock",.8),("pigmentLumaOrganization",.6),("pigmentChromaOrganization",.7),("pigmentLumaComplexity",.6),("pigmentChromaComplexity",.3),("pigmentChromaSpread",.45),("pigmentSpill",.4),("pigmentSpillReach",64),("pigmentDirectionality",.7),("phase4ColorInteraction",1),("phase4PigmentDensity",.5),("amount",.8),("mix",.9),("phase4PlateBTone",.02)]:
        node[knob].setValue(value);render(knob)
    node["pigmentInterface"].setValue(2);node["pigmentInterface"].setValue(1)
    assert node["pigmentPictorialScale"].value()==56
    assert node["phase4PlateBTone"].value()==.02
nuke.scriptSaveAs(os.path.join(OUT,"ArtistControls.nk"),overwrite=1)
with open(os.path.join(OUT,"timings.json"),"w") as f:json.dump(records,f,indent=2)
print("PIGMENT_ARTIST_VALIDATED",flush=True)
