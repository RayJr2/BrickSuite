"""Provenance-only replay of saved Geogram passes, then bounded residual workers."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import numpy as np
import trimesh
from worker import bounded, durable_json


def require_success(state):
    if state['status']!='completed':
        raise RuntimeError(f"Worker failed; see {state['diagnostic_path']}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--geogram-bin',type=Path,required=True)
    parser.add_argument('--cgal-bin',type=Path,required=True)
    parser.add_argument('--prior',type=Path,required=True)
    parser.add_argument('--geogram-results',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--obj',type=Path,required=True)
    parser.add_argument('--reference',type=Path,required=True)
    args = parser.parse_args()
    args.out = args.out.resolve()
    args.out.mkdir(parents=True,exist_ok=True)
    recorded = json.loads((args.geogram_results/'runs.json').read_text(encoding='utf-8'))
    for name,path in [('studio',args.obj),('bambu',args.reference)]:
        if hashlib.sha256(path.read_bytes()).hexdigest()!=recorded['inputs'][name]['sha256']:
            raise ValueError('Exact matched benchmark input changed: '+name)
    suffix = '.exe' if sys.platform=='win32' else ''
    geogram = (args.geogram_bin/('geogram_repair'+suffix)).resolve()
    checker = (args.cgal_bin/('cgal_repair'+suffix)).resolve()
    distance = (args.cgal_bin/('cgal_distance'+suffix)).resolve()
    summary = {'parts':{},'benchmark_hashes_verified':True,'normalization_mm_per_studio_unit':.4}
    for part in ['23422','6553','44375a','80910','3001']:
        folder = args.out/part
        folder.mkdir(exist_ok=True)
        original = (args.geogram_results/'23422-studio.off' if part=='23422' else args.prior/f'{part}-candidate.off').resolve()
        residual = (args.geogram_results/({'23422':'23422-studio-fill-outer','6553':'6553-repeat2','3001':'3001-outer'}.get(part,part+'-fill-outer')+'.off')).resolve()
        mode = 'outer' if part=='3001' else 'fill-outer'
        steps = 2 if part=='6553' else 1
        source_path = original
        lineage = None
        states = []
        for step in range(steps):
            job = folder/f'provenance-replay-{step+1}'
            output = job/'replay.off'
            sidecar = job/'lineage.json'
            state = bounded([geogram,source_path,output,mode,'.0004' if step==0 else '0',sidecar],job,part,'provenance replay')
            states.append(state)
            require_success(state)
            current = np.array(json.loads(sidecar.read_text(encoding='utf-8'))['input_face_plus_one'])
            if lineage is not None:
                selected = current>0
                current[selected] = lineage[current[selected]-1]
            lineage = current
            source_path = output
        # Added attributes must not change the saved residual geometry or face order.
        replay_mesh = trimesh.load_mesh(source_path,process=False)
        residual_mesh = trimesh.load_mesh(residual,process=False)
        if not np.array_equal(replay_mesh.vertices,residual_mesh.vertices) or not np.array_equal(replay_mesh.faces,residual_mesh.faces):
            raise ValueError('Provenance replay differs from saved residual; refusing to attach lineage')
        lineage_path = folder/'lineage.json'
        durable_json(lineage_path,{'input_face_plus_one':lineage.tolist(),'replay_identical_to_saved_residual':True})
        job = folder/'residual-analysis'
        job.mkdir(exist_ok=True)
        config = {'part':part,'original':str(original),'residual':str(residual),
                  'authoritative':str((args.prior/f'{part}-source.off').resolve()),
                  'ancestry':str((args.prior/f'{part}-ancestry.json').resolve()),
                  'bambu':str((args.geogram_results/'23422-bambu.off').resolve()),
                  'lineage':str(lineage_path),'output':str(job)}
        config_path = folder/'config.json'
        durable_json(config_path,config)
        state = bounded([sys.executable,Path(__file__).with_name('measure.py').resolve(),config_path],job,part,
                        'classify and constrain residual',wall_seconds=180,cpu_seconds=180)
        states.append(state)
        require_success(state)
        for label,path in [('constrained',job/'constrained.off'),('original',original)]:
            check_job = folder/f'exact-{label}'
            state = bounded([checker,path,check_job/'inspected.off','inspect'],check_job,part,'exact intersection check '+label)
            states.append(state)
            require_success(state)
        for label,target in [('authoritative',Path(config['authoritative'])),('original',original)]+([('bambu',Path(config['bambu']))] if part=='23422' else []):
            djob = folder/f'distance-{label}'
            state = bounded([distance,job/'constrained.off',target],djob,part,'global distance '+label)
            states.append(state)
            require_success(state)
        summary['parts'][part] = {'states':states,'config':config,'result':str(job/'measurements.json')}
        durable_json(args.out/'run-summary.json',summary)
        print(part,'completed',flush=True)


if __name__=='__main__':
    main()
