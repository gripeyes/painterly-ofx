"""One standalone reference inversion comparison, never linked into Pigment.
Authors' ACES2065-1/D60 table + their LM refinement, converted from ACEScg.
Mixing, residual strategy, quadrature and all spatial data remain unchanged.
"""
import ctypes, gzip, json, pathlib, sys
import numpy as np
from analyze_spectral_diagnosis import pfm, M, rgb
from replay_spectral_diagnosis import quadrature, yab, save
AP0=np.array([[1.0498110175,0,-.0000974845],[-.4959030231,1.3733130458,.0982400361],[0,0,.9912520182]])
base=pathlib.Path('build/phase4-spectral-diagnosis')
lib=ctypes.CDLL(str((base/'rgb2spec-reference.dylib').resolve()))
ptr=np.ctypeslib.ndpointer(dtype=np.float32,flags='C_CONTIGUOUS')
lib.reference_spectra.argtypes=[ctypes.c_char_p,ptr,ctypes.c_int,ptr,ptr];lib.reference_spectra.restype=ctypes.c_int
def compare(root):
    root=pathlib.Path(root); names=[line.split()[1] for line in (root/'trace-schema.txt').read_text().splitlines()]; cols={v:i for i,v in enumerate(names)}
    source=pfm(root/'source-yab.pfm');h,w,_=source.shape;out=np.zeros((h*w,3));off=0;q=quadrature();metrics=[];ap0_outside=0;ap0_residual_max=0
    with gzip.open(root/'trace.f64.gz','rb') as f:
        while True:
            data=f.read(len(names)*8*6*4096)
            if not data:break
            r=np.frombuffer(data,dtype='<f8').reshape(-1,6,len(names));nr=len(r)
            def arr(prefix,n):return r[:,:,[cols[f'{prefix}.{k}'] for k in range(n)]]
            magnitude=r[:,:,cols['positive_scene_magnitude']];alpha=r[:,:,cols['alpha']]
            target=arr('bounded_material_RGB',3).copy();target[magnitude==0]=.9
            ap0=(target@M.T@AP0.T).reshape(-1,3).astype('f4')
            ap0_outside+=int((ap0<0).any(axis=1).sum());ap0_residual_max=max(ap0_residual_max,float(np.maximum(-ap0,0).max()))
            if np.any(ap0>1):raise RuntimeError('Reference table input above 1; refusing implicit clipping')
            # ACEScg has negative-Z primaries: a bounded AP1 material may be
            # outside even AP0's nonnegative domain. Only the representative
            # material is projected; the exact full RGB error is retained in
            # the residual below. Never clip the scene input or final output.
            ap0=np.maximum(ap0,0)
            coeff=np.empty_like(ap0);samples=np.empty((len(ap0),21),dtype='f4')
            code=lib.reference_spectra(str((base/'aces2065.coeff').resolve()).encode(),np.ascontiguousarray(ap0),len(ap0),coeff,samples)
            if code:raise RuntimeError('Reference table load failed')
            samples=samples.reshape(nr,6,21).astype('f8')
            if not np.isfinite(samples).all():raise RuntimeError('Nonfinite reference spectrum')
            # Exactly the existing numerical K/S floor, counted, not a new repair.
            safe=np.clip(samples,1e-6,1-1e-6);ks=(1-safe)**2/(2*safe);decoded=safe@q
            residual=arr('input_scene_RGB',3)-magnitude[:,:,None]*decoded
            fit=np.linalg.norm(decoded-target,axis=-1)
            metrics.append(np.stack([(alpha*fit).sum(axis=1),(alpha*r[:,:,cols['RGB_fit_error']]).sum(axis=1),(alpha*ks.max(axis=-1)).sum(axis=1),(alpha*(safe!=samples).any(axis=-1)).sum(axis=1)],axis=-1))
            result=[]
            for axis in ['Y','AB']:
                weights=arr('weights_'+axis,8)[:,:,:6];weights=weights/weights.sum(axis=-1,keepdims=True)
                mag=np.einsum('nij,nj->ni',weights,magnitude);res=np.einsum('nij,njk->nik',weights,residual)
                material_weight=np.divide(weights*magnitude[:,None,:],mag[:,:,None],out=np.zeros_like(weights),where=mag[:,:,None]>0)
                k=np.einsum('nij,njk->nik',material_weight,ks);R=1/(1+k+np.sqrt(k*k+2*k))
                nonlinear=yab((mag[:,:,None]*(R@q)+res).astype('f4')).astype('f4')
                old_nonlin=arr(axis+'.nonlinear_YAB',3);old_final=arr(axis+'.final_YAB',3)
                nonlinear[:,:,0]=old_final[:,:,0]+.5*(nonlinear[:,:,0]-old_nonlin[:,:,0])
                not_called=(r[:,:,cols[axis+'.magnitude']]==0)&(arr(axis+'.mixed_KS',21).max(axis=-1)==0)
                nonlinear[not_called]=arr('recombined_plate_YAB',3)[not_called]
                result.append(nonlinear)
            mixed=np.stack([result[0][:,:,0],result[1][:,:,1],result[1][:,:,2]],axis=-1)
            out[off:off+nr]=np.einsum('ni,nij->nj',alpha,mixed);off+=nr
    assert off==h*w
    out=out.reshape(h,w,3);save(root/'authors-JH-table-scene-mass',out)
    metrics=np.concatenate(metrics);np.savez_compressed(root/'authors-JH-fit-diagnostics.npz',metrics=metrics.reshape(h,w,4))
    report={'reference_commit':'721145dedf2491851bd46ab8fd165955cb38ddaf','table_resolution':32,'gamut':'ACES2065_1 D60, ACEScg converted via XYZ; no input/output transform',
            'mean_fit_error_current_21_sample_quadrature':float(metrics[:,0].mean()),'mean_fit_error_original':float(metrics[:,1].mean()),
            'AP0_out_of_model_material_count':ap0_outside,'AP0_max_negative_material_residual':ap0_residual_max,
            'mean_max_KS':float(metrics[:,2].mean()),'floor_fraction':float(metrics[:,3].mean()),'min_RGB':float(rgb(out).min()),'negative_pixels':int((rgb(out).min(axis=-1)<-.01).sum())}
    (root/'authors-JH-comparison.json').write_text(json.dumps(report,indent=2));print(root,report,flush=True)
if __name__=='__main__':
    for root in sys.argv[1:]:compare(root)
