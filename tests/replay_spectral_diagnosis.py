"""Offline diagnostic only: identical encoded states, weights, alpha, geometry.
Tests whether residual-only scene colors are poisoning material interaction.
Does not modify the renderer or the installed bundle.
"""
import gzip, pathlib, re, sys, json
import numpy as np
from PIL import Image
from analyze_spectral_diagnosis import pfm, rgb, M, white

def quadrature():
    text=pathlib.Path('src/core/ColorInteraction.cpp').read_text()
    observer=np.array([list(map(float,s.split(','))) for s in re.findall(r'\{([^{}]+)\}',text.split('observer[kInteractionSamples][3]=')[1].split(';')[0])])
    d65=np.array([.95047,1,1.08883]); ill=np.array(list(map(float,text.split('illuminant[kInteractionSamples]={')[1].split('}')[0].split(','))))
    br=np.array([[.8951,.2664,-.1614],[-.7502,1.7135,.0367],[.0389,-.0685,1.0296]])
    mat=M.astype('f4').astype('f8'); to_rgb=np.linalg.inv(M).astype('f4').astype('f8'); w=mat.sum(axis=1);w/=w[1]
    adapt=np.linalg.inv(br)@np.diag((br@w)/(br@d65))@br
    s=observer*ill[:,None];s[[0,-1]]*=.5
    return (to_rgb@adapt@(s*d65/s.sum(axis=0)).T).T
def yab(v):
    xyz=v@M.T
    return np.stack([xyz[...,1],xyz[...,0]/white[0]-xyz[...,1],xyz[...,2]/white[2]-xyz[...,1]],axis=-1)
def save(path,v):
    h,w,_=v.shape
    with open(path.with_suffix('.pfm'),'wb') as f:
        f.write(f'PF\n{w} {h}\n-1.0\n'.encode());f.write(v[::-1].astype('<f4').tobytes())
    Image.fromarray((np.clip(rgb(v),0,1)*255).astype('uint8')).save(path.with_suffix('.png'))
def replay(root):
    root=pathlib.Path(root); names=[line.split()[1] for line in (root/'trace-schema.txt').read_text().splitlines()]; cols={v:i for i,v in enumerate(names)}
    source=pfm(root/'source-yab.pfm');h,w,_=source.shape;states={k:np.zeros((h*w,3)) for k in ['replay-native','replay-magnitude-weighted']};full_rgb={axis:np.zeros((h*w,3)) for axis in ['Y','AB']};off=0;q=quadrature()
    with gzip.open(root/'trace.f64.gz','rb') as f:
        while True:
            data=f.read(len(names)*8*6*4096)
            if not data:break
            r=np.frombuffer(data,dtype='<f8').reshape(-1,6,len(names));nr=len(r)
            def arr(prefix,n):return r[:,:,[cols[f'{prefix}.{k}'] for k in range(n)]]
            magnitude=r[:,:,cols['positive_scene_magnitude']];residual=arr('restored_residual',3);ks=arr('KS',21);alpha=r[:,:,cols['alpha']]
            for label in states:
                result=[]
                for axis in ['Y','AB']:
                    weight=arr('weights_'+axis,8)[:,:,:6];weight=weight/weight.sum(axis=-1,keepdims=True)
                    mag=np.einsum('nij,nj->ni',weight,magnitude);res=np.einsum('nij,njk->nik',weight,residual)
                    material_weight=weight if label=='replay-native' else np.divide(weight*magnitude[:,None,:],mag[:,:,None],out=np.zeros_like(weight),where=mag[:,:,None]>0)
                    k=np.einsum('nij,njk->nik',material_weight,ks)
                    R=1/(1+k+np.sqrt(k*k+2*k));material=R@q
                    nonlinear=yab((mag[:,:,None]*material+res).astype('f4')).astype('f4')
                    full=nonlinear.copy()
                    old_nonlin=arr(axis+'.nonlinear_YAB',3);old_final=arr(axis+'.final_YAB',3)
                    # Same existing density .5. No Y/AB recombination change.
                    nonlinear[:,:,0]=old_final[:,:,0]+.5*(nonlinear[:,:,0]-old_nonlin[:,:,0])
                    bypass=r[:,:,cols[axis+'.bypass']]>0
                    nonlinear[bypass]=old_final[bypass]
                    # Early traces did not mark the outer Spill branch that
                    # skips mix entirely. Its zero-initialized trace is not
                    # a black material result. Recover that unchanged input.
                    not_called=(r[:,:,cols[axis+'.magnitude']]==0)&(arr(axis+'.mixed_KS',21).max(axis=-1)==0)&(~bypass)
                    actual=arr('recombined_plate_YAB',3)
                    nonlinear[not_called]=actual[not_called]
                    unchanged=yab(arr('input_scene_RGB',3)).astype('f4')
                    full[not_called|bypass]=unchanged[not_called|bypass]
                    if label=='replay-native':full_rgb[axis][off:off+nr]=np.einsum('ni,nij->nj',alpha,full)
                    result.append(nonlinear)
                mixed=np.stack([result[0][:,:,0],result[1][:,:,1],result[1][:,:,2]],axis=-1)
                states[label][off:off+nr]=np.einsum('ni,nij->nj',alpha,mixed)
            off+=nr
    assert off==h*w
    original=pfm(root/'spectral-yab.pfm');report={}
    for label,v in states.items():
        v=v.reshape(h,w,3);save(root/label,v)
        report[label]={'max_difference_from_current_YAB':float(np.abs(v-original).max()),'min_RGB':float(rgb(v).min()),'negative_pixels':int((rgb(v).min(axis=-1)<-.01).sum())}
    for axis,v in full_rgb.items():save(root/('corrected-spectral-'+axis+'-weights-RGB'),v.reshape(h,w,3))
    (root/'magnitude-replay.json').write_text(json.dumps(report,indent=2));print(root,report,flush=True)
if __name__=='__main__':
    for root in sys.argv[1:]:replay(root)
