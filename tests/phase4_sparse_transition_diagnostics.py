"""Preserve first sparse-value-curve experiment and measurement-only evidence."""
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


def save(root,destination,previous):
    destination.mkdir(parents=True,exist_ok=True)
    metrics=measure(root)
    (destination/'metrics.json').write_text(json.dumps(metrics,indent=2)+'\n')
    subprocess.run([sys.executable,str(Path(__file__).with_name('phase4_contact_sheets.py')),str(root),str(destination)],check=True)
    for pattern in ['*yab.pfm','plate-?-*-transitions.pgm','plate-?-*-value-rails.pgm','source-boundary-strength.pgm']:
        for p in root.glob(pattern): shutil.copy2(p,destination/p.name)
    identical=0
    for p in root.glob('plate-*'):
        if any(s in p.name for s in ['-alpha.pgm','-support-','-appearance','-chunks.ppm','-retained.pgm','-removed.pgm']):
            q=previous/p.name
            if q.exists():
                assert hashlib.sha256(p.read_bytes()).digest()==hashlib.sha256(q.read_bytes()).digest(),str(p)
                identical+=1
    curves=list(csv.DictReader((root/'transition-curves.csv').open()))
    solves=list(csv.DictReader((root/'transition-solves.csv').open()))
    summary={'upstream_identical_files':identical,'families':{}}
    strengths=np.asarray(Image.open(root/'source-boundary-strength.pgm'))
    for family,id in [('y','0'),('ab','1')]:
        records=[s for s in solves if s['family']==id]
        selected=[s for s in curves if s['family']==id and s['point']=='0']
        coverage=0;supportMass=0;errors=0;unsupported=0
        for plate in 'ABCDEF':
            original=pfm(root/f'plate-{plate}-appearance-yab.pfm')
            field=pfm(root/f'plate-{plate}-synthesized-yab.pfm')
            support=np.asarray(Image.open(root/f'plate-{plate}-support-{family}.pgm'))
            retained=np.asarray(Image.open(root/f'plate-{plate}-{family}-retained.pgm'))>0
            ch=slice(0,1) if family=='y' else slice(1,3)
            changed=np.any(field[...,ch]!=original[...,ch],axis=-1)
            unsupported+=int(np.sum(changed&(support==0)))
            errors+=int(np.sum(changed&retained&(strengths>=193)))
            coverage+=int(np.sum(changed&(support>0)));supportMass+=int(np.sum(support>0))
        assert errors==0 and unsupported==0,(family,errors,unsupported)
        summary['families'][family]={'accepted_curves':len(selected),'curve_vertices':sum(1 for s in curves if s['family']==id),
            'domains':len(records),'solved_domains':sum(s['solved']=='1' for s in records),
            'solved_pixel_fraction':sum(int(s['pixels']) for s in records if s['solved']=='1')/max(1,sum(int(s['pixels']) for s in records)),
            'curve_touched_domain_pixel_fraction':sum(int(s['pixels']) for s in records if int(s['curve_constraints'])>0)/max(1,sum(int(s['pixels']) for s in records)),
            'curve_constraints':sum(int(s['curve_constraints']) for s in records),
            'structural_constraints':sum(int(s['structural_constraints']) for s in records),
            'max_residual':max([float(s['residual']) for s in records]+[0]),
            'supported_changed_fraction':coverage/max(1,supportMass),
            'conservative_structural_value_errors':errors,'zero_support_errors':unsupported}
    (destination/'preservation-and-coverage.json').write_text(json.dumps(summary,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('input',type=Path);parser.add_argument('output',type=Path);parser.add_argument('previous',type=Path)
    args=parser.parse_args();save(args.input,args.output,args.previous)
