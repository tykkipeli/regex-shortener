#!/usr/bin/env python3
"""Reproduce the submitted benchmark with and without the language catalog."""
import argparse
import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument('--seconds',type=float,default=3)
parser.add_argument('--synth',type=int,default=0)
parser.add_argument('--binary',default=str(Path(__file__).resolve().parent/'regex-shortener'))
args=parser.parse_args()
cases=json.loads((Path(__file__).resolve().parent/'examples/regressions-v3.json').read_text())

def run(case,catalog,database):
    command=[args.binary,case['input'],'--seconds',str(args.seconds),'--synth',str(args.synth),'--quiet','--json']
    command+=['--db',database] if catalog else ['--no-catalog']
    result=subprocess.run(command,text=True,capture_output=True,check=True)
    return json.loads(result.stdout)

results=[]
with tempfile.TemporaryDirectory() as temporary,concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    for i,case in enumerate(cases):
        a=pool.submit(run,case,False,str(Path(temporary)/f'unused-{i}.db'))
        b=pool.submit(run,case,True,str(Path(temporary)/f'catalog-{i}.db'))
        entry={'name':case['name'],'input':case['input'],'known':case['known'],
               'options':{'seconds':args.seconds,'synth':args.synth,'jobs':1,'seed':1},
               'without_catalog':a.result(),'with_catalog':b.result()}
        results.append(entry)
        print(f"{case['name']}: {entry['without_catalog']['after']} / {entry['with_catalog']['after']}",file=sys.stderr,flush=True)
print(json.dumps(results,indent=2))
