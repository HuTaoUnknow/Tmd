"""Record actual production integration results after the complete CTest run."""
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
BUILD = ROOT / 'build/Desktop_Qt_6_11_1_MinGW_64_bit_Debug'
backup = Path((ROOT / 'package/architecture-integration-latest.txt').read_text(encoding='utf-8-sig').strip())
assert backup.is_relative_to(ROOT / 'package')
before = json.loads((backup / 'before.json').read_text(encoding='utf-8-sig'))

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


xml_path = BUILD / 'modular-integration-full.xml'
xml = ET.parse(xml_path).getroot()
assert int(xml.attrib['tests']) == 15 and int(xml.attrib['failures']) == 0
assert int(xml.attrib.get('skipped', '0')) == 0
groups = []
for case in xml.findall('testcase'):
    name = case.attrib['name']
    log_path = BUILD / (name + '.txt')
    log = log_path.read_text(encoding='utf-8')
    match = re.search(r'Totals: (\d+) passed, (\d+) failed, (\d+) skipped', log)
    assert match, name
    passed, failed, skipped = map(int, match.groups())
    assert failed == 0, name
    groups.append({'name': name, 'seconds': float(case.attrib['time']),
                   'passed': passed, 'failed': failed, 'skipped': skipped, 'log': str(log_path)})
modular = next(group for group in groups if group['name'] == 'tmd_modular_editing')
assert modular['passed'] == 23 and modular['skipped'] == 0
for folder, forbidden in [('core', r'^[ \t]*#include\s*"(?:application|ui)/'),
                          ('application', r'^[ \t]*#include\s*(?:"ui/|<Q(?:Widget|TextEdit|TextBrowser|Dialog|MainWindow))')]:
    for path in (ROOT / folder).rglob('*'):
        if path.suffix in ('.cpp', '.h'):
            assert not re.search(forbidden, path.read_text(encoding='utf-8-sig'), re.M), str(path)
new_sources = [
    'application/editing/EditCommandRouter.h', 'application/editing/EditCommandRouter.cpp',
    'core/markdown/editing/TextPatch.h', 'core/markdown/editing/TextPatch.cpp',
    'core/markdown/analysis/MarkdownSnapshot.h', 'core/markdown/analysis/MarkdownSnapshot.cpp',
    'ui/document/QtTextEditAdapter.h', 'ui/document/QtTextEditAdapter.cpp',
    'tests/ModularEditingTests.cpp',
]
catalogue = json.loads((OUT / 'integrated/inventory.json').read_text(encoding='utf-8'))
for item in catalogue['files']:
    assert digest(ROOT / item['path']) == item['sha256'], item['path']
cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
for name in new_sources:
    text = (ROOT / name).read_text(encoding='utf-8')
    assert all(line == line.rstrip() for line in text.splitlines()), name
    if name.endswith('.cpp'): assert name in cmake, name
diff_check = subprocess.run(['git', 'diff', '--check'], cwd=ROOT, capture_output=True, text=True)
assert diff_check.returncode == 0, diff_check.stdout + diff_check.stderr
changed = [{'path': item['Path'], 'beforeSha256': item['SHA256'], 'afterSha256': digest(ROOT / item['Path'])}
           for item in before if digest(ROOT / item['Path']) != item['SHA256']]
result = {
    'verifiedAt': datetime.now(timezone.utc).astimezone(timezone(timedelta(hours=8))).isoformat(timespec='seconds'),
    'productionIntegration': True, 'scope': 'Edit routing, source undo adapter, UTF-16 patch and shared rendered-source analysis.',
    'debugBuild': 'passed', 'debugExecutableSha256': digest(BUILD / 'tree_md.exe'),
    'fullCTest': {'groups': len(groups), 'failed': 0, 'seconds': round(sum(group['seconds'] for group in groups), 3),
                  'qtTestSkipped': sum(group['skipped'] for group in groups), 'junit': str(xml_path)},
    'groups': groups, 'moduleRegressionPassed': modular['passed'],
    'layerIncludeChecks': 'passed', 'newSourceWhitespace': 'passed', 'gitDiffCheck': 'passed',
    'sourceCatalogueFilesVerified': len(catalogue['files']), 'newSourcesRegisteredInCMake': True,
    'backup': str(backup), 'changedPreexistingFiles': changed,
    'newSources': [{'path': name, 'sha256': digest(ROOT / name)} for name in new_sources],
    'deferred': ['Render/selection class extraction', 'Knowledge graph read-only input and relation storage split',
                 'Full directory migration', 'Native Windows IME/visual acceptance'],
}
(OUT / 'implementation-verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps({key: result[key] for key in ('verifiedAt', 'debugBuild', 'debugExecutableSha256', 'fullCTest', 'moduleRegressionPassed', 'layerIncludeChecks')}, ensure_ascii=False, indent=2))
