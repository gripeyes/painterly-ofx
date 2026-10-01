"""Presentation/measurement only; consumes completed identical-input CPU comparisons."""
import argparse
import csv
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from phase4_diagnostic_metrics import pfm

LAWS = ["linear-yab", "density", "spectral-pigment"]
MODES = ["C0-A3", "C1-Poisson", "C4-SparseCurve"]

def sheet(paths, labels, target, columns=4, size=320):
    height=min(size,max(round(size*Image.open(p).height/Image.open(p).width) for p in paths))
    canvas = Image.new("RGB", (columns * size, ((len(paths)+columns-1)//columns)*(height+28)), (24,24,24))
    draw = ImageDraw.Draw(canvas)
    for i,(path,label) in enumerate(zip(paths,labels)):
        x,y=(i%columns)*size,(i//columns)*(height+28)
        draw.text((x+5,y+5),label,fill="white")
        image=Image.open(path).convert("RGB");image.thumbnail((size,height))
        canvas.paste(image,(x,y+28))
    canvas.save(target)

def main(root):
    out=root/"color-interaction"
    paths,labels=[],[]
    numerical={}
    matrix=np.array([[.6624541811,.1340042065,.1561876870],
                     [.2722287168,.6740817658,.0536895174],
                     [-.0055746495,.0040607335,1.0103391003]])
    white=matrix.sum(axis=1);white/=white[1]
    def working_rgb(data):
        xyz=np.stack([(data[:,:,0]+data[:,:,1])*white[0],data[:,:,0],
                      (data[:,:,0]+data[:,:,2])*white[2]],axis=-1)
        return xyz@np.linalg.inv(matrix).T
    for mode in MODES:
        d=out/mode
        for stem,label in [("pre-spill", "Pre-Spill")]+[(law,law) for law in LAWS]:
            image=Image.open(d/(stem+".ppm"));image.save(d/(stem+".png"))
            paths.append(d/(stem+".png"));labels.append(mode+" / "+label)
        for law in LAWS:
            for suffix in ["minus-linear-x8","spill-x8"]:
                Image.open(d/(law+"-"+suffix+".ppm")).save(d/(law+"-"+suffix+".png"))
        diff_paths=[d/(law+"-"+kind+".png") for kind in ["spill-x8","minus-linear-x8"] for law in LAWS]
        sheet(diff_paths,[law+" / "+kind for kind in ["Spill x8","vs Linear x8"] for law in LAWS],d/"signed-differences.png",3)
        # All working-space luminance in raw float outputs, not clipped previews.
        linear=pfm(d/"linear-yab-yab.pfm")
        linear_rgb=working_rgb(linear)
        numerical[mode]={}
        for law in LAWS:
            data=pfm(d/(law+"-yab.pfm"))
            numerical[mode][law]={"finite":bool(np.isfinite(data).all()),
                "max_Y_delta":float(np.abs(data[:,:,0]-linear[:,:,0]).max()),
                "AB_difference_p95":float(np.percentile(np.linalg.norm(data[:,:,1:]-linear[:,:,1:],axis=2),95)),
                "negative_RGB_pixel_fraction":float((working_rgb(data).min(axis=2)<-.005).mean()),
                "new_negative_RGB_fraction_vs_linear":float(((working_rgb(data).min(axis=2)<-.005)&(linear_rgb.min(axis=2)>=-.005)).mean())}
        old=root/"color-interaction-initial-normalization"/mode/"linear-yab-yab.pfm"
        if old.exists():
            assert old.read_bytes()==(d/"linear-yab-yab.pfm").read_bytes(), "Linear baseline changed"
        with open(d/"donor-receiver-trajectories.csv") as f: rows=list(csv.DictReader(f))
        swatches=Image.new("RGB",(720,18*46),(24,24,24));draw=ImageDraw.Draw(swatches)
        keys=list(dict.fromkeys((r["law"],r["density"],r["receiver"],r["donor"]) for r in rows))
        for n,key in enumerate(keys):
            curve=[r for r in rows if (r["law"],r["density"],r["receiver"],r["donor"])==key]
            draw.text((4,n*46+2),f"{LAWS[int(key[0])]} D={key[1]} plate {key[3]} -> {key[2]}",fill="white")
            for i,r in enumerate(curve):
                rgb=tuple(int(round(255*np.clip(float(r[c]),0,1))) for c in ["R","G","B"])
                draw.rectangle((i*21,n*46+20,(i+1)*21,n*46+43),fill=rgb)
        swatches.save(d/"donor-receiver-trajectories.png")
    sheet(paths,labels,out/"color-law-comparison.png")
    sheet([root/"source.png"]+paths[:4],["Original fixture"]+labels[:4],out/"C0-source-comparison.png",5)
    with open(out/"presentation-metrics.json","w") as f:json.dump(numerical,f,indent=2)
    print(out/"color-law-comparison.png")

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("root",type=Path);main(parser.parse_args().root)
