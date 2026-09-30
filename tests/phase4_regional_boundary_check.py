"""Read-only processing checks on saved eigenfield outputs; writes diagnostics only."""
import argparse
import csv
from pathlib import Path
import numpy as np
from PIL import Image
from phase4_diagnostic_metrics import pfm


def check(root, destination):
    destination.mkdir(parents=True,exist_ok=True)
    with (destination/'regional-retained-check.csv').open('w') as output:
        writer=csv.writer(output)
        writer.writerow(['pair','plate','family','checked_pixels','changed_pixels'])
        for directory in sorted(root.glob('modes-*')):
            for plate in 'ABCDEF':
                original=pfm(root/f'plate-{plate}-appearance-yab.pfm')
                rendered=pfm(directory/f'plate-{plate}-synthesized-yab.pfm')
                for family in ['y','ab']:
                    # Retained display is an exact binary contour marker.
                    # Safe zeros of support are checked, not quantized .02 cuts.
                    mask=np.asarray(Image.open(root/f'plate-{plate}-{family}-retained.pgm'))>0
                    support=np.asarray(Image.open(root/f'plate-{plate}-support-{family}.pgm'))
                    mask|=support==0
                    mask[0,:]=mask[-1,:]=mask[:,0]=mask[:,-1]=True
                    channel=slice(0,1) if family=='y' else slice(1,3)
                    changed=np.any(original[...,channel]!=rendered[...,channel],axis=-1)
                    errors=int(np.sum(mask & changed))
                    writer.writerow([directory.name,plate,family,int(np.sum(mask)),errors])
                    if errors:
                        raise AssertionError(f'{directory.name}/{plate}/{family}: retained-value errors')


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('input',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    check(args.input,args.output)
