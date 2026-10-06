"""1080p Guided quality evidence. Never runs full-frame Full Reference."""
import json
import os
import time
import nuke

ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),".."))
OUT=os.path.join(ROOT,"build","phase4-preview-quality")
os.makedirs(OUT,exist_ok=True)
kind=next(t for t in nuke.nodeTypes(force_plugin_load=True) if t.lower()=="pigment" or "org.painterlyofx.pigment_v1" in t.lower())
fmt=nuke.addFormat("1920 1080 1 GuidedQuality1080")
timingFile=os.path.join(OUT,"timings.json")
records=json.load(open(timingFile)) if os.path.exists(timingFile) else []
for fixture in ["fashion","knee","lowlight","thin-structures"]:
    if fixture=="thin-structures":
        read=nuke.nodes.Constant(format=fmt.name())
        texture=nuke.nodes.Expression(inputs=[read],expr0="x < 960 ? .05 : .8",expr1="abs(x-500)<1 || abs(y-540)<1 ? 1 : .15",expr2="abs(x-1200)<2 ? .9 : .1",expr3="1")
    else:
        read=nuke.nodes.Read(file=os.path.join(ROOT,"tests/visual/renders/phase4/comparative-pipeline",fixture,"source.png"))
        texture=read
    image=nuke.nodes.Reformat(inputs=[texture],type="to format",format=fmt.name(),resize="distort")
    sourceWrite=nuke.nodes.Write(inputs=[image],file=os.path.join(OUT,fixture+"-Source.exr"),file_type="exr",channels="rgba")
    sourceWrite["datatype"].setValue("32 bit float")
    if not os.path.exists(sourceWrite["file"].value()):nuke.execute(sourceWrite,1,1)
    nuke.delete(sourceWrite)
    node=nuke.createNode(kind,inpanel=False);node.setInput(0,image)
    assert node["pigmentInterface"].value()=="Pigment"
    assert node["phase4ExecutionClass"].value()=="Interactive / Guided"
    write=nuke.nodes.Write(inputs=[node],file_type="exr",channels="rgba");write["datatype"].setValue("32 bit float")
    for quality,label in enumerate(["Fast","Balanced","Detailed"]):
        node["phase4PreviewQuality"].setValue(quality)
        path=os.path.join(OUT,fixture+"-"+label+".exr");write["file"].setValue(path)
        if os.path.exists(path):continue
        start=time.perf_counter();nuke.execute(write,1,1)
        records.append({"fixture":fixture,"quality":label,"seconds_including_write":time.perf_counter()-start})
    node["phase4PreviewQuality"].setValue(1)
    for label,knob,value in [("PreSpill","pigmentSpill",0),("StrongSpill","pigmentSpill",1),
                             ("ScaleSmall","pigmentPictorialScale",24),("ScaleLarge","pigmentPictorialScale",96)]:
        node["pigmentSpill"].setValue(.25);node["pigmentPictorialScale"].setValue(48)
        node[knob].setValue(value)
        path=os.path.join(OUT,fixture+"-"+label+".exr");write["file"].setValue(path)
        if not os.path.exists(path):nuke.execute(write,1,1)
    nuke.delete(write);nuke.delete(node);nuke.delete(image)
    if texture!=read:nuke.delete(texture)
    nuke.delete(read)
with open(os.path.join(OUT,"timings.json"),"w") as f:json.dump(records,f,indent=2)
print("GUIDED_1080_QUALITY_COMPLETE",flush=True)
