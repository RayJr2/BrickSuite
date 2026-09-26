"""Bounded child-process supervisor for experiments, never BrickSuite processes."""
import json
import os
from pathlib import Path
import subprocess
import time
import psutil


def durable_json(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix+'.writing')
    with temporary.open('w', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def bounded(command, directory, part, phase, wall_seconds=120, cpu_seconds=120,
            memory_mib=2048, output_mib=64):
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    log = directory/'worker.log'
    state_path = directory/'worker-state.json'
    state = {'part':part, 'phase':phase, 'status':'starting', 'exit_status':None,
             'timeout':False, 'termination_reason':None, 'command':list(map(str,command)),
             'diagnostic_path':str(log), 'output_directory':str(directory),
             'limits':{'wall_seconds':wall_seconds,'cpu_seconds':cpu_seconds,
                       'memory_mib':memory_mib,'output_mib':output_mib,'threads':1},
             'sample_interval_seconds':.02}
    durable_json(state_path,state)
    environment = dict(os.environ, OMP_NUM_THREADS='1', OPENBLAS_NUM_THREADS='1',
                       MKL_NUM_THREADS='1', NUMEXPR_NUM_THREADS='1')
    start = time.monotonic()
    peak_rss = peak_cpu = peak_output = 0
    reason = None
    with log.open('wb') as output:
        process = subprocess.Popen(list(map(str,command)),stdout=output,stderr=subprocess.STDOUT,
            env=environment,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        monitored = psutil.Process(process.pid)
        state.update(status='running',pid=process.pid)
        durable_json(state_path,state)
        while process.poll() is None:
            processes = [monitored]
            try:
                processes += monitored.children(recursive=True)
            except psutil.NoSuchProcess:
                pass
            rss = committed = cpu = 0
            for child in processes:
                try:
                    memory = child.memory_info()
                    rss += memory.rss
                    committed += getattr(memory,'private',memory.rss)
                    times = child.cpu_times()
                    cpu += times.user+times.system
                except psutil.NoSuchProcess:
                    pass
            written = sum(p.stat().st_size for p in directory.rglob('*') if p.is_file())
            peak_rss,peak_cpu,peak_output = max(peak_rss,rss),max(peak_cpu,cpu),max(peak_output,written)
            if time.monotonic()-start > wall_seconds:
                reason = 'wall_timeout'
            elif cpu > cpu_seconds:
                reason = 'cpu_limit'
            elif max(rss,committed) > memory_mib*1024**2:
                reason = 'memory_limit'
            elif written > output_mib*1024**2:
                reason = 'output_limit'
            if reason:
                for child in reversed(processes):
                    try:
                        child.kill()
                    except psutil.NoSuchProcess:
                        pass
                break
            time.sleep(.02)
        code = process.wait()
        output.flush()
        os.fsync(output.fileno())
    # Check final size too, including children that completed between polls.
    written = sum(p.stat().st_size for p in directory.rglob('*') if p.is_file())
    peak_output = max(peak_output,written)
    if reason is None and written > output_mib*1024**2:
        reason = 'output_limit'
    state.update(status='completed' if code==0 and reason is None else 'failed',exit_status=code,
                 timeout=reason=='wall_timeout',termination_reason=reason,
                 wall_seconds=time.monotonic()-start,sampled_cpu_seconds=peak_cpu,
                 sampled_peak_rss_mib=peak_rss/1024**2,peak_output_mib=peak_output/1024**2)
    durable_json(state_path,state)
    return state
