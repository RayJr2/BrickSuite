"""Limited algorithm sensitivity, never a search for looser acceptance tolerances."""
import argparse
import json
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'mesh-repair'))
from analyze import load, topology
from run_proof import run, executable

parser = argparse.ArgumentParser()
parser.add_argument('--bin', type=Path, required=True)
parser.add_argument('--cgal-bin', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--prior', type=Path, required=True)
args = parser.parse_args()
report = {}
for part in ['23422-studio','23422','6553','44375a','80910','3001']:
    source = args.out/'23422-studio.off' if part.endswith('studio') else args.prior/f'{part}-candidate.off'
    for variant,mode,epsilon in [('exact','fill-outer','0'),('simplify','fill-outer-simplify','.0004')]:
        key = f'{part}-{variant}'
        output = args.out/f'{key}.off'
        report[key] = run([executable(args.bin,'geogram_repair'),source,output,mode,epsilon],args.out/f'{key}.log')
        if report[key]['exit_code'] == 0:
            report[key]['topology'] = topology(load(output))
            report[key]['check'] = run([executable(args.cgal_bin,'cgal_repair'),output,args.out/f'{key}-inspected.off','inspect'],args.out/f'{key}-check.log')
        print(key, report[key], flush=True)
        (args.out/'sensitivity.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    # Upstream documents repeated intersection processing after double rounding.
    source = args.out/f'{part}-fill-outer.off'
    for iteration in [2,3]:
        key = f'{part}-repeat{iteration}'
        output = args.out/f'{key}.off'
        report[key] = run([executable(args.bin,'geogram_repair'),source,output,'fill-outer','0'],args.out/f'{key}.log')
        if report[key]['exit_code'] != 0:
            break
        report[key]['topology'] = topology(load(output))
        report[key]['check'] = run([executable(args.cgal_bin,'cgal_repair'),output,args.out/f'{key}-inspected.off','inspect'],args.out/f'{key}-check.log')
        source = output
        print(key, report[key], flush=True)
        (args.out/'sensitivity.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
