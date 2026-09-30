"""Assemble saved harness diagnostics without filtering processing data."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw


def contact(paths, target, columns=3):
    images=[Image.open(p).convert('RGB') for p in paths]
    if not images:
        return
    width=max(i.width for i in images)
    height=max(i.height for i in images)
    # Contact-sheet display reduction only; originals remain in the harness dir.
    scale=min(1, 320/width)
    w,h=int(width*scale),int(height*scale)
    sheet=Image.new('RGB',(columns*w,((len(images)+columns-1)//columns)*(h+20)),(30,30,30))
    draw=ImageDraw.Draw(sheet)
    for n,(path,image) in enumerate(zip(paths,images)):
        x,y=(n%columns)*w,(n//columns)*(h+20)
        draw.text((x+3,y+3),path.stem,fill='white')
        image.thumbnail((w,h))
        sheet.paste(image,(x,y+20))
    sheet.save(target)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('input',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    for pattern,name in [('plate-?-alpha.pgm','public-alpha'),
                         ('plate-?-support-y.pgm','support-y'),
                         ('plate-?-support-ab.pgm','support-ab'),
                         ('plate-?-appearance.ppm','public-appearance'),
                         ('latent-??.pgm','latent-alpha'),
                         ('latent-??-appearance.ppm','latent-appearance'),
                         ('plate-?-y-chunks.ppm','y-chunks'),
                         ('plate-?-ab-chunks.ppm','ab-chunks'),
                         ('plate-?-y-retained.pgm','y-retained'),
                         ('plate-?-ab-retained.pgm','ab-retained'),
                         ('plate-?-y-removed.pgm','y-removed'),
                         ('plate-?-ab-removed.pgm','ab-removed'),
                         ('plate-?-source-gradient.pgm','source-gradients'),
                         ('plate-?-simplified-gradient.pgm','simplified-gradients'),
                         ('plate-?-primitive.pgm','primitive-selection'),
                         ('plate-?-fit-error.pgm','fit-errors')]:
        contact(sorted(args.input.glob(pattern)),args.output/(name+'.png'))
    for pattern,name in [('plate-?-broad-y-influence.pgm','broad-y-influence'),
                         ('plate-?-broad-ab-influence.pgm','broad-ab-influence')]:
        contact(sorted(args.input.glob(pattern)),args.output/(name+'.png'))
    for name in ['gate-a-reconstruction','synthesized-composite','pre-spill','post-spill','no-interior','mean-only']:
        path=args.input/(name+'.ppm')
        if path.exists():
            Image.open(path).save(args.output/(name+'.png'))
    paired=[args.input/(name+'.ppm') for name in
            ['gate-a-reconstruction','no-interior','synthesized-composite']]
    if all(p.exists() for p in paired):
        contact(paired,args.output/'interior-comparison.png')
    paired=[args.input/(name+'.ppm') for name in
            ['gate-a-reconstruction','mean-only','synthesized-composite']]
    if all(p.exists() for p in paired):
        contact(paired,args.output/'first-moment-comparison.png')
    difference=args.input/'interior-difference.png'
    if difference.exists():
        Image.open(difference).save(args.output/difference.name)
    for path in args.input.glob('*-y-direction-error-*.png'):
        Image.open(path).save(args.output/path.name)
    for name in ['gate-a.csv','poisson.csv','hierarchy.csv',
                 'component-occupancy.csv','component-correlation.csv']:
        path=args.input/name
        if path.exists():
            (args.output/name).write_text(path.read_text())
