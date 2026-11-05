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
    output = args.output.resolve()
    dependencies = ['nds-bootstrap-release.nds', 'nds-bootstrap-hb-release.nds', 'release-bootstrap.ver']
    for path in [*(bootstrap / name for name in dependencies), cia]:
        if not path.is_file():
            parser.error(f'Missing build dependency: {path}')
    subprocess.run(['python3', 'tests/check_build.py'], cwd=ROOT, check=True)
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
        'validation': ['make -j6 all', 'make test', 'make package', 'make check-build'],
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
                'Verified in melonDS 1.1: DSi SD + NAND discovery; Unlaunch SD boot; DSi/3DS\n'
                'themes; Settings round trip; argv/emulator arguments; bootstrap filesystem\n'
                'and FAT aliases; Mario Kart via retail bootstrap in DS flashcard mode.\n'
                'Internal-SD retail bootstrap stalled in this emulator, including a direct\n'
                'official-bootstrap control. The user has confirmed retail bootstrap and\n'
                'pico-loader gameplay input on DSpico hardware.\n\n'
                'This revision addresses the reported scroll-sound interrupt deadlock,\n'
                'splash/cursor palette colors, bootstrap version display and Settings\n'
                'backlight restoration. It also reduces banner I/O and menu fade waits.\n'
                'These fixes still need hardware retesting, especially DSi-mode ALT boot\n'
                'through Pico-Launcher and scrolling through folders.\n\n'
                'Useful console tests: cold boot; Settings and folder navigation; retail\n'
                'bootstrap boot/save/relaunch; argv/emulator launching; internal DSi SD/NAND;\n'
                '3DS widescreen/reboot; pico-loader; ALT boot from Pico-Launcher; lid\n'
                'close/open in Settings. See BLOCKSDS-PORT.md.\n')
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
