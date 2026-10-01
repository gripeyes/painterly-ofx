"""Photographic CPU/Metal contact sheets and bounded-error summary."""
import argparse
import csv
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

def pfm(path):
    with path.open("rb") as f:
        assert f.readline().strip()==b"PF"
        w,h=map(int,f.readline().split())
        assert float(f.readline())==-1
        return np.flipud(np.frombuffer(f.read(),dtype="<f4").reshape(h,w,3)).copy()

def rgb(yab):
    matrix=np.array([[.6624541811,.1340042065,.1561876870],[.2722287168,.6740817658,.0536895174],[-.0055746495,.0040607335,1.0103391003]])
    white=matrix.sum(axis=1);white/=white[1]
    xyz=np.stack([(yab[...,0]+yab[...,1])*white[0],yab[...,0],(yab[...,0]+yab[...,2])*white[2]],axis=-1)
    return xyz@np.linalg.inv(matrix).T

def tile(values):
    return Image.fromarray(np.uint8(np.clip(values,0,1)*255+.5)).resize((256,256))

parser=argparse.ArgumentParser()
parser.add_argument("input",type=Path)
parser.add_argument("output",type=Path)
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
rows=list(csv.DictReader((args.input/"parity.csv").open()))
summary={"cases":len(rows),"max_YAB_error":max(float(r["max_error"]) for r in rows),"max_YAB_rmse":max(float(r["rmse"]) for r in rows),"transport":"CPU/Metal graph fields asserted bit-exact before appearance comparison; identical alpha/support/topology", "Gate_C":"unaccepted"}
for fixture in ["fashion","knee","lowlight"]:
    sheet=Image.new("RGB",(3*256,6*280),(24,24,24));draw=ImageDraw.Draw(sheet)
    for row,(mode,law) in enumerate([(m,l) for m in ["C0","C1","C4"] for l in [0,1]]):
        directory=args.input/f"{fixture}-{mode}-{law}-128-1"
        cpu=pfm(directory/"cpu-yab.pfm");metal=pfm(directory/"metal-yab.pfm")
        cr,mr=rgb(cpu),rgb(metal)
        for column,(name,data) in enumerate([("CPU",cr),("Metal",mr),("signed RGB difference x10000",.5+10000*(mr-cr))]):
            sheet.paste(tile(data),(column*256,row*280+24))
            draw.text((column*256+5,row*280+4),f"{mode} {'Linear' if law==0 else 'Density'}: {name}",fill="white")
    sheet.save(args.output/(fixture+"-cpu-metal.png"))
for law in ["0","1"]:
    active=[r for r in rows if r["law"]==law and r["spill"]=="1" and r["reach"]=="128"]
    summary["Linear" if law=="0" else "Density"]={key:float(np.median([float(r[key]) for r in active])) for key in ["transport_ms","metal_transport_ms","cpu_interaction_ms","metal_total_ms","gpu_ms","pack_ms","unpack_ms"]}
(args.output/"summary.json").write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
