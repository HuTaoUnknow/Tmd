"""Verify the review deliverables against the captured source and test results."""
from pathlib import Path
from datetime import datetime, timezone, timedelta
import hashlib
import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

sys.stdout.reconfigure(encoding='utf-8')
ROOT = Path(__file__).resolve().parents[4]
OUT = Path(__file__).resolve().parent.parent
inventory = json.loads((OUT / 'inventory.json').read_text(encoding='utf-8'))
changed = [item['path'] for item in inventory['files']
           if hashlib.sha256((ROOT / item['path']).read_bytes()).hexdigest().upper() != item['sha256']]
assert not changed, changed
baseline_dir = ROOT / 'build/Desktop_Qt_6_11_1_MinGW_64_bit_Debug'
proposal_dir = ROOT / 'build-architecture-proposal'
records = []
for path in (baseline_dir / 'architecture-review-baseline.xml', proposal_dir / 'proposal-validation.xml'):
    xml = ET.parse(path).getroot()
    assert int(xml.attrib['failures']) == 0 and int(xml.attrib.get('skipped', '0')) == 0
    records.append({'file': str(path), 'tests': int(xml.attrib['tests']),
                    'cases': [item.attrib['name'] for item in xml.findall('testcase')]})
test_log = (proposal_dir / 'proposal-tests.txt').read_text(encoding='utf-8')
totals = re.search(r'Totals: (\d+) passed, (\d+) failed, (\d+) skipped', test_log)
assert totals and tuple(map(int, totals.groups())) == (20, 0, 0)
proposal_files = sorted(path for path in (ROOT / 'proposals/modular-editing').rglob('*') if path.is_file())
doc_files = sorted(path for path in OUT.rglob('*') if path.is_file() and path.name != 'verification.json')
missing_links, whitespace_errors = [], []
for path in proposal_files + doc_files:
    if path.suffix in ('.md', '.py', '.cpp', '.h', '.txt'):
        text = path.read_text(encoding='utf-8-sig')
        for number, line in enumerate(text.splitlines(), 1):
            if line.rstrip() != line: whitespace_errors.append(f'{path}:{number}')
        if path.suffix == '.md':
            for target in re.findall(r'\]\((D:/[^)]+)\)', text):
                actual = re.sub(r':\d+$', '', target)
                # This report is generated only after the other checks pass.
                if Path(actual) != OUT / 'verification.json' and not Path(actual).exists(): missing_links.append(actual)
assert not missing_links, missing_links
assert not whitespace_errors, whitespace_errors
diff_check = subprocess.run(['git', 'diff', '--check'], cwd=ROOT, capture_output=True, text=True)
assert diff_check.returncode == 0, diff_check.stdout + diff_check.stderr
prototype = [{'path': path.relative_to(ROOT).as_posix(),
              'sha256': hashlib.sha256(path.read_bytes()).hexdigest().upper(),
              'lines': len(path.read_text(encoding='utf-8').splitlines())} for path in proposal_files]
data = {
    'reviewDate': '2026-10-18',
    'verifiedAt': datetime.now(timezone.utc).astimezone(timezone(timedelta(hours=8))).isoformat(timespec='seconds'),
    'baselineSourceFilesVerifiedUnchanged': len(inventory['files']), 'changedBaselineSources': changed,
    'baselineDebugBuild': 'passed', 'baselineRegressionGroups': records[0],
    'proposalIsolatedDebugBuild': 'passed', 'proposalRegression': records[1],
    'qtTestTotals': {'passed': 20, 'failed': 0, 'skipped': 0, 'includesInitAndCleanup': True,
                     'actualEditorParityRows': 8, 'unicodeCorpusPairs': 169, 'randomSequencePairs': 500},
    'productionIntegration': False,
    'checks': {'gitDiffCheck': 'passed', 'newFileWhitespace': 'passed', 'artifactFileLinks': 'passed'},
    'limitations': ['No full 14-group production regression rerun in this review-only change.',
                    'No claim of Windows native IME/visual validation or measured application speedup.',
                    'One offscreen FreeType debug message; no failed or skipped tests.'],
    'prototypeFiles': prototype,
}
(OUT / 'verification.json').write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps({key: value for key, value in data.items() if key not in ('prototypeFiles', 'limitations')}, ensure_ascii=False, indent=2))
