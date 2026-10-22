#!/usr/bin/env python3
"""Official SL-haaste tasks, independently modeled as DFAs (no answer catalog).

Record lengths: https://www.ohjelmointiputka.net/slh/, retrieved 2026-10-22.
Task definitions: https://www.ohjelmointiputka.net/slh/<id>/.
Run from any directory: python3 sl_benchmark.py --seconds 3 --output results.json
"""
import argparse
from collections import deque
from concurrent.futures import ThreadPoolExecutor
import itertools
import json
from pathlib import Path
import re
import os
import selectors
import signal
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parent
RECORDS = [
    [10,14,7,15,15,7,13,19,16], [17,13,21,16,45,43,89,37,71],
    [14,13,17,25,50,18,9,21,13], [37,19,25,29,44,49,49,18,49],
    [69,126,31,42,49,51,63,24,108]]

def machine(alphabet, start, step, accept):
    states = [start]
    ids = {start: 0}
    rows = []
    for state in states:
        destinations = []
        for ch in '0123456789':
            nxt = step(state, ch) if state is not None and ch in alphabet else None
            if nxt not in ids:
                ids[nxt] = len(states)
                states.append(nxt)
            destinations.append(ids[nxt])
        rows.append([int(state is not None and accept(state)), *destinations])
    return rows

def literal_set(words, alphabet):
    words = set(words)
    prefixes = {w[:i] for w in words for i in range(len(w)+1)}
    return machine(alphabet, '', lambda s,c: s+c if s+c in prefixes else None,
                   lambda s:s in words)

def tasks():
    result = []
    def add(id, description, model):
        a,b = map(int,id.split('.'))
        result.append(dict(id=id, description=description, record=RECORDS[a-1][b-1],
                           source=f'https://www.ohjelmointiputka.net/slh/{id}/', dfa=model))
    def length(modulus, predicate, alphabet='01'):
        return machine(alphabet,0,lambda s,c:(s+1)%modulus,predicate)
    def capped_length(limit, predicate):
        return machine('01',0,lambda s,c:min(limit+1,s+1),predicate)
    def ends(same, empty=False):
        return machine('01','',lambda s,c:c+c if not s else s[0]+c,
                       lambda s:empty if not s else (s[0]==s[-1])==same)
    def runs(contains):
        def step(s,c):
            if s[1]==3:return s
            return (c,min(3,s[1]+1)) if s[0]==c else (c,1)
        return machine('01',('',0),step,lambda s:(s[1]==3)==contains)
    def counts(m,n,predicate):
        return machine('01',(0,0),lambda s,c:((s[0]+(c=='0'))%m,(s[1]+(c=='1'))%n),
                       lambda s:predicate(*s))
    def decimal(modulus):
        return machine('0123456789',-1,
                       lambda s,c:None if s==-1 and c=='0' else ((max(s,0)*10+int(c))%modulus),
                       lambda s:s==0)
    primes=[n for n in range(2,100) if all(n%d for d in range(2,int(n**.5)+1))]
    add('1.1','Gerade Wortlänge',length(2,lambda n:n==0))
    add('1.2','Ungerade Wortlänge',length(2,lambda n:n==1))
    add('1.3','Genau fünf Zeichen',capped_length(5,lambda n:n==5))
    add('1.4','Gleicher erster und letzter Buchstabe',ends(True))
    add('1.5','Verschiedener erster und letzter Buchstabe',ends(False))
    add('1.6','Unäre Länge durch fünf teilbar',length(5,lambda n:n==0,'0'))
    add('1.7','Unäre Länge nicht durch fünf teilbar',length(5,lambda n:n!=0,'0'))
    add('1.8','Drei gleiche Zeichen in Folge vorhanden',runs(True))
    add('1.9','Positive Dezimalzahl durch fünf teilbar',decimal(5))
    add('2.1','Genau zwei Nullen oder genau zwei Einsen',machine('01',(0,0),
        lambda s,c:(min(3,s[0]+(c=='0')),min(3,s[1]+(c=='1'))),lambda s:2 in s))
    add('2.2','Drittletztes Zeichen ist Null',machine('01','',lambda s,c:(s+c)[-3:],
        lambda s:len(s)==3 and s[0]=='0'))
    def odd_positions(s,c):
        parity, chosen = s
        if parity==0:
            if chosen and chosen!=c:return None
            chosen=c
        return (1-parity,chosen)
    add('2.3','An allen ungeraden Positionen steht dasselbe Zeichen',
        machine('01',(0,''),odd_positions,lambda s:True))
    add('2.4','Gleich viele Teilwörter 01 und 10',ends(True,True))
    def end_pairs(s,c):
        first,last=s
        return ((first+c)[:2],(last+c)[-2:])
    for id,same in [('2.5',True),('2.6',False)]:
        add(id,'Anfangs- und Endpaar sind '+('gleich' if same else 'verschieden'),
            machine('01',('',''),end_pairs,
                    lambda s,equal=same:len(s[0])==2 and ((s[0]==s[1])==equal)))
    add('2.7','Jede der Ziffern 1 bis 4 genau einmal',
        literal_set(map(''.join,itertools.permutations('1234')),'1234'))
    add('2.8','Dezimale Primzahlen kleiner als 100',literal_set(map(str,primes),'0123456789'))
    add('2.9','Unäre Primzahllängen kleiner als 100',literal_set(('0'*n for n in primes),'0'))
    add('3.1','Mindestens eine Null und eine Eins',machine('01',0,
        lambda s,c:s|(1<<int(c)),lambda s:s==3))
    add('3.2','Unäre Länge durch genau einen von 2 und 3 teilbar',
        length(6,lambda n:(n%2==0)!=(n%3==0),'0'))
    add('3.3','Keine drei gleichen Zeichen in Folge',runs(False))
    for ones in [1,2]:
        add(f'3.{3+ones}',f'Länge durch drei teilbar und genau {ones} Einsen',
            machine('01',(0,0),lambda s,c:((s[0]+1)%3,min(ones+1,s[1]+(c=='1'))),
                    lambda s:s==(0,ones)))
    add('3.6','Wortlänge ungleich fünf',capped_length(5,lambda n:n!=5))
    sums={0}
    for n in range(1,100):
        if n-7 in sums or n-8 in sums:sums.add(n)
    add('3.7','Unäre Länge ist Summe von Siebenen und Achten',machine('0',0,
        lambda s,c:min(42,s+1),lambda s:s in sums))
    def distance(s,c):
        parity,seen,found=s
        return (1-parity, seen|((1<<parity) if c=='0' else 0),
                found or (c=='0' and bool(seen&(1<<parity))))
    for id,positive in [('3.8',True),('3.9',False)]:
        add(id,'Zwei Nullen mit ungerade vielen Zeichen dazwischen: '+str(positive),
            machine('01',(0,0,False),distance,lambda s,p=positive:s[2]==p))
    def not_permutation(s,c):
        bit=1<<(int(c)-1)
        return 16 if s==16 or s&bit else s|bit
    add('4.1','Nicht jede der Ziffern 1 bis 4 kommt genau einmal vor',
        machine('1234',0,not_permutation,lambda s:s!=15))
    for id,predicate,description in [
        ('4.2',lambda a,b:a==0 or b==0,'Mindestens eine Ziffer hat gerade Häufigkeit'),
        ('4.3',lambda a,b:a==1 or b==1,'Mindestens eine Ziffer hat ungerade Häufigkeit'),
        ('4.4',lambda a,b:a==0 and b==0,'Beide Häufigkeiten gerade'),
        ('4.5',lambda a,b:a==1 and b==1,'Beide Häufigkeiten ungerade'),
        ('4.7',lambda a,b:a==0 and b==1,'Nullen gerade, Einsen ungerade')]:
        add(id,description,counts(2,2,predicate))
    add('4.6','Schaltjahre im Intervall 1800 bis 2200',literal_set(
        (str(y) for y in range(1800,2201) if y%4==0 and (y%100!=0 or y%400==0)),
        '0123456789'))
    forbidden='101010'
    add('4.8','Alle binären Wörter außer einem einzelnen Wort',
        machine('01','',lambda s,c:s+c if forbidden.startswith(s+c) else '#',
                lambda s:s!=forbidden))
    add('4.9','Positive Dezimalzahl durch vier teilbar',decimal(4))
    add('5.1','Nullen durch drei und Einsen durch zwei teilbar',counts(3,2,
        lambda a,b:a==0 and b==0))
    add('5.2','Nullen nicht durch drei und Einsen nicht durch zwei teilbar',counts(3,2,
        lambda a,b:a!=0 and b!=0))
    def balance(s,c):
        if s==3:return 3
        nxt=s+(1 if c=='0' else -1)
        return nxt if abs(nxt)<=2 else 3
    for id,bounded in [('5.3',True),('5.4',False)]:
        add(id,'Betrag der Präfix-Zähldifferenz immer höchstens zwei: '+str(bounded),
            machine('01',0,balance,lambda s,b=bounded:(s!=3)==b))
    add('5.5','Keine gleichen Nachbarziffern über 1 bis 4',machine('1234','',
        lambda s,c:None if s==c else c,lambda s:True))
    add('5.6','Unäre Länge weder durch fünf noch durch sieben teilbar',
        length(35,lambda n:n%5!=0 and n%7!=0,'0'))
    def sum_decimal(s,c):
        if s==-2:return None
        if s==-1:return -2 if c=='0' else int(c)
        return min(42,s*10+int(c))
    add('5.7','Dezimalzahl als Summe von Siebenen und Achten',machine('0123456789',-1,
        sum_decimal,lambda s:s==-2 or s in sums))
    def avoid(s,c):
        if s==len(forbidden):return s
        v=forbidden[:s]+c
        return max(i for i in range(len(forbidden)+1) if v.endswith(forbidden[:i]))
    add('5.8','Ein bestimmtes Teilwort kommt nirgends vor',machine('01',0,avoid,
        lambda s:s<len(forbidden)))
    add('5.9','Positive Dezimalzahl durch drei teilbar',decimal(3))
    return sorted(result,key=lambda t:tuple(map(int,t['id'].split('.'))))

def serialize(rows):
    return str(len(rows))+'\n'+'\n'.join(' '.join(map(str,row)) for row in rows)+'\n'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds',type=float,default=3)
    parser.add_argument('--synth',type=int,default=0)
    parser.add_argument('--workers',type=int,default=2)
    parser.add_argument('--binary',default=str(ROOT/'regex-shortener'))
    parser.add_argument('--baseline-binary',help='Optional previous version for paired comparison')
    parser.add_argument('--output',default=str(ROOT/'examples/benchmarks-sl-v7.json'))
    parser.add_argument('--input-suite',default=str(ROOT/'examples/sl-inputs.json'),help='Frozen automatically generated inputs for reproducible comparisons')
    parser.add_argument('--only',nargs='*',help='Task IDs to run')
    parser.add_argument('--stop-at-record',action='store_true',help='Benchmark harness interrupts after a published target is reached; never passes that length to the optimizer')
    args=parser.parse_args()
    fixed={r['id']:r['input'] for r in json.loads(Path(args.input_suite).read_text())} if args.input_suite else {}
    selected=[t for t in tasks() if not args.only or t['id'] in args.only]
    with tempfile.TemporaryDirectory() as tmp:
        adapter=str(Path(tmp)/'dfa-tools')
        subprocess.run(['g++','-std=c++17','-O2','-pthread',str(ROOT/'tests/dfa_tools.cpp'),
                        '-o',adapter],check=True)
        def run(task):
            data=serialize(task['dfa'])
            started=time.monotonic()
            if task['id'] in fixed:
                expr=fixed[task['id']]
                subprocess.run([adapter,expr],input=data,text=True,capture_output=True,timeout=35,check=True)
            else:
                gen=subprocess.run([adapter],input=data,text=True,capture_output=True,timeout=40)
                if gen.returncode:
                    return {**{k:v for k,v in task.items() if k!='dfa'},'error':gen.stderr.strip()}
                expr=gen.stdout.rstrip('\n')
            def optimize(binary):
                command=[str(Path(binary).resolve()),expr,'--no-catalog','--seconds',
                         str(args.seconds),'--synth',str(args.synth),'--json']
                if not args.stop_at_record:
                    p=subprocess.run(command+['--quiet'],text=True,capture_output=True,
                                     timeout=args.seconds+45,check=True)
                    result=json.loads(p.stdout)
                else:
                    p=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                    selector=selectors.DefaultSelector()
                    selector.register(p.stdout,selectors.EVENT_READ,'out')
                    selector.register(p.stderr,selectors.EVENT_READ,'err')
                    output=bytearray()
                    pending=''
                    interrupted=False
                    deadline=time.monotonic()+args.seconds+45
                    while selector.get_map():
                        if time.monotonic()>deadline:
                            p.kill()
                            raise TimeoutError(f"Optimizer deadline: {task['id']}")
                        for event,_ in selector.select(.25):
                            block=os.read(event.fileobj.fileno(),65536)
                            if not block:
                                selector.unregister(event.fileobj)
                                continue
                            if event.data=='out':
                                output.extend(block)
                                continue
                            pending+=block.decode('utf-8',errors='replace')
                            while '\n' in pending:
                                line,pending=pending.split('\n',1)
                                found=re.search(r'\] (\d+) Zeichen:',line)
                                reached=(found and int(found.group(1))<=task['record']) or (
                                    len(expr)<=task['record'] and line.startswith('Minimaler DFA:'))
                                if reached and not interrupted and p.poll() is None:
                                    try:
                                        p.send_signal(signal.SIGINT)
                                        interrupted=True
                                    except ProcessLookupError:
                                        pass
                    selector.close()
                    if p.wait()!=0:
                        raise RuntimeError(f"Optimizer failed: {task['id']}: {p.returncode}")
                    result=json.loads(output.decode())
                    result['harness_interrupted_after_record']=interrupted
                # Compare to the separately modeled task, not just to the input.
                subprocess.run([adapter,result['regex']],input=data,text=True,
                    capture_output=True,timeout=35,check=True)
                return result
            result={k:v for k,v in task.items() if k!='dfa'}
            result.update(input=expr,input_length=len(expr),options={
                'seconds':args.seconds,'synth':args.synth,'seed':1,'jobs':1,'catalog':False,
                'stop_at_record':args.stop_at_record})
            result['current']=optimize(args.binary)
            if args.baseline_binary:result['previous']=optimize(args.baseline_binary)
            result['matches_record']=result['current']['after']<=task['record']
            result['task_dfa_proof']=True
            result['wall_seconds']=round(time.monotonic()-started,3)
            print(f"{task['id']}: {len(expr)} -> {result['current']['after']} / record {task['record']}",flush=True)
            return result
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            results=list(pool.map(run,selected))
    output=dict(source='https://www.ohjelmointiputka.net/slh/',records_retrieved='2026-10-22',
        omitted={},
        methodology='Independent task DFA -> one deterministic GNFA order -> regex input; '
                    'no catalog; output checked exactly against the task DFA. '
                    'Record lengths are comparison targets, never synthesis inputs.',results=results)
    Path(args.output).write_text(json.dumps(output,ensure_ascii=False,indent=2)+'\n')
    valid=[r for r in results if 'error' not in r]
    print(f"{len(valid)} proved; {sum(r['matches_record'] for r in valid)} match published length; "
          f"{len(results)-len(valid)} preparation failures")

if __name__=='__main__':main()
