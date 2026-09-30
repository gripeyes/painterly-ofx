"""Assemble saved harness diagnostics without filtering processing data."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw


def contact(paths, target, columns=3, labels=None):
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
        draw.text((x+3,y+3),labels[n] if labels else path.stem,fill='white')
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
                         ('plate-?-synthesized.ppm','regional-plate-fields'),
                         ('plate-?-boundary.ppm','boundary-appearance'),
                         ('plate-?-y-transitions.pgm','y-transitions'),
                         ('plate-?-ab-transitions.pgm','ab-transitions'),
                         ('plate-?-y-value-rails.pgm','y-value-rails'),
                         ('plate-?-ab-value-rails.pgm','ab-value-rails'),
                         ('plate-?-side-values.ppm','curve-side-values'),
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
    for name in ['gate-a-reconstruction','synthesized-composite','pre-spill','post-spill','no-interior','mean-only','first-only']:
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
    paired=[args.input/(name+'.ppm') for name in
            ['gate-a-reconstruction','first-only','synthesized-composite']]
    if all(p.exists() for p in paired):
        contact(paired,args.output/'second-moment-comparison.png')
    difference=args.input/'interior-difference.png'
    if difference.exists():
        Image.open(difference).save(args.output/difference.name)
    for path in args.input.glob('*-y-direction-error-*.png'):
        Image.open(path).save(args.output/path.name)
    ablation=([args.input/'gate-a-reconstruction.ppm',args.input/'synthesized-composite.ppm']+
              [args.input/f'barrier-{cut}/synthesized-composite.ppm' for cut in [25,50,75]])
    if all(p.exists() for p in ablation):
        contact(ablation,args.output/'barrier-ablation-comparison.png',labels=
                ['Original','Second, fixed barriers','Remove cue <= 0.25','Remove cue <= 0.50','Remove cue <= 0.75'])
    eigen=([args.input/'gate-a-reconstruction.ppm']+
           [args.input/f'modes-{y}-{ab}/synthesized-composite.ppm' for y,ab in [(2,1),(4,2),(8,4),(12,6)]])
    if all(p.exists() for p in eigen):
        contact(eigen,args.output/'regional-eigen-sweep.png',labels=['Original','Y2 AB1','Y4 AB2','Y8 AB4','Y12 AB6'])
    boundary=[args.input/'gate-a-reconstruction.ppm',args.input/'exact-side/synthesized-composite.ppm',args.input/'broad-side/synthesized-composite.ppm']
    if all(p.exists() for p in boundary):
        contact(boundary,args.output/'boundary-appearance-comparison.png',labels=['Original','Exact side Y2 AB1','Broad side Y2 AB1'])
    transition=[args.input/'gate-a-reconstruction.ppm',args.input/'synthesized-composite.ppm']
    if (args.input/'transition-curves.csv').exists() and all(p.exists() for p in transition):
        contact(transition,args.output/'sparse-transition-comparison.png',columns=2,labels=['Corrected A3 reconstruction','Sparse value-curve field'])
    for plate in 'ABCDEFGH':
        for family in ['y','ab']:
            paths=sorted(args.input.glob(f'plate-{plate}-{family}-mode-*.pgm'),
                         key=lambda p:int(p.stem.rsplit('-',1)[1]))
            contact(paths,args.output/f'plate-{plate}-{family}-eigenmodes.png',columns=4)
    for name in ['gate-a.csv','poisson.csv','hierarchy.csv',
                 'component-occupancy.csv','component-correlation.csv','regional-modes.csv','regional-fits.csv','regional-boundaries.csv','transition-curves.csv','transition-solves.csv']:
        path=args.input/name
        if path.exists():
            (args.output/name).write_text(path.read_text())
