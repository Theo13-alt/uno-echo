"""Collect the actual PE dependencies; no Qt installation is needed to run the result."""
from pathlib import Path
import os
import shutil
import zipfile
import argparse
import pefile
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, default=root / 'dist/UNO echo')
parser.add_argument('--build-dir', type=Path, default=root / 'build')
parser.add_argument('--qt-dir', type=Path, default=os.environ.get('ECHO_QT'))
parser.add_argument('--runtime-dir', type=Path, action='append', default=[])
args = parser.parse_args()
if args.qt_dir is None:
    parser.error('Pass --qt-dir or set ECHO_QT to the Qt 6 MSVC x64 prefix')
qt = args.qt_dir.resolve()
out = args.output.resolve()
if out.exists() and any(out.iterdir()):
    parser.error('Output directory must be empty; choose a fresh --output directory')
out.mkdir(parents=True, exist_ok=True)
runtime_dirs = list(args.runtime_dir)
vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
if vswhere.exists() and not runtime_dirs:
    installation = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    if installation:
        redist = Path(installation) / 'VC/Redist/MSVC'
        runtime_dirs = sorted(redist.glob('*/x64/Microsoft.VC*.CRT'), reverse=True)
search = [qt / 'bin', *runtime_dirs]
system = Path(os.environ['SystemRoot']) / 'System32'
queue = []
def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    queue.append(target)
copy(args.build_dir / 'UNO echo.exe', out / 'UNO echo.exe')
plugin_roots = (qt / 'plugins', qt / 'Qt6/plugins')
plugins = next((folder for folder in plugin_roots if (folder / 'platforms/qwindows.dll').is_file()), None)
if plugins is None:
    parser.error('Qt plugins were not found under plugins/ or Qt6/plugins/')
for plugin in ('platforms/qwindows.dll', 'iconengines/qsvgicon.dll', 'imageformats/qsvg.dll', 'tls/qschannelbackend.dll', 'tls/qcertonlybackend.dll'):
    copy(plugins / plugin, out / 'plugins' / plugin)
seen = set()
while queue:
    binary = queue.pop()
    if binary.name.lower() in seen:
        continue
    seen.add(binary.name.lower())
    pe = pefile.PE(str(binary))
    imports = list(getattr(pe, 'DIRECTORY_ENTRY_IMPORT', [])) + list(getattr(pe, 'DIRECTORY_ENTRY_DELAY_IMPORT', []))
    for entry in imports:
        name = entry.dll.decode('ascii')
        if name.lower().startswith(('api-ms-', 'ext-ms-')) or name.lower() in seen:
            continue
        source = next((folder / name for folder in search if (folder / name).exists()), None)
        if source:
            copy(source, out / name)
        elif not (system / name).exists():
            raise RuntimeError(f'Missing dependency: {name} required by {binary.name}')
    pe.close()
(out / 'qt.conf').write_text('[Paths]\nPlugins=plugins\n', encoding='utf-8')
shutil.copy2(root / 'README.md', out / 'README.md')
shutil.copytree(root / 'docs', out / 'docs', dirs_exist_ok=True)
shutil.copy2(root / 'BUILDING.md', out / 'BUILDING.md')
shutil.copy2(root / 'CREDITS.md', out / 'CREDITS.md')
shutil.copy2(root / 'LICENSE', out / 'LICENSE')
shutil.copytree(root / 'LICENSES', out / 'licenses', dirs_exist_ok=True)
for package in ('qtbase', 'qtsvg', 'qtscxml', 'brotli', 'bzip2', 'double-conversion', 'freetype', 'harfbuzz', 'icu', 'libpng', 'md4c', 'openssl', 'pcre2', 'zlib', 'zstd'):
    license_path = qt / 'share' / package / 'copyright'
    if license_path.exists():
        shutil.copy2(license_path, out / 'licenses' / (package + '.txt'))
for license_path in (root / 'data/echo').glob('*LICENSE*'):
    shutil.copy2(license_path, out / 'licenses' / license_path.name)
for license_path in (root / 'src/3rdparty/qhotkey').glob('LICENSE*'):
    shutil.copy2(license_path, out / 'licenses' / ('qhotkey-' + license_path.name))
with zipfile.ZipFile(out / 'source.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    for folder in ('src/3rdparty/qhotkey', 'data/echo', 'tools', 'tests', 'LICENSES', 'docs'):
        for path in (root / folder).rglob('*'):
            if path.is_file() and '.git' not in path.parts and path.name != '.git':
                archive.write(path, path.relative_to(root))
    for name in ('CMakeLists.txt', 'README.md', 'BUILDING.md', 'CREDITS.md', 'LICENSE', 'CHANGELOG.md', '.gitignore', '.gitattributes', 'src/compactmain.cpp', 'src/compactwindow.cpp', 'src/compactwindow.h', 'src/selection.cpp', 'src/selection.h', 'src/onlinetranslator.cpp', 'src/onlinetranslator.h'):
        archive.write(root / name, name)
archive = shutil.make_archive(str(out), 'zip', out.parent, out.name)
print(f'Packaged {len(seen)} binaries: {archive}')
