"""Save the isolated exact/broad side-boundary comparison; no image processing targets."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import numpy as np
from PIL import Image
from phase4_diagnostic_metrics import pfm, measure


def save(root, destination, previous):
    destination.mkdir(parents=True, exist_ok=True)
    script=Path(__file__).with_name('phase4_contact_sheets.py')
    subprocess.run([sys.executable,str(script),str(root),str(destination)],check=True)
    rows=[]
    upstream=0
    for p in root.glob('plate-*'):
        if any(s in p.name for s in ['-alpha.pgm','-support-','-appearance','-chunks.ppm','-retained.pgm','-removed.pgm']):
            q=previous/p.name
            if q.exists():
                assert hashlib.sha256(p.read_bytes()).digest()==hashlib.sha256(q.read_bytes()).digest(),str(p)
                upstream+=1
    for side in ['exact-side','broad-side']:
        source=root/side
        target=destination/side
        target.mkdir(exist_ok=True)
        metrics=measure(source)
        (target/'metrics.json').write_text(json.dumps(metrics,indent=2)+'\n')
        subprocess.run([sys.executable,str(script),str(source),str(target)],check=True)
        for pattern in ['synthesized-yab.pfm','source-yab.pfm','regional-modes.csv','plate-?-boundary-yab.pfm']:
            for p in source.glob(pattern): shutil.copy2(p,target/p.name)
        for plate in 'ABCDEF':
            original=pfm(root/f'plate-{plate}-appearance-yab.pfm')
            boundary=pfm(source/f'plate-{plate}-boundary-yab.pfm')
            field=pfm(source/f'plate-{plate}-synthesized-yab.pfm')
            for family,channels in [('y',slice(0,1)),('ab',slice(1,3))]:
                mask=np.asarray(Image.open(root/f'plate-{plate}-{family}-retained.pgm'))>0
                changed=np.any(boundary[...,channels]!=original[...,channels],axis=-1)
                outside=int(np.sum(changed & ~mask))
                assert outside==0,(side,plate,family,'boundary geometry changed')
                errors=int(np.sum(np.any(field[...,channels]!=boundary[...,channels],axis=-1)&mask))
                assert errors==0,(side,plate,family,'boundary values not imposed')
                support=np.asarray(Image.open(root/f'plate-{plate}-support-{family}.pgm'))
                unsafe=int(np.sum(np.any(field[...,channels]!=original[...,channels],axis=-1)&(support==0)))
                assert unsafe==0,(side,plate,family,'unsupported extrapolation')
                rows.append([side,plate,family,int(np.sum(mask)),int(np.sum(changed)),outside,errors,unsafe])
    exact=pfm(root/'exact-side/synthesized-yab.pfm')
    prior=pfm(previous/'modes-2-1/synthesized-yab.pfm')
    summary={'upstream_identical_files':upstream,'baseline_previous_max_error':float(np.max(np.abs(exact-prior))),
             'baseline_previous_rmse':float(np.sqrt(np.mean((exact-prior)**2)))}
    (destination/'preservation.json').write_text(json.dumps(summary,indent=2)+'\n')
    with (destination/'boundary-check.csv').open('w') as output:
        writer=csv.writer(output)
        writer.writerow(['side','plate','family','retained_pixels','changed_boundary_pixels','changes_outside_contour','imposed_value_errors','zero_support_errors'])
        writer.writerows(rows)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('input',type=Path)
    parser.add_argument('output',type=Path)
    parser.add_argument('previous',type=Path)
    args=parser.parse_args()
    save(args.input,args.output,args.previous)
