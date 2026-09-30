"""Inspect saved photographic hierarchy cuts; never alter processing fields."""
import argparse
import csv
import json
from pathlib import Path

import numpy as np
from PIL import Image

from phase4_contact_sheets import contact


def inspect(directory, output):
    output.mkdir(parents=True, exist_ok=True)
    rows=list(csv.DictReader((directory/'hierarchy-sweep.csv').open()))
    scales=sorted({int(row['scale']) for row in rows})
    count=max(int(row['plate']) for row in rows)+1
    source=np.asarray(Image.open(directory/'gate-a-reconstruction.ppm').convert('RGB'),float)
    summary={'scales':scales, 'plate_count':count, 'nested':True,
             'new_retained_pixels':0,
             'qualification':'Nested source-grid boundaries do not prove silhouette survival or artistic passage.'}
    for family in ('y','ab'):
        preceding=[None]*count
        previous_counts=[None]*count
        for scale in scales:
            influence=np.zeros(source.shape[:2])
            for plate in range(count):
                prefix='plate-'+chr(ord('A')+plate)
                retained=np.asarray(Image.open(directory/f'scale-{scale}'/(prefix+f'-{family}-retained.pgm')))>0
                alpha=np.asarray(Image.open(directory/(prefix+'-alpha.pgm')),float)/255
                influence+=alpha*retained
                if preceding[plate] is not None:
                    new=int(np.sum(retained & ~preceding[plate]))
                    summary['new_retained_pixels']+=new
                    summary['nested'] &= new==0
                row=next(r for r in rows if int(r['scale'])==scale and int(r['plate'])==plate)
                chunks=int(row[family+'_chunks'])
                if previous_counts[plate] is not None:
                    summary['nested'] &= chunks<=previous_counts[plate]
                previous_counts[plate]=chunks
                preceding[plate]=retained
            amount=np.clip(influence,0,1)[...,None]*.7
            overlay=source*(1-amount)+np.array([255,35,15])*amount
            Image.fromarray(np.uint8(np.clip(overlay,0,255))).save(output/f'{family}-scale-{scale}.png')
        contact([output/f'{family}-scale-{scale}.png' for scale in scales],
                output/(family+'-boundary-sweep.png'))
    (output/'hierarchy-metrics.json').write_text(json.dumps(summary,indent=2)+'\n')
    # Preserve the exact counts, not only a display rendering.
    (output/'hierarchy-sweep.csv').write_text((directory/'hierarchy-sweep.csv').read_text())
    return summary


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('directory',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    print(json.dumps(inspect(args.directory,args.output),indent=2))
