"""Stage and downstream comparison sheets; presentation/measurement only."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import numpy as np
from PIL import Image,ImageDraw
from phase4_contact_sheets import contact
from phase4_diagnostic_metrics import pfm, band_metrics, broad_direction, broad_curvature

NAMES=['C0-A3','C1-Poisson','C2-SecondMoments','C3-RegionalEigen','C4-SparseCurve']
def compact(paths,target,labels):
    canvas=Image.new('RGB',(960,((len(paths)+2)//3)*280),(25,25,25));draw=ImageDraw.Draw(canvas)
    for n,(p,label) in enumerate(zip(paths,labels)):
        x=(n%3)*320;y=(n//3)*280;draw.text((x+3,y+3),label,fill='white')
        img=Image.open(p).convert('RGB');img.thumbnail((320,255));canvas.paste(img,(x,y+22))
    canvas.save(target)

def save(root,destination):
    destination.mkdir(parents=True,exist_ok=True)
    subprocess.run([sys.executable,str(Path(__file__).with_name('phase4_contact_sheets.py')),str(root),str(destination)],check=True)
    source=pfm(root/'source-yab.pfm')
    Image.open(root/'source.ppm').save(destination/'source.png')
    preImages=[];grid=[];labels=[];summary={'gate_C':'not accepted','gate_D':'diagnostic only','modes':{},'support':{}}
    for plate in 'ABCDEF':
        support=pfm(root/f'plate-{plate}-ownership-support.pfm');alpha,y,ab=[support[...,k] for k in range(3)]
        threshold=.1*max(float(y.max()),float(ab.max()))
        summary['support'][plate]={'sum_AB_over_Y':float(ab.sum()/max(1e-12,y.sum())),
            'area_Y':int(np.sum(y>threshold)),'area_AB':int(np.sum(ab>threshold)),
            'fraction_AB_ge_Y':float(np.mean(ab>=y-1e-7)),'max_AB_minus_Y':float((ab-y).max())}
        for variant in ['y','ab']:
            channel=y if variant=='y' else ab
            Image.fromarray(np.uint8(np.clip(channel,0,1)*255)).save(destination/f'plate-{plate}-support-{variant}.png')
        shutil.copy2(root/f'plate-{plate}-ownership-support.pfm',destination/f'plate-{plate}-ownership-support.pfm')
    for name in NAMES:
        directory=root/name;out=destination/name;out.mkdir(exist_ok=True)
        pre=pfm(directory/'pre-spill-yab.pfm');data={'pre_bands':band_metrics(source,pre),'pre_direction32':broad_direction(source,pre,16),'pre_curvature32':broad_curvature(source,pre,16)}
        Image.open(directory/'pre-spill.ppm').save(out/'pre-spill.png');preImages.append(out/'pre-spill.png')
        for side in ['pre','post']:
            paths=sorted(directory.glob(f'plate-?-{side}-appearance.ppm'))
            contact(paths,out/f'{side}-plate-appearance.png')
        for variant in ['spill','ab-only','ab-reach-zero']:
            post=pfm(directory/f'{variant}-yab.pfm');delta=post-pre
            data[variant]={'delta_rmse_YAB':np.sqrt(np.mean(delta*delta,axis=(0,1))).tolist(),'max_abs_Y_delta':float(np.abs(delta[...,0]).max()),
                           'affected_AB_pixels_1e-5':int(np.sum(np.linalg.norm(delta[...,1:],axis=-1)>1e-5))}
            if variant!='spill':assert np.array_equal(pre[...,0],post[...,0]),'AB-only changed Y'
            Image.open(directory/f'{variant}.ppm').save(out/f'{variant}.png')
            Image.fromarray(np.uint8(np.clip(.5+16*delta,0,1)*255)).save(out/f'{variant}-signed-YAB-difference.png')
            shutil.copy2(directory/f'{variant}-yab.pfm',out/f'{variant}-yab.pfm')
        yMaps=[];abMaps=[];transportY=[];transportAB=[]
        for plate in 'ABCDEF':
            influence=pfm(directory/f'plate-{plate}-spill-influence.pfm');transport=pfm(directory/f'plate-{plate}-spill-transport.pfm')
            for kind in ['influence','transport']:
                for variant in ['spill','ab-only','ab-reach-zero']:
                    p=directory/f'plate-{plate}-{variant}-{kind}.pfm'
                    if name!='C0-A3':
                        reference=root/'C0-A3'/p.name
                        assert p.read_bytes()==reference.read_bytes(),'representation altered shared transport or interaction'
            intrinsic=pfm(root/f'plate-{plate}-ownership-support.pfm')
            for k,family in [(0,'y'),(1,'ab')]:
                Image.fromarray(np.uint8(np.clip(influence[...,k],0,1)*255)).save(out/f'plate-{plate}-{family}-spill-influence.png')
                Image.fromarray(np.uint8(np.clip(transport[...,k],0,1)*255)).save(out/f'plate-{plate}-{family}-transport.png')
                (yMaps if k==0 else abMaps).append(out/f'plate-{plate}-{family}-spill-influence.png')
                (transportY if k==0 else transportAB).append(out/f'plate-{plate}-{family}-transport.png')
            data.setdefault('transport',{})[plate]={'gainY_sum':float(np.maximum(0,transport[...,0]-intrinsic[...,1]).sum()),
                'gainAB_sum':float(np.maximum(0,transport[...,1]-intrinsic[...,2]).sum()),
                'Y_area_005':int(np.sum(transport[...,0]>.05)),'AB_area_005':int(np.sum(transport[...,1]>.05))}
        contact(yMaps,out/'Y-spill-influence.png');contact(abMaps,out/'AB-spill-influence.png')
        contact(transportY,out/'Y-transport.png');contact(transportAB,out/'AB-transport.png')
        contact([out/'pre-spill.png',out/'spill.png',out/'ab-only.png',out/'spill-signed-YAB-difference.png'],out/'spill-comparison.png',columns=4,
                labels=['Pre-Spill','Y+AB Spill','AB-only, Y exact','Signed delta x16'])
        grid.extend([out/'pre-spill.png',out/'spill.png',out/'spill-signed-YAB-difference.png']);labels.extend([name+' pre',name+' post','Signed delta x16'])
        data['AB_reach_effect_rmse']=float(np.sqrt(np.mean((pfm(directory/'ab-only-yab.pfm')[...,1:]-pfm(directory/'ab-reach-zero-yab.pfm')[...,1:])**2)))
        shutil.copy2(directory/'pre-spill-yab.pfm',out/'pre-spill-yab.pfm');summary['modes'][name]=data
    contact([destination/'source.png']+preImages,destination/'Gate-C-comparison.png',columns=3,labels=['Source']+NAMES)
    contact(grid,destination/'Gate-C-Spill-matrix.png',columns=3,labels=labels)
    upstream=['source.png','A1-frozen-eigenmodes.png','latent-alpha.png','latent-appearance.png','public-appearance.png','gate-a-reconstruction.png','public-alpha.png','support-y.png','support-ab.png','y-chunks.png','ab-chunks.png','y-retained.png','ab-retained.png']
    compact([destination/p for p in upstream],destination/'Stage-by-stage-upstream.png',
            ['Source','A1 frozen eigen vocabulary','A2 latent alpha','A3 latent appearance','A3 public appearance','Public reconstruction','Public alpha','Y support','AB support','B: Y chunks','B: AB chunks','Retained Y contours','Retained AB contours'])
    for name in ['comparative-provenance.csv','separation.csv','source-yab.pfm','shared-upstream.snapshot','comparative-settings.csv']:
        shutil.copy2(root/name,destination/name)
    manifest={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'shared-upstream.snapshot',root/'source-yab.pfm']}
    manifest['mode_outputs']={str(p.relative_to(destination)):hashlib.sha256(p.read_bytes()).hexdigest() for name in NAMES for p in (destination/name).glob('*-yab.pfm')}
    (destination/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (destination/'comparative-metrics.json').write_text(json.dumps(summary,indent=2)+'\n')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('input',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args();save(args.input,args.output)
