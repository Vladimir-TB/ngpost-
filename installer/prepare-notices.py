"""Collect unmodified upstream license texts from verified release sources.

Inputs: artifacts/release-compliance/{qtbase,qtsvg}-everywhere-src-6.8.3,
par2-source and rar-license.html. See docs/RELEASE.md for provenance/downloads.
"""
from pathlib import Path
import json
import shutil

root = Path(__file__).resolve().parents[1]
sources = root / 'artifacts/release-compliance'
output = root / 'installer/notices'
output.mkdir(parents=True, exist_ok=True)
for module in ('qtbase', 'qtsvg'):
    source = sources / f'{module}-everywhere-src-6.8.3'
    destination = output / 'Qt-6.8.3' / module
    shutil.copytree(source / 'LICENSES', destination / 'LICENSES', dirs_exist_ok=True)
    for attribution in source.rglob('qt_attribution*.json'):
        target = destination / attribution.relative_to(source)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(attribution, target)
        entries = json.loads(attribution.read_text(encoding='utf-8'), strict=False)
        for entry in entries if isinstance(entries, list) else [entries]:
            files = entry.get('LicenseFile', [])
            if isinstance(files, str):
                files = [files]
            for name in files:
                license_file = (attribution.parent / name).resolve()
                relative = license_file.relative_to(source.resolve())
                target_license = destination / relative
                target_license.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(license_file, target_license)
par2 = output / 'par2cmdline-turbo-1.3.0'
par2.mkdir(exist_ok=True)
shutil.copyfile(sources / 'par2-source/COPYING', par2 / 'COPYING')
shutil.copyfile(sources / 'par2-source/README.md', par2 / 'README.md')
# Preserve notices embedded in third-party headers as well as the main GPL text.
for file in (sources / 'par2-source/parpar').rglob('*'):
    if file.is_file() and file.suffix in ('.h', '.c', '.cpp'):
        text = file.read_text(encoding='utf-8', errors='replace').lower()
        if any(word in text for word in ('copyright', 'license', 'licence')):
            target = par2 / file.relative_to(sources / 'par2-source')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(file, target)
shutil.copyfile(sources / 'rar-license.html', output / 'RAR-EULA-en.html')
print('Collected upstream Qt, par2 and RAR notices:', output)
