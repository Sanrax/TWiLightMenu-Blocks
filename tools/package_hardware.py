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
                'This revision fixes incomplete SD reads in the handoff loaders and\n'
                'ARM9 cache synchronization for aligned/unaligned SD buffers. It also\n'
                'retains the earlier ARM9 reset-cache correction. Please check\n'
                'console-SD/Unlaunch game launches (including Mario Kart) and Settings\n'
                'return after saving a change. These reported hardware regressions still\n'
                'need device confirmation; emulator passes do not validate caching.\n\n'
                'Verified in melonDS 1.1: DSi SD + NAND discovery; Unlaunch SD boot; DSi/3DS\n'
                'themes; Settings round trip; argv/emulator arguments; bootstrap filesystem\n'
                'and FAT aliases; Mario Kart via retail bootstrap in DS flashcard mode.\n'
                'Internal-SD retail bootstrap stalled in this emulator, including a direct\n'
                'official-bootstrap control. The user has confirmed retail bootstrap and\n'
                'pico-loader gameplay input on DSpico hardware.\n\n'
                'Hardware feedback confirms the previous normal BOOT.NDS revision works.\n'
                'This build includes those fixes plus visible-entry banner loading,\n'
                'selected box-art caching, build-decoded photos and UI PNGs, shorter menu\n'
                'fades, shared translation parsing, batched font-map reads and cached,\n'
                'batched reads in the ARM7 handoff loaders. GRF reads are sequential,\n'
                'with padded GPU texture and palette upload buffers.\n'
                'In the DS/DLDI emulator fixture, the menu program reads in 0.40 s\n'
                'instead of 1.15 s. This is one loader phase, not a hardware cold-boot\n'
                'measurement; the fixture uses 512-byte clusters. The additional gain\n'
                'from batching is small with 32 KiB clusters in internal-SD emulation.\n'
                'A separate full-font fixture reduces cold font setup from 651 to\n'
                '112 ms in the emulator. This is not a hardware boot-time claim.\n'
                'See BLOCKSDS-PORT.md for scope and validation.\n\n'
                'The DSpico hardware log stopped while mounting internal SD. This build\n'
                'enables ARM7 SD access before probing the controller and bounds failed\n'
                'initialization waits. It also clears stale SD IPC sync state before\n'
                'ARM7 reset, fixing a reproduced DSi-mode DLDI handoff race.\n'
                'SD transfer r3 also corrects DMA word counts, waits for the complete\n'
                'buffer, bounds stalled transfers, and prevents the native SD interrupt\n'
                'handler from clearing flags needed by the custom polling driver.\n'
                'The user confirmed ALT and TEST boot and function on DSpico with r3.\n'
                'Power/navigation r4 routes the power button to firmware reboot instead\n'
                'of the SDK return-to-loader path, and serializes I2C status polling.\n'
                'Folder/page/device changes fade only the game/folder icon row;\n'
                'the surrounding interface stays steady, without a loading overlay.\n'
                'Startup fades directly into the prepared menu without a loading bar.\n'
                'DS Classic also uses compressed indexed images and faster fast fades.\n'
                'Settings/startup r5 reuses the decoded menu background and skips\n'
                'scanline waits while the display is fully hidden. Settings entry\n'
                'uses a fast fade; exit overlaps music/screen fades and removes\n'
                'the post-save wait and redundant redraws. The measured Settings\n'
                'exit phase is about 0.45 s versus 1.44 s in DS/DLDI emulation,\n'
                'excluding the following loader and menu initialization. Macro-mode\n'
                'status rendering now runs outside VBlank to fix an IRQ crash.\n'
                'Prepared-translations r6 moves translation parsing and escape\n'
                'processing into the build for the browser, Settings, DS Classic,\n'
                'title, manual and image viewer, with the original INI fallback.\n'
                'English language setup measures about 8 ms versus 54 ms in the\n'
                'browser and 12 ms versus 98 ms in Settings in DS/DLDI emulation.\n'
                'Clock/browsing r7 caches formatted clock/date text per RTC second.\n'
                'A 500-file directory scan measures about 99 ms versus 185 ms\n'
                'in the matching DS/DLDI fixture. Blinking, format changes,\n'
                'midnight and sleep recovery are retained. Hidden clock-row\n'
                'copies skip their scanline wait while visible updates retain it.\n'
                'Font/startup r8 compacts the bundled Default font maps without\n'
                'changing glyphs or spacing. DSi/debug-RAM glyph caches read only\n'
                'the 512-byte file pages needed for displayed text, retaining\n'
                'those pages across menu handoffs. With matching compact fonts,\n'
                'DSi/DLDI direct-entry initialization measures 1.04 -> 0.56 s\n'
                'for the browser and 0.64 -> 0.15 s for Settings. These exclude\n'
                'the preceding loader/title/SDK startup and are emulator figures.\n'
                'Audio/startup r9 uses a circular PCM queue with the same total\n'
                'capacity. It preloads several seconds and fills the remaining\n'
                'space during normal main-thread background operations. Audio\n'
                'setup measures about 24 ms versus 88 ms in the DSi fixture;\n'
                'browser initialization falls from about 557 to 489 ms. Intro\n'
                'loops, stereo, cached PCM, jingle delays and ARM7 fades remain.\n'
                'Asset-lookup r10 indexes immutable NitroFS graphics directories and\n'
                'opens textures/palettes by native ID, retaining normal stdio for\n'
                'external skins and preserving format/override precedence. Matching\n'
                'DSi/DLDI graphics setup measures 173 -> 69 ms; initialization falls\n'
                'from 489 -> 389 ms, excluding preceding loader/title/SDK work.\n'
                'These are emulator phase timings, not hardware cold-boot figures.\n'
                'The user has confirmed successful ALT power-button reboot on DSpico\n'
                'in DSi mode. Diagnostic/profiling builds live on the diagnostics branch.\n'
                'Entry files remain separate pending a consolidation compatibility audit.\n\n'
                'Useful console checks for this build: cold boot with splashes disabled;\n'
                'open game folders; scroll and change pages; DS Classic round trips;\n'
                'Settings return and lid reopening; argv/emulator launching. Compare\n'
                'on the same card, folder and settings as your previous build.\n')
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
