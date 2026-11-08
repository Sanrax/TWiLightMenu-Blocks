# Optimization map

These changes are separate from the [BlocksDS compatibility port](blocksds-port.md).
Paths below identify the implementation to inspect when adapting an optimization
to upstream; most algorithms do not require changing toolchains.

**Measurement scope:** paired melonDS 1.1 tests, with matching settings/assets
within each comparison. DS/DLDI and DSi/DLDI denote emulated execution modes.
Startup means ARM9 `main()` to the first interactive frame unless a narrower
phase is named; it excludes preceding loaders/title/SDK startup. Rows use
different baselines and must not be added into an overall speedup. Hardware
cold-boot gains have not been quantified. [Phase data](https://github.com/Sanrax/TWiLightMenu-Blocks/blob/diagnostics/docs/benchmarks/startup.json)
and the diagnostics branch's host tests preserve the comparison evidence without
BIOS, NAND or ROMs.

## Browsing and common work

| Change and affected behavior | Implementation | Improvement |
| --- | --- | --- |
| Load banners only for the seven visible icons; read version-sized banners/512-byte headers; cache selected box art and skip absent custom-icon directories. Preserve argv, NDZ, animated icons and title/classification before use. | [fileBrowse.cpp](../romsel_dsimenutheme/arm9/source/fileBrowse.cpp), [iconTitle.cpp](../romsel_dsimenutheme/arm9/source/iconTitle.cpp), [ndsmetadata.h](../universal/include/common/ndsmetadata.h). | 48-entry DS/DLDI first-page metadata: 358 → 88 ms. Ordinary DS banners: 9,152 → 2,112 bytes. |
| Keep eight reusable icon texture banks; prepare data on the main thread and publish to VBlank. Queue scroll-stop SFX outside the interrupt. | [iconHandler.cpp](../romsel_dsimenutheme/arm9/source/graphics/iconHandler.cpp), iconTitle.cpp, [graphics.cpp](../romsel_dsimenutheme/arm9/source/graphics/graphics.cpp), [sound.cpp](../romsel_dsimenutheme/arm9/source/sound.cpp). | Removes per-icon frame waits and renderer heap churn; fixes scrolling/FIFO freezes. No isolated timing claim. |
| Obtain FAT attributes during enumeration; pass directory lists by reference; avoid startup free-space scans and allocating extension checks. Wood queries free space only when displayed. | All three `romsel_*/arm9/source/fileBrowse.cpp` and `.h`, [stringtool.cpp](../universal/source/common/stringtool.cpp), [fat_ext.h](../universal/arm9/include/fat_ext.h). | Fewer per-entry path lookups/copies and no FAT free-space scan just to determine cluster size; not timed separately. |
| Cache formatted clock/date per RTC second; share calendar conversion; skip hidden clock-copy scanline waits. Refresh on formats, time jumps and sleep. | [clockText.h](../universal/include/common/clockText.h), [date.cpp](../romsel_dsimenutheme/arm9/source/date.cpp), graphics.cpp, [ThemeTextures.cpp](../romsel_dsimenutheme/arm9/source/graphics/ThemeTextures.cpp). | 500-entry DS/DLDI directory phase: 185 → 99 ms. 1,000 formatting pairs: 182 → 6.5 ms, excluding drawing/I/O. |
| Buffer INI reads in 4 KiB chunks and index keys rather than rescan/copy lines; preserve duplicate precedence, case, history and output formatting. | [inifile.cpp](../universal/source/common/inifile.cpp), [inifile.h](../universal/include/common/inifile.h). | Removes repeated full-file scans/allocations; no isolated startup timing. |

## Startup, assets and transitions

| Change and affected behavior | Implementation | Improvement |
| --- | --- | --- |
| Give six handoff loaders separate FAT-sector caches; batch contiguous clusters in reads up to 64 KiB, with fragmentation/alignment and single-sector fallbacks. | [bootloaderFatCache.h](../universal/include/common/bootloaderFatCache.h), [bootloaderFileRead.h](../universal/include/common/bootloaderFileRead.h), six loader `source/fat.c` callers. | DS/DLDI title binary-read phase with 512-byte clusters: 1,147 → 399 ms (cache), then 393 ms (batching). Batching adds little with 32 KiB clusters. |
| Reuse CPU images already staged for a DSi entry handoff. | [flashcard nds_loader_arm9.c](../booter_fc/arm9/source/nds_loader_arm9.c), [bootloader boot.c](../booter_fc/bootloader/source/boot.c). | Avoids rereading the same images after storage reset; no separate time measurement. |
| Build indexed photo/UI caches and convert palette colors once; retain external PNG decoding and malformed-cache fallback. Traverse photo rows directly. | [photo_index.cpp](../tools/photo_index.cpp), [indexedPhoto.h](../universal/include/common/indexedPhoto.h), [menuImage.h](../universal/include/common/menuImage.h), Texture.cpp, graphics.cpp and quickmenu graphics. | Default photo working data: about 127 → 32 KiB. DS/DLDI UI texture phase: 67 → 29 ms; battery textures: 40 → 25 ms. |
| Read GRF chunks sequentially, reuse file-size metadata, validate chunks, pad texture dimensions/palettes for GPU uploads. | [Texture.cpp](../romsel_dsimenutheme/arm9/source/graphics/Texture.cpp), [Texture.h](../romsel_dsimenutheme/arm9/source/graphics/Texture.h), ThemeTextures.cpp. | Avoids end/back seeks and repeated reads; fixes malformed/custom asset uploads. No isolated timing claim. |
| Index immutable NitroFS graphics directories once and open by file ID; cache missing extensions/directories, bound memory and reset on remount. External skins keep normal stdio/precedence. | [nitroFile.cpp](../universal/source/common/nitroFile.cpp), [nitroFile.h](../universal/include/common/nitroFile.h), Texture.cpp, [systemdetails.cpp](../universal/source/common/systemdetails.cpp). | DSi/DLDI graphics: 173 → 69 ms; browser startup: 489 → 389 ms. DSi-mode 3DS UI startup: 440 → 356 ms. Includes a version-specific SDK descriptor fix; see port notes. |
| Reuse the decoded initial top background and skip scanline waits only while master brightness hides the destination. | ThemeTextures.cpp, graphics.cpp, [browser main](../romsel_dsimenutheme/arm9/source/main.cpp). | DS/DLDI browser startup: 815 → 749 ms (DSi UI), 749 → 682 ms (3DS UI), in that comparison's fixture. Visible updates keep their safe scanline wait. |
| Remove startup/navigation progress overlays and fixed fade waits; retain nine-frame (~150 ms) icon-row-only navigation fades and keep surrounding UI steady. | fileBrowse.cpp, graphics.cpp, browser main, [title main](../title/arm9/source/main.cpp). | Removes redundant blank/loading frames; transitions continue servicing audio. Animation duration excludes directory I/O. |
| Use indexed/RLE built-in DS Classic images and palette conversion. The optional **Fast menu fades** setting shortens DSi/Classic entry fades; original timing is the default. DSi title bubbles wait for the icon drop to finish. | [quickmenu Makefile](../quickmenu/Makefile), [Classic graphics.cpp](../quickmenu/arm9/source/graphics/graphics.cpp), [Classic main](../quickmenu/arm9/source/main.cpp), fileBrowse.cpp, menuImage.h/photo_index.cpp. | DS/DLDI Classic startup: 1,133 → 815 ms from images; 564 ms with the shorter fade. These earlier measurements used fast fades. |
| Overlap Settings screen/music fades, use the fast entry fade and remove redundant exit redraws/post-save delay; keep all settings saves. | [settingsgui.cpp](../settings/arm9/source/settingsgui.cpp), [settingsgui.h](../settings/arm9/source/settingsgui.h), [soundeffect.h](../settings/arm9/source/soundeffect.h), browser main. | DS/DLDI Settings exit phase: 1,438 → 447 ms, excluding the following loader/menu initialization. |

## Language, fonts and audio

| Change and affected behavior | Implementation | Improvement |
| --- | --- | --- |
| Share linear escape decoding; prepare bundled translations as validated `LNG1` packs during the build. Retain INI fallback, missing/blank defaults, UTF-8 and live language selection. | [languageEscape.h](../universal/include/common/languageEscape.h), [language_pack.cpp](../tools/language_pack.cpp), [languagePack.cpp](../universal/source/common/languagePack.cpp), [languagepacks.mk](../universal/languagepacks.mk), six apps' `language.cpp`. | English DS/DLDI language setup: browser 54 → 8 ms; Settings 98 → 12 ms. About 30/23 KB less linked browser/Settings ARM9 code. |
| Share bounded NFTR map reading/lookup, batch mapping reads and compact only bundled Default maps using existing NFTR encodings. Preserve every glyph, width and mapping. | [nftrFontMap.h](../universal/include/common/nftrFontMap.h), component `FontGraphic.cpp`, [compact_nftr.py](../tools/compact_nftr.py), bundled Default `.nftr` assets. | DS/DLDI cold font setup: 651 → 112 ms before compaction; compaction then ~113 → 95 ms. 62 → 29 map blocks and 19,536 bytes saved per Default font. |
| Fill DSi/debug-RAM glyph caches in 512-byte file pages as text needs them; retain pages across program handoffs and track buffer ownership. Keep the smaller DS cache. | [nftrGlyphPages.h](../universal/include/common/nftrGlyphPages.h), title/Settings/Classic/browser/manual/imageview `FontGraphic.cpp` and `.h`. | DSi/DLDI font phase: browser 569 → 78 ms; Settings 562 → 69 ms. Startup: 1,042 → 557 ms and 640 → 150 ms respectively, with matching compact fonts. |
| Load built-in launch/jingle SFX on demand, preload navigation/lid sounds and retain the memory loader for long custom-skin paths. | [sound.cpp](../romsel_dsimenutheme/arm9/source/sound.cpp), [soundbank.h](../universal/include/common/soundbank.h). | Initial built-in sample data: 435,500 → 183,424 bytes. No isolated startup-time improvement claimed. |
| Replace copying PCM halves with a circular queue; preload several seconds and refill in bounded main-thread reads. IRQs copy published samples only. Retain mono/stereo layouts, jingle delay, fades, intro/loop handling and split-allocation fallback. | [audioRing.h](../universal/include/common/audioRing.h), [pcmStream.h](../universal/include/common/pcmStream.h), [streamingaudio.c](../romsel_dsimenutheme/arm9/source/streamingaudio.c), sound.cpp. | Same 384,000-byte capacity. DSi/DLDI audio setup: 88 → 24 ms; browser startup: 557 → 489 ms. |

## Verifying and measuring

Agent regression tests, linked-image checks, profiling targets, timing markers,
boot/SD traces, the argv homebrew probe and
measurement records live on the [`diagnostics` branch](https://github.com/Sanrax/TWiLightMenu-Blocks/blob/diagnostics/docs/diagnostics.md).
Use its instructions to run checks or measurements manually. `master` contains
no test files or runtime instrumentation, and builds and GitHub Actions do not
run these checks.

Host coverage maps directly to changes: `loaderfat`, `ndsmetadata`, `inifile`,
`clocktext`, `photo`/`menuimage`/`texture`/`nitrofile`, `language`/`languagepack`,
`fontmap`/`nftrcompact`/`glyphpages`, and `soundbank`/`audiostream` in
[diagnostics tests](https://github.com/Sanrax/TWiLightMenu-Blocks/tree/diagnostics/tests).
The final graphics pass has seven pixel-identical paired bottom-screen captures,
including DS/DSi modes, both UIs, macro mode and external PNG/GRF/font skins.
Hardware results and remaining coverage are listed in the port notes.
