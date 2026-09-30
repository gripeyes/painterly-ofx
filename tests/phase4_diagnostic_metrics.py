"""Unclipped CPU gate diagnostics; requires NumPy and Pillow, no render changes."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def pfm(path):
    with path.open('rb') as f:
        assert f.readline().strip() == b'PF'
        width, height = map(int, f.readline().split())
        scale = float(f.readline())
        assert scale < 0  # harness writes little-endian floats
        return np.fromfile(f, dtype='<f4').reshape(height, width, 3)[::-1].astype(float)


def gradient_terms(alpha, appearance):
    ownership_energy = np.zeros(3)
    appearance_energy = np.zeros(3)
    composite_energy = np.zeros(3)
    for axis in (1, 2):
        low = [slice(None)] * 3
        high = [slice(None)] * 3
        low[axis] = slice(None, -1)
        high[axis] = slice(1, None)
        ap, aq = alpha[tuple(low)], alpha[tuple(high)]
        low.append(slice(None))
        high.append(slice(None))
        up, uq = appearance[tuple(low)], appearance[tuple(high)]
        a = np.sum((aq-ap)[..., None] * .5 * (up+uq), axis=0)
        b = np.sum(.5 * (ap+aq)[..., None] * (uq-up), axis=0)
        ownership_energy += np.sum(a*a, axis=(0, 1))
        appearance_energy += np.sum(b*b, axis=(0, 1))
        composite_energy += np.sum((a+b)**2, axis=(0, 1))
    return {'ownership_gradient_energy':ownership_energy.tolist(),
            'appearance_gradient_energy':appearance_energy.tolist(),
            'composite_gradient_energy':composite_energy.tolist(),
            'cancellation_ratio':((ownership_energy+appearance_energy)/
                                  np.maximum(1e-20, composite_energy)).tolist()}


def analysis_gaussian(values, sigma):
    """Measurement only. Never used by Pigment's renderer."""
    radius=int(np.ceil(3*sigma))
    offsets=np.arange(-radius,radius+1)
    kernel=np.exp(-.5*(offsets/sigma)**2)
    kernel/=np.sum(kernel)
    result=values.copy()
    for axis in (0,1):
        padding=[(0,0)]*3
        padding[axis]=(radius,radius)
        extended=np.pad(result,padding,mode='edge')
        filtered=np.zeros_like(result)
        for k,weight in enumerate(kernel):
            selection=[slice(None)]*3
            selection[axis]=slice(k,k+result.shape[axis])
            filtered+=weight*extended[tuple(selection)]
        result=filtered
    return result


def band_metrics(source, output):
    sf=analysis_gaussian(source,1)
    sm=analysis_gaussian(source,4)
    of=analysis_gaussian(output,1)
    om=analysis_gaussian(output,4)
    fine_source=np.mean((source-sf)**2,axis=(0,1))
    medium_source=np.mean((sf-sm)**2,axis=(0,1))
    return {'qualification':'Analysis-only bands; energy reduction is not an artistic pass.',
            'fine_energy_ratio':(np.mean((output-of)**2,axis=(0,1))/np.maximum(1e-20,fine_source)).tolist(),
            'medium_energy_ratio':(np.mean((of-om)**2,axis=(0,1))/np.maximum(1e-20,medium_source)).tolist(),
            'broad_normalized_rmse':(np.sqrt(np.mean((om-sm)**2,axis=(0,1)))/
                                     np.maximum(1e-6,np.std(sm,axis=(0,1)))).tolist()}


def broad_direction(source, output, half_span):
    """Measurement-only centered long chords, without filtered target images.

    Each gradient spans 2*half_span pixels. This whole-frame statistic may
    cross contours, so it supplements (never replaces) photographic judgment.
    """
    h=half_span
    def chords(v):
        gx=(v[h:-h,2*h:]-v[h:-h,:-2*h])/(2*h)
        gy=(v[2*h:,h:-h]-v[:-2*h,h:-h])/(2*h)
        return np.stack([gx,gy],axis=-1)
    a,b=chords(source),chords(output)
    ma=np.linalg.norm(a,axis=-1);mb=np.linalg.norm(b,axis=-1)
    dot=np.sum(a*b,axis=-1)
    result=[]
    for channel in range(3):
        scale=max(1e-8,float(np.std(source[...,channel])))
        mask=ma[...,channel]>scale*.01/(2*h)
        weights=ma[...,channel]*mask
        total=max(1e-20,float(np.sum(weights)))
        cosine=np.clip(dot[...,channel]/np.maximum(1e-20,ma[...,channel]*mb[...,channel]),-1,1)
        result.append({'weighted_cosine':float(np.sum(weights*cosine)/total),
                       'weighted_angle_degrees':float(np.sum(weights*np.degrees(np.arccos(cosine)))/total),
                       'relative_gradient_error':float(np.sqrt(np.sum((a[...,channel,:]-b[...,channel,:])**2)/
                                                               max(1e-20,np.sum(a[...,channel,:]**2)))),
                       'measured_fraction':float(np.mean(mask))})
    return result


def direction_image(source, output, half_span, path):
    """Grayscale Y angle error: black aligned, white reversed; weak source dark."""
    h=half_span
    def chords(v):
        return np.stack([(v[h:-h,2*h:,0]-v[h:-h,:-2*h,0]),
                         (v[2*h:,h:-h,0]-v[:-2*h,h:-h,0])],axis=-1)
    a,b=chords(source),chords(output)
    ma=np.linalg.norm(a,axis=-1);mb=np.linalg.norm(b,axis=-1)
    cosine=np.clip(np.sum(a*b,axis=-1)/np.maximum(1e-20,ma*mb),-1,1)
    image=np.zeros(source.shape[:2])
    image[h:-h,h:-h]=np.arccos(cosine)/np.pi*(ma>max(1e-8,float(np.std(source[...,0])))*.01)
    Image.fromarray(np.uint8(image*255)).save(path)


def measure(directory):
    paths = sorted(directory.glob('plate-?-appearance-yab.pfm'))
    automatic = np.stack([pfm(p) for p in paths])
    synthesized = np.stack([pfm(directory/(p.name.replace('appearance', 'synthesized')))
                             for p in paths])
    alpha = np.stack([np.asarray(Image.open(directory/(p.name[:7]+'-alpha.pgm')),
                                  dtype=float)/255 for p in paths])
    # Display alpha is quantized; term energies are supporting diagnostics,
    # not numerical parity or exact sum-to-one evidence.
    alpha /= np.maximum(1e-20, np.sum(alpha, axis=0))
    source = pfm(directory/'source-yab.pfm')
    output = pfm(directory/'synthesized-yab.pfm')
    result = {'alpha_precision':'8-bit saved diagnostic, renormalized',
            'automatic_interlayer_rms_yab':np.sqrt(np.mean(np.var(automatic, axis=0), axis=(0,1))).tolist(),
            'automatic':gradient_terms(alpha, automatic),
            'synthesized':gradient_terms(alpha, synthesized),
            'output_rmse_yab':np.sqrt(np.mean((output-source)**2, axis=(0, 1))).tolist(),
            'bands':band_metrics(source,output),
            'broad_direction':{str(span*2):broad_direction(source,output,span) for span in [8,16,32]},
            'automatic_layer_min':np.min(automatic, axis=(1, 2)).tolist(),
            'automatic_layer_max':np.max(automatic, axis=(1, 2)).tolist()}
    baseline = directory/'no-interior-yab.pfm'
    if (directory/'mean-only-yab.pfm').exists():
        baseline=directory/'mean-only-yab.pfm'
    if baseline.exists():
        before = pfm(baseline)
        result['baseline_bands'] = band_metrics(source, before)
        if baseline.name=='no-interior-yab.pfm':
            result['without_interior']=result['baseline_bands']
        result['baseline_name']=baseline.name
        result['baseline_broad_direction']={str(span*2):broad_direction(source,before,span) for span in [8,16,32]}
        result['interior_change_rmse_yab'] = np.sqrt(np.mean((output-before)**2, axis=(0,1))).tolist()
        for channel in ['y','ab']:
            masks=np.stack([np.asarray(Image.open(directory/(p.name[:7]+f'-broad-{channel}-influence.pgm')))>0
                            for p in paths])
            result[f'{channel}_moment_footprint_alpha_weighted_fraction']=float(np.mean(np.sum(alpha*masks,axis=0)))
        # Presentation only: unclipped differences stay in the PFM files.
        Image.fromarray(np.uint8(np.clip(.5+4*(output-source),0,1)*255)).save(directory/'interior-difference.png')
        for span in [8,16,32]:
            direction_image(source,before,span,directory/f'baseline-y-direction-error-{2*span}.png')
            direction_image(source,output,span,directory/f'result-y-direction-error-{2*span}.png')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result=json.dumps(measure(args.directory), indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(result+'\n')
    print(result)
