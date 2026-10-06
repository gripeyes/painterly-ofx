"""Extract installed-host timing and cache evidence (not an artistic gate)."""
import json
import re
import sys
from pathlib import Path

records=[]
current={}
previous={}
for line in Path(sys.argv[1]).read_text().splitlines():
    if "ARTIST_BEGIN " in line:
        current={"control":line.split("ARTIST_BEGIN ",1)[1]}
    elif "PHASE4_PROFILE " in line:
        values=dict(re.findall(r"(\w+)=([^ ]+)",line))
        current["node_ms"]={k:float(values[k]) for k in
            ["analysis","hierarchy","field","transport","interaction","final","total"]}
        counters={k:list(map(int,values[k].split('/'))) for k in
            ["builds","source_builds","cuts","interactions","supports","fields"]}
        current["cache_counts"]=counters
        current["rebuilt"]={k:[v-(previous.get(k,[0]*len(a))[i]) for i,v in enumerate(a)]
                            for k,a in counters.items()}
        previous=counters
        current["metal"]=int(values["metal"])
    elif "ARTIST_END " in line:
        current["host_seconds_including_write"]=float(line.rsplit(' ',1)[1])
        records.append(current)
Path(sys.argv[2]).write_text(json.dumps({"cache_count_order":{
    "builds":"automatic / hierarchy / field / transport",
    "supports":"Y / AB", "fields":"Y / AB"},"records":records},indent=2))
print(json.dumps(records,indent=2))
