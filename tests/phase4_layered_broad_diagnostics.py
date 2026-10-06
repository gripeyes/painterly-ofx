"""C5 presentation/measurement only; no processing-data edits or acceptance by RMSE."""
from pathlib import Path
import csv, hashlib, json, sys
import numpy as np
from PIL import Image
from phase4_contact_sheets import contact
from phase4_diagnostic_metrics import pfm, band_metrics, broad_direction, broad_curvature

def gray(v, path, signed=False):
    if signed:
        scale=max(1e-6,float(np.quantile(np.abs(v),.98)))
        v=.5+.45*v/scale
    else:
        lo,hi=np.quantile(v,[.02,.98]);v=(v-lo)/max(1e-6,hi-lo)
    Image.fromarray(np.uint8(np.clip(v,0,1)*255)).save(path)

def save(root, dest):
    dest.mkdir(parents=True,exist_ok=True);c=root/'C5-LayeredBroadFields'
    baseline=pfm(c/'C0.pfm');report={'gate_C':'not accepted pending photographs','spill':0,'law':'Linear YAB','modes':{}}
    paths=[];labels=[]
    for mode in ['C0','Y-only','AB-only','Y-AB']:
        Image.open(c/(mode+'.ppm')).save(dest/(mode+'.png'))
        paths.append(dest/(mode+'.png'));labels.append(mode)
        v=pfm(c/(mode+'.pfm'))
        report['modes'][mode]={'bands':band_metrics(baseline,v),'direction':broad_direction(baseline,v,16),'curvature':broad_curvature(baseline,v,16)}
        if mode=='Y-only':assert np.array_equal(v[...,1:],baseline[...,1:]),'Y-only altered AB'
        if mode=='AB-only':assert np.array_equal(v[...,0],baseline[...,0]),'AB-only altered Y'
    contact(paths,dest/'isolation.png',columns=4,labels=labels)
    for plate in 'ABCDEF':
        prefix='plate-'+plate;images=[];names=[]
        for kind in ['broad-target','broad','structure','medium','micro','pre-spill']:
            v=pfm(c/(prefix+'-'+kind+'.pfm'))
            for k,ch in enumerate(['Y','A','B']):
                path=dest/(prefix+'-'+kind+'-'+ch+'.png');gray(v[...,k],path,kind in ['structure','medium','micro']);images.append(path);names.append(kind+' '+ch)
        contact(images,dest/(prefix+'-information.png'),columns=3,labels=names)
        images=[];names=[]
        for f in sorted(c.glob(prefix+'-layer-*-field.pfm')):
            stem=f.stem[:-6];membership=c/(stem+'-membership.pgm');v=pfm(f)
            path=dest/(stem+'-membership.png');Image.open(membership).save(path);images.append(path);names.append(stem+' occupancy')
            for k,ch in enumerate(['Y','A','B']):
                path=dest/(stem+'-'+ch+'.png');gray(v[...,k],path);images.append(path);names.append(ch+' field')
        contact(images,dest/(prefix+'-sublayers.png'),columns=4,labels=names)
        v=[pfm(c/(prefix+'-'+kind+'.pfm')) for kind in ['broad','structure','medium','micro']]
        original=pfm(root/(prefix+'-appearance-yab.pfm'))
        report.setdefault('decomposition_max_error',{})[plate]=float(np.max(np.abs(sum(v)-original)))
    for f in ['fits.csv','layers.csv','provenance.txt']:(dest/f).write_bytes((c/f).read_bytes())
    report['frozen_inputs']={f:hashlib.sha256((root/f).read_bytes()).hexdigest() for f in ['shared-upstream.snapshot','source-yab.pfm']}
    report['active_layers']=list(csv.DictReader((c/'fits.csv').open()))
    (dest/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')

if __name__=='__main__':save(Path(sys.argv[1]),Path(sys.argv[2]))
