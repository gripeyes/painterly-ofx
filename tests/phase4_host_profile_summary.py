"""Associate in-node stage measurements with actual Nuke Write cases."""
import argparse
import csv
import json
import re
import statistics
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument("log",type=Path)
parser.add_argument("output",type=Path)
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
records=[];current=None
for line in args.log.read_text().splitlines():
    if "PHASE4_PROFILE" in line:
        current=dict(re.findall(r"(\w+)=([^\s]+)",line))
    match=re.search(r"Writing .+/([^/]+)\.exr took",line)
    if match and current:
        current["case"]=match.group(1);records.append(current);current=None
fields=["analysis","hierarchy","field","transport","interaction","final","total"]
summary={"scope":"Actual Nuke stage timings; interaction is fused Spill weights/color law/alpha reconstruction. final is Amount/Mask/Mix, RGB/premult and alpha copy. Separate color-law time cannot be isolated from the fused GPU kernel.","cases":len(records),"groups":{}}
for law in [0,1]:
    for backend in [0,1]:
        rows=[r for r in records if re.search(r"law%d-backend%d-run[12]$"%(law,backend),r["case"])]
        if rows:
            key=("Linear" if law==0 else "Density")+(" Auto" if backend==0 else " CPU")
            summary["groups"][key]={f:statistics.median(float(r[f]) for r in rows) for f in fields}
summary["cold_analysis"]=[{k:r[k] for k in ["case","size"]+fields} for r in records if r["case"].endswith("C0-law0-backend1-run0")]
summary["scale_edits"]=[r for r in records if r["case"].endswith("scale-edit")]
(args.output/"host-stage-summary.json").write_text(json.dumps(summary,indent=2))
if records:
    with (args.output/"host-stages.csv").open("w",newline="") as f:
        writer=csv.DictWriter(f,fieldnames=list(records[0]));writer.writeheader();writer.writerows(records)
print(json.dumps(summary,indent=2))
