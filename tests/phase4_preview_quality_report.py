"""Measurement/presentation only: no image-model changes or Full rerenders."""
import json
import subprocess
import sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

root=Path(sys.argv[1]);out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
def read(path):
    pfm=root/(path.stem+".pfm")
    if not pfm.exists():subprocess.run(["/opt/homebrew/bin/oiiotool",str(path),"--ch","R,G,B","-o",str(pfm)],check=True)
    with pfm.open('rb') as f:
        assert f.readline().strip()==b'PF'
        w,h=map(int,f.readline().split());scale=float(f.readline())
        return np.flipud(np.frombuffer(f.read(),dtype='<f4' if scale<0 else '>f4').reshape(h,w,3)).copy()
def yab(rgb):
    matrix=np.array([[.6624541811,.1340042065,.1561876870],[.2722287168,.6740817658,.0536895174],[-.0055746495,.0040607335,1.0103391003]])
    xyz=rgb@matrix.T;white=matrix.sum(axis=1);white/=white[1]
    return np.stack([xyz[...,1],xyz[...,0]/white[0]-xyz[...,1],xyz[...,2]/white[2]-xyz[...,1]],axis=-1)
def broad(v):
    h,w=v.shape[:2];h-=h%32;w-=w%32
    return v[:h,:w].reshape(h//32,32,w//32,32,3).mean(axis=(1,3))
def edge_test(original,output):
    source=yab(original)[...,0];target=yab(output)[...,0];result={}
    for axis in [0,1]:
        a=np.abs(np.diff(source,axis=axis));b=np.abs(np.diff(target,axis=axis))
        indices=np.argwhere(a>max(1e-5,float(np.quantile(a,.995))))[::8]
        shifts=[]
        for y,x in indices:
            coordinate=y if axis==0 else x
            if coordinate<3 or coordinate>=a.shape[axis]-3:continue
            scores=b[y-3:y+4,x] if axis==0 else b[y,x-3:x+4]
            expected=a[y-3:y+4,x] if axis==0 else a[y,x-3:x+4]
            # Two sides of a one-pixel line can be equally strong. A changed
            # tie ordering is not relocation of that line.
            sourcePeaks=np.flatnonzero(expected>=expected.max()*(1-1e-5))
            shifts.append(int(np.min(np.abs(sourcePeaks-int(np.argmax(scores))))))
        result['vertical' if axis==0 else 'horizontal']={"samples":len(shifts),"p95_peak_shift_pixels":float(np.quantile(shifts,.95)) if shifts else None,"fraction_within_one_pixel":float(np.mean(np.array(shifts)<=1)) if shifts else None}
    return result
def tile(rgb):
    # Display-only gamma, identical for all panels; not part of Pigment.
    return Image.fromarray(np.uint8(np.clip(rgb,0,1)**(1/2.2)*255+.5)).resize((480,270))
reports={}
for fixture in ['fashion','knee','lowlight','thin-structures']:
    source=read(root/(fixture+'-Source.exr'))
    panels=[('Source',source)];reports[fixture]={}
    reference=read(Path('build/phase4-artist-1920-full/cold.exr')) if fixture=='fashion' else None
    for label in ['Fast','Balanced','Detailed','PreSpill','StrongSpill','ScaleSmall','ScaleLarge']:
        rgb=read(root/(fixture+'-'+label+'.exr'));panels.append((label,rgb))
        stats={"source_edge_localization":edge_test(source,rgb),"finite":bool(np.isfinite(rgb).all())}
        if reference is not None:
            error=rgb-reference;stats['reference_RGB_rmse']=float(np.sqrt(np.mean(error**2)))
            field=broad(yab(rgb));truth=broad(yab(reference))
            stats['reference_32px_broad_YAB_rmse']=np.sqrt(np.mean((field-truth)**2,axis=(0,1))).tolist()
        reports[fixture][label]=stats
    if reference is not None:panels.append(('Saved Full Reference',reference))
    sheet=Image.new('RGB',(480*3,294*3),(24,24,24));draw=ImageDraw.Draw(sheet)
    for i,(label,rgb) in enumerate(panels):
        x=(i%3)*480;y=(i//3)*294;draw.text((x+6,y+5),fixture+' / '+label,fill='white');sheet.paste(tile(rgb),(x,y+24))
    sheet.save(out/(fixture+'-quality.png'))
(out/'metrics.json').write_text(json.dumps({"measurement_limits":"Source edge test is supporting evidence, not Reference topology equivalence. Only fashion has a saved matching Full-HD Reference. Broad statistics use non-overlapping 32px cells, never a reconstruction target.","fixtures":reports},indent=2))
print(json.dumps(reports,indent=2))
