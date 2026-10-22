#!/usr/bin/env python3
"""End-to-end tests against Python's independent fullmatch implementation."""
import itertools
import json
import os
from pathlib import Path
import random
import re
import signal
import subprocess
import sys
import time
import tempfile
import concurrent.futures

BIN = str(Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'regex-shortener').resolve())
COUNT = int(os.environ.get('RX_FUZZ_CASES', '120'))


def call(*args, codes=(0,), timeout=15):
    p = subprocess.run([BIN, *([] if '--db' in args else ['--no-catalog']), *args], text=True, capture_output=True, timeout=timeout)
    assert p.returncode in codes, (args, p.returncode, p.stdout, p.stderr)
    return p


def optimize(expr, seconds='0.035', *extra):
    p = call(expr, '--seconds', seconds, '--quiet', '--json', *extra)
    j = json.loads(p.stdout)
    assert j['equivalent'] is True, j
    assert j['after'] <= j['before'], j
    assert j['after'] == len(j['regex']), j
    return j


def words(alphabet='012', limit=5):
    for n in range(limit + 1):
        yield from map(''.join, itertools.product(alphabet, repeat=n))


def assert_language(a, b, samples):
    pa, pb = re.compile(a), re.compile(b)
    for w in samples:
        assert bool(pa.fullmatch(w)) == bool(pb.fullmatch(w)), (a, b, w)


# Syntax accepted by the task. Whole-word semantics, all ten digits, epsilon.
valid = ['0', '00|111|0000', '00(0|1)', '(01)*', '110?110?',
         '[145]', '[2-46-9]', '0{0}', '1{2}', '(01){2,5}', '2{3,}',
         '(0{2})*', '(0?){3}', '(0{2,3}){2,4}', '(0{2,3})*',
         '([01]+|2?)*', '(0|1)?', '', '()', '()+', '(){2}', '0|', '|0', '[0-9]']
for expr in valid:
    j = optimize(expr)
    assert_language(expr, j['regex'], words())

# Strict rejection rather than silently interpreting a different regex dialect.
invalid = ['0{2}*', '0++', '0?*', '[^0]', '[9-0]', '[]', '[0-]', '.',
           '^0$', r'\d', '(0', '0)', '0{,2}', '0{3,2}', '0{2', '[01',
           'a', '0 1', '*0', '0{99999999999999999,88888888888888888}']
for expr in invalid:
    call(expr, '--seconds', '0.01', codes=(2,))

# Exact equivalence and SHORTEST separating words, including the empty word.
equivalent = [('(0|1)*', '[01]*'), ('(00)*', '(0{2})*'),
              ('(0{2,3})*', '(00|000)*'), ('(0?){3}', '0{0,3}'),
              ('(01|001|0001)*', '(00?0?1)*'), ('0*1*|0*1*0*1*', '0*1*0*1*')]
for a, b in equivalent:
    j = json.loads(call('--equiv', a, b, '--json').stdout)
    assert j['equivalent'] and j['witness'] is None
for a, b, witness in [('0*', '0+', ''), ('(00)*', '0*', '0'),
                      ('[01]*', '[0-9]*', '2'), ('0', '1', '0'),
                      ('(0{2,3})*', '0*', '0')]:
    j = json.loads(call('--equiv', a, b, '--json', codes=(1,)).stdout)
    assert not j['equivalent'] and j['witness'] == witness, j

# Real shortening rather than an implementation that always returns its input.
for a, maxlen in [('000000000000', 5), ('00|01|10|11', 7),
                  ('(01|001|0001)*', 9), ('0*1*|0*1*0*1*', 8),
                  ('(0|1)*', 5), ('[0123456789]', 5),
                  ('(01)(01)(01)(01)(01)', 7), ('((0*)*)*', 2),
                  ('0{4}|0{5}|0{6}', 6)]:
    j = optimize(a, '0.35')
    assert len(j['regex']) <= maxlen, (a, j)

# Regression for the user's example and its whole rewrite family. These
# improvements must work without enumerative synthesis or lucky random orders.
new_cases = [
    ('5|[1-9][0-9]*[05]',16), ('7|[1-9][0-9]*[07]',16),
    ('[1-9][0-9]*',10), ('[0-9]*[1-9]',10), ('[2-9][1-9]*',10),
    ('01(01|2)*',7), ('(01|2)*01',7), ('(01|23)(01|23|4)*',13),
    ('[1-9]+[0-9]*',10), ('[1-9]{1,4}[0-9]*',10),
    ('[0-9]*[1-9]{1,4}',10), ('1234567(1234567|8)*',12), ('[1-9]*[0-9]*',6), ('[0-9]*[1-9]*',6),
]
for expr, maximum in new_cases:
    j=optimize(expr,'0.08','--synth','0')
    assert len(j['regex'])<=maximum, (expr,j)
    call('--equiv',expr,j['regex'])
    assert_language(expr,j['regex'],words('0159',4))

# Independent randomized differential checks: parser, minimizer and optimizer.
rng = random.Random(463790)

def expression(depth=0):
    if depth > 2 or rng.random() < .33:
        return rng.choice(['0', '1', '2', '[01]', '[012]', '[2-4]', '0{0}'])
    kind = rng.randrange(3)
    if kind == 0:
        return '(' + expression(depth+1) + '|' + expression(depth+1) + ')'
    if kind == 1:
        return '(' + expression(depth+1) + expression(depth+1) + ')'
    return '(' + expression(depth+1) + ')' + rng.choice(['*', '+', '?', '{2}', '{0,3}', '{2,3}', '{2,}'])

samples = list(words('0123', 4))
for i in range(COUNT):
    expr = expression()
    j = optimize(expr)
    assert_language(expr, j['regex'], samples)
    # Test the public acceptance command, not just equivalence to the output.
    for w in rng.sample(samples, 2):
        p = call('--match', expr, w, codes=(0, 1))
        assert (p.stdout.strip() == 'true') == bool(re.fullmatch(expr, w)), (expr, w, p.stdout)
    if i % 20 == 19:
        print(f'Zufallstests: {i+1}/{COUNT}', flush=True)

# Real SL-haaste task, checked against its direct substring definition.
expr = (Path(__file__).resolve().parents[1] / 'examples/avoid-101010.txt').read_text().strip()
j = optimize(expr, '0.15')
pat = re.compile(j['regex'])
for word in words('01', 12):
    assert bool(pat.fullmatch(word)) == ('101010' not in word), (j, word)

# Limits must fail closed: huge counts preserve exactly the original language.
for expr, extra in [('0{10000000000000000000000000000000000001}', ()), ('0{1000000000}', ()), ('0{200}', ('--max-states', '4')),
                    ('(01)*', ('--max-nfa', '2'))]:
    j = optimize(expr, '0.05', *extra)
    assert j['regex'] == expr and j['proof'] == 'identity', j

# A large automaton with smaller optimizable sublanguages.
j = optimize('(0*1*|0*1*0*1*)[01]{50}', '0.3')
call('--equiv', j['input'], j['regex'], '--seconds', '5')

# Stdin, files, JSON, parallel execution, numeric option validation.
assert call('--version').stdout.strip() == 'regex-shortener 3.0'
p = subprocess.run([BIN, '--no-catalog', '--seconds', '0.05', '--quiet'], input='000000000000\n', text=True, capture_output=True, check=True)
assert len(p.stdout.strip()) <= 5
j = optimize('(01|001|0001)*', '0.15', '--jobs', '2')
assert_language('(01|001|0001)*', j['regex'], samples)
for extra in [('--seconds', 'nan'), ('--seconds', '0'), ('--jobs', '0'), ('--max-states', 'x')]:
    call('0', *extra, codes=(2,))

# Interrupt yields the best CERTIFIED result, not a discarded best-so-far.
p = subprocess.Popen([BIN, '--no-catalog', '((0|1)*)*', '--seconds', '60', '--quiet', '--json'], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
time.sleep(.1)
p.send_signal(signal.SIGINT)
out, err = p.communicate(timeout=10)
assert p.returncode == 0, (out, err)
j = json.loads(out)
assert j['equivalent'] and j['after'] <= 5, j
# Persistent semantic catalog: whole expressions, windows in a flattened
# concatenation, damaged records, and concurrent read/merge/write.
with tempfile.TemporaryDirectory() as temporary:
    db=str(Path(temporary)/'languages.db')
    call('--db',db,'--learn','1*0[01]*')
    call('--db',db,'--learn','[01]*0[01]*')
    j=optimize('9([01]*0[01]*)8','0.5','--db',db,'--synth','0')
    assert j['after']<=10 and j['catalog_hits']>0, j
    again=optimize('9([01]*0[01]*)8','0.08','--db',db,'--synth','0')
    assert again['after']<=10 and again['catalog_hits']>0, again
    assert_language('9([01]*0[01]*)8',j['regex'], ['98','908','9108','90108','9118','0908','90008'])
    call('--db',db,'--learn','0|1')
    record=next(line for line in Path(db).read_text().splitlines() if line.endswith('\t0|1'))
    corrupt=str(Path(temporary)/'corrupt.db')
    Path(corrupt).write_text(record.split('\t')[0]+'\t0\n')
    rejected=optimize('0|1','0.15','--db',corrupt)
    assert_language('0|1',rejected['regex'],words('012',3))
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        tasks=[pool.submit(call,'--db',db,'--learn',expr) for expr in ['0{20}','1{19}']]
        for task in tasks:task.result()
    text=Path(db).read_text()
    assert '\t0{20}\n' in text and '\t1{19}\n' in text
    disabled=str(Path(temporary)/'disabled.db')
    optimize('(0|1)*','0.05','--db',disabled,'--no-catalog')
    assert not Path(disabled).exists()

# Submitted regressions: exercise search independently of the shipped known
# solutions, then confirm catalog import and its exact verification.
regressions=json.loads((Path(__file__).resolve().parents[1]/'examples/regressions-v3.json').read_text())
limits=[51,200,37,50,44,49]
with tempfile.TemporaryDirectory() as temporary:
    for i,case in enumerate(regressions):
        call('--equiv',case['input'],case['known'])
        j=optimize(case['input'],'3' if i==0 else '1','--synth','0')
        assert j['after']<=limits[i], (case['name'],j)
        call('--equiv',case['input'],j['regex'])
        db=str(Path(temporary)/'known.db')
        # Seed location also works for binaries built outside the project.
        call('--db',db,'--learn',case['known'])
        known=optimize(case['input'],'0.3','--db',db,'--synth','0')
        assert known['after']<=len(case['known']), (case['name'],known)
        assert known['catalog_hits']>0, (case['name'],known)

print(f'OK: Syntax, exakte Aequivalenz, Kuerzungen, {COUNT} Zufallstests, Limits, Katalog, Parallelbetrieb und Strg+C.')
