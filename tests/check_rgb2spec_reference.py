"""Validate the standalone reference under its own integration, not ours."""
import ctypes, csv, json, pathlib, sys
import numpy as np
from analyze_spectral_diagnosis import pfm, rgb, M
base=pathlib.Path('build/phase4-spectral-diagnosis');lib=ctypes.CDLL(str((base/'rgb2spec-reference.dylib').resolve()))
ptr=np.ctypeslib.ndpointer(dtype=np.float32,flags='C_CONTIGUOUS')
lib.reference_native_error.argtypes=[ctypes.c_char_p,ptr,ctypes.c_int,ptr]
AP0=np.array([[1.0498110175,0,-.0000974845],[-.4959030231,1.3733130458,.0982400361],[0,0,.9912520182]])
for fixture in ['fashion','knee','lowlight']:
    fields=pathlib.Path('build/phase4-comparative')/fixture
    targets=[]
    for plate in 'ABCDEF':
        v=rgb(pfm(fields/f'plate-{plate}-appearance-yab.pfm'))[::16,::16].reshape(-1,3)
        magnitude=np.maximum(v.max(axis=-1),0)/.9
        target=np.divide(np.maximum(v,0),magnitude[:,None],out=np.full_like(v,.9),where=magnitude[:,None]>0)
        targets.append(np.maximum(target@M.T@AP0.T,0))
    data=np.ascontiguousarray(np.concatenate(targets),dtype='f4');error=np.empty(len(data),dtype='f4')
    code=lib.reference_native_error(str((base/'aces2065.coeff').resolve()).encode(),data,len(data),error)
    if code:raise RuntimeError('Reference load failed')
    np.savetxt(fields/'spectral-diagnosis'/'authors-native-fit.csv',np.column_stack([data,error]),delimiter=',',header='AP0_R,AP0_G,AP0_B,native_RGB_fit_error',comments='')
    report={'samples':len(data),'mean':float(error.mean()),'p95':float(np.quantile(error,.95)),'max':float(error.max()),'nonfinite':int((~np.isfinite(error)).sum())}
    (fields/'spectral-diagnosis'/'authors-native-fit.json').write_text(json.dumps(report,indent=2));print(fixture,report)
