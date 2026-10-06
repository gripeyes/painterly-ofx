"""Measurement only: never changes processing data or applies cleanup."""
import csv, gzip, json, pathlib, sys
import numpy as np
from PIL import Image, ImageDraw

M=np.array([[.6624541811,.1340042065,.1561876870],[.2722287168,.6740817658,.0536895174],[-.0055746495,.0040607335,1.0103391003]])
white=M.sum(axis=1)/M[1].sum()
def pfm(path):
    with open(path,'rb') as f:
        assert f.readline().strip()==b'PF'
        w,h=map(int,f.readline().split()); scale=float(f.readline())
        return np.frombuffer(f.read(),dtype='<f4' if scale<0 else '>f4').reshape(h,w,3)[::-1].copy()
def rgb(yab):
    xyz=np.stack([(yab[...,0]+yab[...,1])*white[0],yab[...,0],(yab[...,0]+yab[...,2])*white[2]],axis=-1)
    return xyz@np.linalg.inv(M).T
def analyze(root):
    root=pathlib.Path(root); names=[line.split()[1] for line in (root/'trace-schema.txt').read_text().splitlines()]
    cols={n:i for i,n in enumerate(names)}; n=len(names)
    source=pfm(root/'source-yab.pfm'); h,w,_=source.shape
    linear=pfm(root/'linear-yab.pfm'); density=pfm(root/'density-yab.pfm'); spectral=pfm(root/'spectral-yab.pfm')
    features=np.zeros((h*w,15)); offset=0
    with gzip.open(root/'trace.f64.gz','rb') as f:
        while True:
            data=f.read(n*8*6*4096)
            if not data: break
            r=np.frombuffer(data,dtype='<f8').reshape(-1,6,n)
            alpha=r[:,:,cols['alpha']]; rows=len(r)
            def arr(prefix,count):return r[:,:, [cols[f'{prefix}.{k}'] for k in range(count)]]
            weights=arr('weights_AB',8)[:,:,:6]; weights=weights/weights.sum(axis=-1,keepdims=True)
            donor_alpha=(alpha[:,:,None]*weights).sum(axis=1)
            pre_rgb=arr('AB.nonlinear_RGB',3).copy()
            no_call=(r[:,:,cols['AB.magnitude']]==0)&(arr('AB.mixed_KS',21).max(axis=-1)==0)
            pre_rgb[no_call]=arr('input_scene_RGB',3)[no_call]
            values=[r[:,:,cols['positive_scene_magnitude']],r[:,:,cols['RGB_fit_error']],
                np.linalg.norm(arr('restored_residual',3),axis=-1),np.abs(arr('coefficients',3)).max(axis=-1),
                arr('reflectance',21).min(axis=-1),arr('KS',21).max(axis=-1),
                r[:,:,cols['coefficient_bound_attempts']]>0,r[:,:,cols['reflectance_floor']]>0,
                arr('AB.mixed_KS',21).max(axis=-1),pre_rgb.min(axis=-1),
                arr('bounded_material_RGB',3).min(axis=-1),r[:,:,cols['grey_clamp']],
                r[:,:,cols['positive_scene_magnitude']]==0,arr('input_scene_RGB',3).min(axis=-1),arr('input_scene_RGB',3).max(axis=-1)]
            for j,v in enumerate(values):features[offset:offset+rows,j]=((alpha if j in (8,9) else donor_alpha)*v).sum(axis=1)
            offset+=rows
    assert offset==w*h
    labels=['magnitude','fit_error','residual','coeff_max','R_min','KS_max','bound_fraction','floor_fraction','mixed_KS_max','pre_recomb_RGB_min','material_min','grey_clamp_fraction','zero_magnitude_fraction','input_min','input_max']
    srgb=rgb(spectral); drgb=rgb(density); delta=np.max(np.abs(srgb-drgb),axis=-1).ravel()
    bad=(srgb.min(axis=-1)<-.01).ravel()
    report={'size':[w,h],'negative_output_pixels':int(bad.sum()),'max_RGB_difference':float(delta.max()),'features':{}}
    for j,label in enumerate(labels):
        v=features[:,j]; cor=float(np.corrcoef(v,delta)[0,1]) if v.std()>0 else None
        report['features'][label]={'correlation_with_RGB_difference':cor,'all_mean':float(v.mean()),'bad_mean':float(v[bad].mean()) if bad.any() else None,'range':[float(v.min()),float(v.max())]}
    for label in ['spectral-Y-weights-RGB','spectral-AB-weights-RGB']:
        corrected=root/('corrected-'+label+'.pfm')
        v=rgb(pfm(corrected if corrected.exists() else root/(label+'.pfm')))
        report[label]={'negative_pixels':int((v.min(axis=-1)<-.01).sum()),'min':float(v.min()),'max':float(v.max())}
    (root/'localization.json').write_text(json.dumps(report,indent=2))
    with open(root/'catastrophic-pixels.csv','w') as f:
        out=csv.writer(f);out.writerow(['x','y','delta_RGB','spectral_R','spectral_G','spectral_B']+labels)
        for ix in np.argsort(delta)[-256:][::-1]:out.writerow([int(ix%w),int(ix//w),delta[ix],*srgb.reshape(-1,3)[ix],*features[ix]])
    panels=['linear','density','spectral','spectral-Y-weights-RGB','spectral-AB-weights-RGB']
    canvas=Image.new('RGB',(480*3,250*2),'#222');draw=ImageDraw.Draw(canvas)
    for i,label in enumerate(panels):
        corrected=root/('corrected-'+label+'.png')
        im=Image.open(corrected if corrected.exists() else root/(label+'.ppm'));im.thumbnail((480,220));x=(i%3)*480;y=(i//3)*250
        canvas.paste(im,(x,y+25));draw.text((x+8,y+5),label,fill='white')
    canvas.save(root/'comparison.png')
    print(root, json.dumps({k:v for k,v in report.items() if k!='features'}))
if __name__=='__main__':
    for root in sys.argv[1:]:analyze(root)
