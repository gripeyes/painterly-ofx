"""Archive the repaired transport comparison without recomputing upstream stages."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import shutil
import numpy as np
from PIL import Image
from phase4_contact_sheets import contact
from phase4_diagnostic_metrics import pfm

def save(root,out):
    out.mkdir(parents=True,exist_ok=True)
    shutil.copy2(root/'repaired-reach/reach.csv',out/'reach.csv')
    modes=['C0-A3','C1-Poisson','C4-SparseCurve'];reaches=[0,12,48,128]
    rows=[];labels=[];metrics={}
    for mode in modes:
        src=root/'repaired-reach'/mode;dst=out/mode;dst.mkdir(exist_ok=True)
        paths=[];names=[];base=pfm(src/'reach-0.pfm');metrics[mode]={}
        for reach in reaches:
            Image.open(src/f'reach-{reach}.ppm').save(dst/f'reach-{reach}.png')
            shutil.copy2(src/f'reach-{reach}.pfm',dst/f'reach-{reach}.pfm')
            paths.append(dst/f'reach-{reach}.png');names.append(f'{mode}: Reach {reach}')
            result=pfm(src/f'reach-{reach}.pfm');delta=result-base
            assert np.array_equal(result[...,0],base[...,0]),'Reach changed Y in AB-only comparison'
            metrics[mode][reach]={'AB_RMSE_vs_zero_reach':float(np.sqrt(np.mean(delta[...,1:]**2))), 'max_Y_change':float(np.abs(delta[...,0]).max())}
            Image.fromarray(np.uint8(np.clip(.5+64*delta,0,1)*255)).save(dst/f'reach-{reach}-difference-x64.png')
        contact(paths,dst/'reach-comparison.png',columns=4,labels=names);rows.extend(paths);labels.extend(names)
        for family in ['Y','AB']:
            maps=[];captions=[]
            for plate in 'ABCDEF':
                for reach in reaches:
                    p=src/f'plate-{plate}-reach-{reach}-{family}-transport.pgm'
                    name=f'plate-{plate}-reach-{reach}-{family}-transport.png';Image.open(p).save(dst/name)
                    maps.append(dst/name);captions.append(f'{plate} {family} Reach {reach}')
            contact(maps,dst/f'{family}-transport-reach.png',columns=4,labels=captions)
    contact(rows,out/'C0-C1-C4-reach-comparison.png',columns=4,labels=labels)
    (out/'reach-metrics.json').write_text(json.dumps(metrics,indent=2)+'\n')
    (out/'provenance.json').write_text(json.dumps({'upstream_snapshot_SHA256':hashlib.sha256((root/'shared-upstream.snapshot').read_bytes()).hexdigest(),
        'settings':'Spill .8, Y 0, AB 1, asymmetry .5, respect .8; only Reach changes',
        'formula':'max seed(q)*exp(-D_F(q,p)/Reach); Reach=0 exact intrinsic support',
        'gate_C':'not accepted','host_validation':'blocked by unavailable Nuke license'},indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('input',type=Path);p.add_argument('output',type=Path);a=p.parse_args();save(a.input,a.output)
