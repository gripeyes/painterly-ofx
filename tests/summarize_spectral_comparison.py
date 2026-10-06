"""Compact saved acceptance evidence; all operations here are measurement/presentation."""
import json, pathlib, sys, hashlib
import numpy as np
from PIL import Image, ImageDraw
from analyze_spectral_diagnosis import pfm,rgb
for fixture in ['fashion','knee','lowlight']:
    root=pathlib.Path('build/phase4-comparative')/fixture;diagnosis=root/'spectral-diagnosis';candidate=root/'spectral-material-mass'
    export=pathlib.Path('tests/visual/renders/phase4/spectral-diagnosis')/fixture;export.mkdir(parents=True,exist_ok=True)
    paths=[('Linear',diagnosis/'linear.ppm'),('Density',diagnosis/'density.ppm'),('Original Spectral',diagnosis/'spectral.ppm'),
           ('Scene-mass Spectral',candidate/'spectral.ppm'),('Authors J/H + scene mass',diagnosis/'authors-JH-table-scene-mass.png'),
           ('C0 before Spill (saved baseline)',root/'C0-A3/pre-spill.ppm')]
    canvas=Image.new('RGB',(1440,600),'#222');draw=ImageDraw.Draw(canvas)
    for i,(label,path) in enumerate(paths):
        im=Image.open(path);im.thumbnail((480,270));x=i%3*480;y=i//3*300;canvas.paste(im,(x,y+25));draw.text((x+8,y+5),label,fill='white')
    canvas.save(export/'comparison.png')
    density=rgb(pfm(diagnosis/'density-yab.pfm'));report={}
    for label,path in [('original',diagnosis/'spectral-yab.pfm'),('candidate',candidate/'spectral-yab.pfm'),('authors',diagnosis/'authors-JH-table-scene-mass.pfm')]:
        v=pfm(path);r=rgb(v);delta=r-density
        # Not processing: localized departure from Density, measured against
        # a 3x3 neighborhood median. No filtered result is rendered or used.
        local=np.median(np.lib.stride_tricks.sliding_window_view(np.pad(delta,((1,1),(1,1),(0,0)),mode='edge'),(3,3),axis=(0,1)),axis=(-1,-2))
        isolated=np.max(np.abs(delta-local),axis=-1)
        report[label]={'max_RGB_difference_from_Density':float(np.abs(delta).max()),'negative_RGB_below_minus_0_01':int((r.min(axis=-1)<-.01).sum()),
                       'localized_difference_over_0_02':int((isolated>.02).sum()),'max_localized_difference':float(isolated.max())}
        np.save(export/(label+'-localized-difference-measurement.npy'),isolated.astype('f4'))
        diff=(v-pfm(diagnosis/'density-yab.pfm')).astype('<f4');h,w,_=v.shape
        with open(export/(label+'-minus-Density-signed-yab.pfm'),'wb') as f:f.write(f'PF\n{w} {h}\n-1.0\n'.encode());f.write(diff[::-1].tobytes())
        with open(export/(label+'-scene-RGB.pfm'),'wb') as f:f.write(f'PF\n{w} {h}\n-1.0\n'.encode());f.write(r[::-1].astype('<f4').tobytes())
    for law in ['linear','density']:
        before=(diagnosis/(law+'-yab.pfm')).read_bytes();after=(candidate/(law+'-yab.pfm')).read_bytes()
        assert before==after;report[law+'_unchanged_sha256']=hashlib.sha256(before).hexdigest()
    replay=pfm(diagnosis/'replay-magnitude-weighted.pfm');native=pfm(candidate/'spectral-yab.pfm')
    report['native_candidate_vs_frozen_trace_replay_max_YAB']=float(np.abs(replay-native).max())
    (export/'metrics.json').write_text(json.dumps(report,indent=2));print(fixture,report,flush=True)
