#!/usr/bin/env python3
"""Package built menus for hardware tests, using an explicit bootstrap release."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda: file.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def merge(source, destination):
    shutil.copytree(source, destination, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('.gitkeep'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bootstrap', type=Path, required=True,
                        help='Extracted official release containing both NDS files and release-bootstrap.ver')
    parser.add_argument('--cia', type=Path, required=True, help='CIA generated from the current booter.nds')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/hardware-test/dist')
    args = parser.parse_args()
    bootstrap = args.bootstrap.resolve()
    cia = args.cia.resolve()
    entrypoint = ROOT / 'booter/booter.nds'
    output = args.output.resolve()
    dependencies = ['nds-bootstrap-release.nds', 'nds-bootstrap-hb-release.nds', 'release-bootstrap.ver']
    for path in [*(bootstrap / name for name in dependencies), cia, entrypoint]:
        if not path.is_file():
            parser.error(f'Missing build dependency: {path}')
    if entrypoint.read_bytes() not in cia.read_bytes():
        parser.error('CIA does not contain the rebuilt booter.nds; regenerate it before packaging')
    output.mkdir(parents=True, exist_ok=True)

    source_digest = hashlib.sha256()
    paths = git('ls-files', '-z', '--cached', '--others', '--exclude-standard').split(b'\0')
    for name in sorted(set(paths)):
        path = ROOT / name.decode()
        if name and path.is_file():
            source_digest.update(name + b'\0' + bytes.fromhex(sha256(path)))
    info = {
        'built_at_utc': datetime.now(timezone.utc).isoformat(),
        'source_commit': git('rev-parse', 'HEAD').decode().strip(),
        'working_tree_modified': bool(git('status', '--porcelain')),
        'source_files_sha256': source_digest.hexdigest(),
        'submodules': git('submodule', 'status').decode().strip(),
        'sdk': 'BlocksDS 1.24.0',
        'bootstrap_version': (bootstrap / 'release-bootstrap.ver').read_text().strip(),
        'bootstrap_sha256': {name: sha256(bootstrap / name) for name in dependencies},
        'cia_sha256': sha256(cia),
        'cia_entrypoint_sha256': sha256(entrypoint),
        'validation': ['CIA contains current booter.nds', '7z archive integrity'],
    }
    install = {
        'DSi': 'Merge the archive contents into the SD card root. Select BOOT.NDS in Unlaunch.\n'
               'The title/ folder is only used by existing hiyaCFW/SDNAND installations.\n',
        '3DS': 'Merge the archive contents into the SD card root. Launch TWiLight Menu++ normally.\n'
               'TWiLight Menu.cia contains the rebuilt entry point and can be installed with your usual CIA installer.\n',
        'Flashcard': 'Merge the archive contents into the flashcard microSD root and run BOOT.NDS.\n'
                     'For kernel autoboot, use only the Autoboot folder matching your actual flashcard.\n',
    }
    package = ROOT / '7zfile'
    archives = []
    with tempfile.TemporaryDirectory(prefix='staging-', dir=output) as temporary:
        staging = Path(temporary)
        for platform in install:
            stage = staging / platform
            stage.mkdir()
            merge(package / '_nds', stage / '_nds')
            merge(package / 'roms', stage / 'roms')
            merge(package / 'Multimedia/_nds', stage / '_nds')
            if platform == 'Flashcard':
                merge(package / 'Flashcard users', stage)
            else:
                merge(package / 'DSi&3DS - SD card users', stage)
                if platform == 'DSi':
                    merge(package / 'DSi - SD card users', stage)
                    merge(package / 'DSi - CFW users/SDNAND root', stage)
                else:
                    merge(package / '3DS - CFW users', stage)
                    shutil.copy2(cia, stage / 'TWiLight Menu.cia')
                    (stage / 'ntrboot.nds').unlink(missing_ok=True)
                for name in ['bootplg.srldr', 'gbaswitch.srldr']:
                    (stage / '_nds/TWiLightMenu' / name).unlink(missing_ok=True)
            for name in dependencies:
                shutil.copy2(bootstrap / name, stage / '_nds' / name)
            for directory in ['gamesettings', 'icons', 'boxart', 'extras/fonts', '3dsmenu/themes',
                              'dsimenu/themes', 'akmenu/themes', 'r4menu/themes']:
                (stage / '_nds/TWiLightMenu' / directory).mkdir(parents=True, exist_ok=True)
            (stage / '_nds/nds-bootstrap').mkdir(exist_ok=True)
            shutil.copy2(ROOT / 'LICENSE', stage / 'LICENSE-TWiLightMenu.txt')
            shutil.copy2(ROOT / 'docs/blocksds-port.md', stage / 'BLOCKSDS-PORT.md')
            (stage / 'BUILD-INFO.json').write_text(json.dumps(dict(info, platform=platform), indent=2) + '\n')
            (stage / 'README-HARDWARE-TEST.txt').write_text(
                'TWiLight Menu++ BlocksDS hardware-test build — ' + platform + '\n\n' + install[platform] + '\n'
                'This is a test build for an existing TWiLight Menu++ installation. Preserve your\n'
                'settings, saves, ROMs, custom launcher configs and emulator/add-on files. No\n'
                'settings.ini, per-game preferences, saves, ROMs, NAND or BIOS files are bundled.\n'
                'All menu themes are retained. The rebuilt image viewer and multimedia launch\n'
                'configs are included. Additional emulator add-ons remain separate.\n\n'
                'Bundled nds-bootstrap is the unmodified ' + info['bootstrap_version'] + '.\n'
                'Source: https://github.com/DS-Homebrew/nds-bootstrap (tag matching the version above).\n'
                'Menu source: the accompanying working tree identified in BUILD-INFO.json.\n\n'
                'This revision replaces the legacy runtime SD/NAND IPC Sync driver with\n'
                'native BlocksDS FIFO storage and TMIO interrupts. Stand-alone handoff\n'
                'loaders retain polling reads because they run with IRQs disabled.\n'
                'INI saves now close a temporary file before replacing the original\n'
                'and keep a backup for recovery. nds-bootstrap is unchanged.\n\n'
                'Original DSi/DS Classic entry fade timing is restored by default.\n'
                'Fast menu fades can be enabled in Settings. The DSi title bubble\n'
                'appears after the icon drop animation. Internal menu handoffs keep\n'
                'cleared VRAM hidden to avoid a black flicker between modules.\n'
                'Navigation retains icon-row fades without a loading overlay.\n\n'
                'Console detection now uses the SDK hardware flag, replacing a write probe\n'
                'that mistook mirrored retail DSi RAM for extra development-unit RAM.\n'
                'This caused bootstrap to erase its own ARM9 loader when clearing RAM.\n'
                'Start the normal BOOT.NDS once so the title module corrects the model\n'
                'saved in both menu and bootstrap settings before testing a game.\n\n'
                'The r15 console-SD menu and Settings checks passed on the tested DSi.\n'
                'The console-detection correction still needs hardware confirmation: test\n'
                'Mario Kart through Unlaunch, repeated Settings changes and B exits,\n'
                'and extended browsing in both DSi and 3DS themes. Check that settings\n'
                'persist after reboot. Also check lid reopening and flashcard launching.\n'
                'Keep a copy of your settings and saves before replacing an older build.\n\n'
                'In the local melonDS 1.1 setup, an unmodified BlocksDS default-ARM7\n'
                'control stalls on its first native SD write, as does this menu.\n'
                'Emulator results cannot establish native SD reliability on hardware.\n'
                'Earlier DSpico hardware checks passed bootstrap/pico-loader gameplay\n'
                'inputs and ALT power-button reboot. The DSi-mode bootstrap Quit Game\n'
                'issue is tracked upstream; no workaround is included here.\n\n'
                'See BLOCKSDS-PORT.md and docs/optimizations.md in the source repository\n'
                'for the implementation map, measurements and validation boundaries.\n'
                'Diagnostic code and agent tests live on the diagnostics branch.\n')
            (stage / 'version.txt').write_text(
                'TWiLight Menu++ BlocksDS hardware-test build\n'
                f'SDK: {info["sdk"]}\n'
                f'nds-bootstrap: {info["bootstrap_version"]}\n'
                f'Source base: {info["source_commit"]} (working tree modifications included)\n'
                f'Built: {info["built_at_utc"]}\n')
            sums = ''.join(f'{sha256(path)}  {path.relative_to(stage).as_posix()}\n'
                           for path in sorted(stage.rglob('*')) if path.is_file())
            (stage / 'SHA256SUMS').write_text(sums)
            archive = staging / f'TWiLightMenu-BlocksDS-{platform}.7z'
            subprocess.run(['7z', 'a', '-bd', '-mx=5', '-mmt=2', str(archive), '.'],
                           cwd=stage, check=True, stdout=subprocess.DEVNULL)
            subprocess.run(['7z', 't', '-bd', str(archive)], check=True, stdout=subprocess.DEVNULL)
            archives.append(archive)
            print(f'{archive.name}: {archive.stat().st_size:,} bytes; archive integrity passed', flush=True)
        # Publish only complete, validated archives. An interrupted compressor
        # must not leave a partial file under a hardware-download filename.
        (staging / 'SHA256SUMS').write_text(''.join(f'{sha256(path)}  {path.name}\n' for path in archives))
        (staging / 'BUILD-INFO.json').write_text(json.dumps(info, indent=2) + '\n')
        for path in [*archives, staging / 'BUILD-INFO.json', staging / 'SHA256SUMS']:
            path.replace(output / path.name)


if __name__ == '__main__':
    main()
