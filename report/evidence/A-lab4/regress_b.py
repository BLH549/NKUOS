#!/usr/bin/env python3
"""Re-run the existing B fixture and record the current integration status."""
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile

output = Path(__file__).resolve().parent
root = output.parents[2]
script = root / 'report/evidence/B-lab4/verify.sh'
with tempfile.TemporaryDirectory(prefix='nkuos-lab4-b-regression-') as temporary:
    work = Path(temporary)
    for mode in ('prepare', 'run'):
        with (output / ('B-regression-' + mode + '.log')).open('w') as log:
            subprocess.run(['bash', str(script), mode, str(work)], cwd=root,
                           stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                           check=True, timeout=60)
    shutil.copyfile(work / 'qemu.log', output / 'B-regression-qemu.log')
print('PASS: existing B fixture (PCB, stack, rollback, first entry, arguments)')

results = {}
for target in ('qemu', 'grade'):
    grade_directory = Path('/tmp/lab4_grade')
    if target == 'grade' and grade_directory.exists():
        results[target] = 'not run: grading directory already in use'
        continue
    with (output / ('make-' + target + '.log')).open('w') as log:
        process = subprocess.Popen(['make', target], cwd=root / 'code',
            stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
            start_new_session=True)
        try:
            results[target] = process.wait(timeout=45)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGTERM)
            process.wait(timeout=5)
            results[target] = 'timeout after 45 seconds'
    if target == 'grade' and grade_directory.exists():
        assert grade_directory.resolve() == Path('/tmp/lab4_grade')
        shutil.rmtree(grade_directory)
(output / 'integration-status.json').write_text(json.dumps(results, indent=2) + '\n')
print('Recorded integration status:', results)
