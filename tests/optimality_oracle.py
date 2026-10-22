#!/usr/bin/env python3
"""Independent raw syntax enumeration and Python-re word semantics.

This deliberately does not deduplicate by the C++ observation algebra, reuse
its weighted parser, or use a solution catalog.
"""
from pathlib import Path
import itertools,json,re,subprocess,sys
binary=str(Path(sys.argv[1] if len(sys.argv)>1 else Path(__file__).resolve().parents[1]/'regex-shortener').resolve())
maximum=6
E=[set() for _ in range(maximum+1)]
C=[set() for _ in E]; Q=[set() for _ in E]; A=[set() for _ in E]
E[0].add(''); C[0].add('')
suffixes=['*','+','?']+[f'{{{n}}}' for n in range(1000)]+[f'{{{n},}}' for n in range(100)]
suffixes += [f'{{{a},{b}}}' for a in range(10) for b in range(a,10)]
for size in range(1,maximum+1):
    if size==1:A[size].update(['0','1'])
    if size==4:A[size].add('[01]')
    if size>=2:A[size].update('('+s+')' for s in E[size-2])
    Q[size].update(A[size])
    for suffix in suffixes:
        before=size-len(suffix)
        if before>0:Q[size].update(s+suffix for s in A[before])
    C[size].update(Q[size])
    for left in range(1,size):
        C[size].update(a+b for a in C[left] for b in Q[size-left])
    E[size].update(C[size])
    for left in range(size):
        E[size].update(a+'|'+b for a in E[left] for b in C[size-left-1])
words=[''.join(w) for n in range(8) for w in itertools.product('01',repeat=n)]
matching={}; enumerated=0
for size,expressions in enumerate(E):
    for expression in expressions:
        pattern=re.compile(expression)
        signature=tuple(bool(pattern.fullmatch(w)) for w in words)
        matching.setdefault(signature,[]).append((size,expression));enumerated+=1
cases=['','0','00','01','0?','0*','0+','0|1','[01]','0|00','00*','0*1*','[01]*','(0|1){3}','[0-9]*','0{97}']
proved=0
for expr in cases:
    p=subprocess.run([binary,expr,'--prove-only','--prove-seconds','3','--prove-length','9','--quiet','--json','--no-catalog'],text=True,capture_output=True,check=True)
    out=json.loads(p.stdout)
    assert out['optimal'] is True,out
    assert out['optimality']['lower_bound']==out['after'],out
    assert out['optimality']['proven'] is True,out
    if set(re.sub(r'\{[^}]*\}','',out['regex']))<=set('01[]|()*+?'):
        signature=tuple(bool(re.fullmatch(out['regex'],w)) for w in words)
        # Finite observations can collide, e.g. 0{9} and 0{97} on short
        # words. Resolve every shorter collision by an exact product; never
        # mistake a sample collision for a counterexample to optimality.
        for size,candidate in matching.get(signature,[]):
            if size < out['after']:
                exact=subprocess.run([binary,'--equiv',candidate,out['regex'],'--seconds','10'],text=True,capture_output=True,timeout=12)
                assert exact.returncode==1,(out,candidate,exact.stdout,exact.stderr)
    for w in words:
        assert bool(re.fullmatch(expr,w))==bool(re.fullmatch(out['regex'],w)),(expr,out,w)
    proved+=1
for args in [ ['[01]*','--prove-seconds','0'], ['[01]*','--prove-length','2'],
              ['(0|11|100)*','--prove-seconds','0.000001'] ]:
    p=subprocess.run([binary,'--prove-only','--quiet','--json',*args],text=True,capture_output=True,check=True)
    out=json.loads(p.stdout);assert out['optimal'] is False,out
print(f'OK: {enumerated} independently generated raw expressions, {len(words)} words, {proved} shortest proofs and 3 incomplete/disabled checks.')
