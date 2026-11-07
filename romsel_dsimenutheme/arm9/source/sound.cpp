#include "sound.h"
#include "tool/adpcm-xq.h"

#include "graphics/themefilenames.h"
#include "common/twlmenusettings.h"
#include "common/systemdetails.h"
#include "common/logging.h"
#include "graphics/fontHandler.h"
#include "language.h"
#include "fileCopy.h"
#include "streamingaudio.h"
#include "string.h"
#include "common/tonccpy.h"
#include <algorithm>
#include <sys/stat.h>
#include <new>
#include "common/soundbank.h"

#define SFX_STARTUP		0
#define SFX_WRONG		1
#define SFX_LAUNCH		2
#define SFX_STOP		3
#define SFX_SWITCH		4
#define SFX_SELECT		5
#define SFX_BACK		6
#define SFX_LIDCLOSE	7
#define SFX_LIDOPEN		8

#define MSL_NSONGS		0
#define MSL_NSAMPS		MSL_NSAMPS_LID
#define MSL_BANKSIZE	MSL_NSAMPS_LID


extern bool controlTopBright;

volatile char* SFX_DATA = (char*)NULL;
mm_word SOUNDBANK[MSL_BANKSIZE] = {0};

// Built-in banks use Maxmod's file loader, so unused jingles do not have to be
// read or kept in RAM. Custom paths can exceed that loader's 64-character
// filename limit; retain the memory-bank interface for those themes.
bool SoundControl::readSfxBank() {
	sfxFilePath = ms().theme == TWLSettings::EThemeSaturn
		? TFN_SATURN_SOUND_EFFECTBANK : TFN_DEFAULT_SOUND_EFFECTBANK;
	FILE* soundbank_file = nullptr;
	if (ms().theme != TWLSettings::EThemeSaturn && ms().dsiMusic == 3) {
		soundbank_file = fopen(std::string(TFN_SOUND_EFFECTBANK).c_str(), "rb");
		if (soundbank_file) sfxFilePath = nullptr;
	}
	if (!soundbank_file) soundbank_file = fopen(sfxFilePath, "rb");
	if (!soundbank_file) return false;

	u8 bytes[20] = {};
	soundbank::Header header{};
	bool valid = fread(bytes, 1, sizeof(bytes), soundbank_file) == sizeof(bytes)
		&& soundbank::readHeader(bytes, sizeof(bytes), header);
	if (valid && !sfxFilePath) {
		valid = fseek(soundbank_file, 0, SEEK_END) == 0;
		const long size = valid ? ftell(soundbank_file) : -1;
		valid = size >= long(sizeof(bytes)) && fseek(soundbank_file, 0, SEEK_SET) == 0;
		if (valid) {
			SFX_DATA = new (std::nothrow) char[size];
			valid = SFX_DATA && fread((void*)SFX_DATA, 1, size, soundbank_file) == size_t(size)
				&& soundbank::validEffects((const void*)SFX_DATA, size);
		}
	}
	fclose(soundbank_file);
	if (!valid) {
		delete[] SFX_DATA;
		SFX_DATA = nullptr;
		return false;
	}
	sfxSampleCount = header.sampleCount;
	startup_sample_length = header.startupSamples;
	return true;
}

// Preload navigation and lid sounds. The larger launch and startup effects
// are loaded on their first use; older banks can omit the lid sounds.
void SoundControl::loadSfxEffects() {
	loadEffect(SFX_SELECT);
	loadEffect(SFX_STOP);
	loadEffect(SFX_WRONG);
	loadEffect(SFX_BACK);
	loadEffect(SFX_SWITCH);

	snd_launch = {
	    {SFX_LAUNCH},	    // id
	    (int)(1.0f * (1 << 10)), // rate
	    0,			     // handle
	    255,		     // volume
	    128,		     // panning
	};
	snd_select = {
	    {SFX_SELECT},	    // id
	    (int)(1.0f * (1 << 10)), // rate
	    0,			     // handle
	    255,		     // volume
	    128,		     // panning
	};
	snd_stop = {
	    {SFX_STOP},		     // id
	    (int)(1.0f * (1 << 10)), // rate
	    0,			     // handle
	    255,		     // volume
	    128,		     // panning
	};
	snd_wrong = {
	    {SFX_WRONG},	     // id
	    (int)(1.0f * (1 << 10)), // rate
	    0,			     // handle
	    255,		     // volume
	    128,		     // panning
	};
	snd_back = {
	    {SFX_BACK},		     // id
	    (int)(1.0f * (1 << 10)), // rate
	    0,			     // handle
	    255,		     // volume
	    128,		     // panning
	};
	snd_switch = {
	    {SFX_SWITCH},	    // id
	    (int)(1.0f * (1 << 10)), // rate
	    0,			     // handle
	    255,		     // volume
	    128,		     // panning
	};
	mus_startup = {
	    {SFX_STARTUP},	   // id
	    (int)(1.0f * (1 << 10)), // rate
	    0,			     // handle
	    255,		     // volume
	    128,		     // panning
	};

	if (!sfxHasLidSounds()) return;

	loadEffect(SFX_LIDCLOSE);
	loadEffect(SFX_LIDOPEN);

	snd_lidClose = {
	    {SFX_LIDCLOSE},	     // id
	    (int)(1.0f * (1 << 10)), // rate
	    0,			     // handle
	    255,		     // volume
	    128,		     // panning
	};
	snd_lidOpen = {
	    {SFX_LIDOPEN},	     // id
	    (int)(1.0f * (1 << 10)), // rate
	    0,			     // handle
	    255,		     // volume
	    128,		     // panning
	};
}

SoundControl::SoundControl()
	: sfxDataLoaded(false), sfxSampleCount(0), loadedEffects(0), sfxFilePath(nullptr),
	  stream_is_playing(false), stream_source(NULL), startup_sample_length(0), seekPos(0)
 {

	sndSys.mod_count = MSL_NSONGS;
	sndSys.samp_count = MSL_NSAMPS;
	sndSys.mem_bank = SOUNDBANK;
	sndSys.fifo_channel = FIFO_MAXMOD;

	mmInit(&sndSys);
	reloadSfxData();
}

void SoundControl::loadStream(const bool prepMsg) {
	unloadStream();
	if (ms().dsiMusic == 0 || ms().theme == TWLSettings::EThemeSaturn) {
		return;
	}

	if (!alloc_streaming_buf()) return;
	resetStreamSettings();

	bool loopableMusic = false;

	mkdir(sys().isRunFromSD() ? "sd:/_nds/TWiLightMenu/cache" : "fat:/_nds/TWiLightMenu/cache", 0777);
	mkdir(sys().isRunFromSD() ? "sd:/_nds/TWiLightMenu/cache/music" : "fat:/_nds/TWiLightMenu/cache/music", 0777);
	std::string devicePath = sys().isRunFromSD() ? "sd:" : "fat:";

	stream.sampling_rate = 16000;	 		// 16000Hz
	stream.format = MM_STREAM_16BIT_MONO;  // select format

	if (ms().theme == TWLSettings::EThemeSaturn) {
		stream_source = fopen(std::string(TFN_DEFAULT_SOUND_BG).c_str(), "rb");
	} else {
		switch(ms().dsiMusic) {
			case 5: // HBL
			case 2: // DSi Shop
			{	std::string loopPath = std::string(ms().dsiMusic == 5 ? TFN_HBL_LOOP_SOUND_BG : TFN_SHOP_LOOP_SOUND_BG);
				if (access(loopPath.c_str(), F_OK) == 0) {
					stream_start_source = fopen(std::string(ms().dsiMusic == 5 ? TFN_HBL_START_SOUND_BG : TFN_SHOP_START_SOUND_BG).c_str(), "rb");
					stream_source = fopen(loopPath.c_str(), "rb");
					if (!stream_source) { unloadStream(); return; }
					logPrint(ms().dsiMusic == 5 ? "Using HBL music\n" : "Using DSi Shop music\n");

					// Read properties from WAV header
					u8 numChannels = 1;
					fseek(stream_source, 0x16, SEEK_SET);
					fread(&numChannels, sizeof(u8), 1, stream_source);
					stream.format = numChannels == 2 ? MM_STREAM_8BIT_STEREO : MM_STREAM_16BIT_MONO;
					fseek(stream_source, 0x18, SEEK_SET);
					fread(&stream.sampling_rate, sizeof(u16), 1, stream_source);

					seekPos = 0x2C;
					loopableMusic = true;
					break;
				}
			}
			case 3: { // Theme
				bool customSkin = false;
				switch (ms().theme) {
					case 0:
					default:
						customSkin = (tfn().uiDirectory() != TFN_FALLBACK_DSI_UI_DIRECTORY);
						break;
					case 1:
						customSkin = (tfn().uiDirectory() != TFN_FALLBACK_3DS_UI_DIRECTORY);
						break;
					//case 4:
					//	customSkin = (tfn().uiDirectory() != TFN_FALLBACK_SATURN_UI_DIRECTORY);
					//	break;
					case 5:
						customSkin = (tfn().uiDirectory() != TFN_FALLBACK_HBLAUNCHER_UI_DIRECTORY);
						break;
				}
				if (customSkin) {
					std::string musicStartPath = TFN_START_SOUND_BG;
					std::string musicPath = TFN_SOUND_BG;
					std::string cacheStartPath = TFN_START_SOUND_BG_CACHE;
					std::string cachePath = TFN_SOUND_BG_CACHE;
					bool closed = false;
					if (access(musicPath.c_str(), F_OK) == 0) {
						loopableMusic = (access(musicStartPath.c_str(), F_OK) == 0);
						if (loopableMusic) {
							logPrint("Using custom theme music\n");
							stream_start_source = fopen(musicStartPath.c_str(), "rb");
						}

						// Read properties from WAV header
						u8 wavFormat = 0;
						u8 numChannels = 1;
						stream_source = fopen(musicPath.c_str(), "rb");
						if (!stream_source) { unloadStream(); return; }
						fseek(stream_source, 0x14, SEEK_SET);
						fread(&wavFormat, sizeof(u8), 1, stream_source);
						fseek(stream_source, 0x16, SEEK_SET);
						fread(&numChannels, sizeof(u8), 1, stream_source);
						stream.format = numChannels == 2 ? MM_STREAM_8BIT_STEREO : MM_STREAM_16BIT_MONO;
						fseek(stream_source, 0x18, SEEK_SET);
						fread(&stream.sampling_rate, sizeof(u16), 1, stream_source);

						if (wavFormat == 0x11) {
							// If music is ADPCM and hasn't been successfully converted yet, do so now
							bool doClearText = false;
							if (loopableMusic && (access(cacheStartPath.c_str(), F_OK) != 0 || getFileSize(cacheStartPath.c_str()) == 0)) { // Start point
								if (prepMsg) {
									clearText(false);
									printLarge(false, 0, 88 - (calcLargeFontHeight(STR_PREPARING_MUSIC) - largeFontHeight()) / 2, STR_PREPARING_MUSIC, Alignment::center);
									updateText(false);
									doClearText = true;
								}
								if (adpcm_main(musicStartPath.c_str(), cacheStartPath.c_str(), numChannels == 2) == -1) {
									remove(cacheStartPath.c_str());
								}
							}
							if (access(cachePath.c_str(), F_OK) != 0 || getFileSize(cachePath.c_str()) == 0) { // Loop point
								if (prepMsg && !doClearText) {
									clearText(false);
									printLarge(false, 0, 88 - (calcLargeFontHeight(STR_PREPARING_MUSIC) - largeFontHeight()) / 2, STR_PREPARING_MUSIC, Alignment::center);
									updateText(false);
									doClearText = true;
								}
								if (adpcm_main(musicPath.c_str(), cachePath.c_str(), numChannels == 2) == -1) {
									remove(cachePath.c_str());
								}
							}
							if (prepMsg && doClearText) {
								clearText(false);
								updateText(false);
							}
							if (loopableMusic) {
								fclose(stream_start_source);
							}
							fclose(stream_source);
							closed = true;
						} else {
							seekPos = 0x2C;
						}
					} else {
						loopableMusic = (access(cacheStartPath.c_str(), F_OK) == 0);
						closed = true;
					}
					if (closed) {
						if (loopableMusic) {
							stream_start_source = fopen(cacheStartPath.c_str(), "rb");
						}
						stream_source = fopen(cachePath.c_str(), "rb");
					}
					if (stream_source) break; } // fallthrough if stream_source fails.
				}
			case 4:
			case 1:
			default: {
				const bool use3DSMusic = (ms().dsiMusic == 4 || (ms().dsiMusic == 3 && ms().theme == TWLSettings::ETheme3DS)) && (access(std::string(TFN_DEFAULT_SOUND_BG_3D).c_str(), F_OK) == 0);
				const bool useHBLMusic = (ms().dsiMusic == 3) && (ms().theme == TWLSettings::EThemeHBL) && (access(std::string(TFN_HBL_LOOP_SOUND_BG).c_str(), F_OK) == 0);
				bool useBetterDSiMusic = false;
				if (useHBLMusic) {
					logPrint("Using HBL music\n");
					stream_start_source = fopen(std::string(TFN_HBL_START_SOUND_BG).c_str(), "rb");
					loopableMusic = true;
				} else if (use3DSMusic) {
					logPrint("Using 3DS menu music\n");
				} else {
					logPrint("Using DSi menu music\n");
					useBetterDSiMusic = (access(std::string(TFN_BETTER_DEFAULT_SOUND_BG).c_str(), F_OK) == 0);
					if (useBetterDSiMusic) {
						logPrint("Better version detected\n");
					}
				}
				stream_source = fopen(std::string(useHBLMusic ? TFN_HBL_LOOP_SOUND_BG : (use3DSMusic ? TFN_DEFAULT_SOUND_BG_3D : (useBetterDSiMusic ? TFN_BETTER_DEFAULT_SOUND_BG : TFN_DEFAULT_SOUND_BG))).c_str(), "rb");
				if (!stream_source) { unloadStream(); return; }

				// Read properties from WAV header
				u8 numChannels = 1;
				fseek(stream_source, 0x16, SEEK_SET);
				fread(&numChannels, sizeof(u8), 1, stream_source);
				stream.format = numChannels == 2 ? MM_STREAM_8BIT_STEREO : MM_STREAM_16BIT_MONO;
				fseek(stream_source, 0x18, SEEK_SET);
				fread(&stream.sampling_rate, sizeof(u16), 1, stream_source);

				seekPos = 0x2C;
				break; }
		}
	}

	logPrint("\n");

	if (!pcmStream.reset(stream_source, loopableMusic ? stream_start_source : nullptr, seekPos)) {
		unloadStream();
		return;
	}

	stream.buffer_length = 0x1000;
	stream.callback = on_stream_request;
	stream.timer = MM_TIMER0;
	stream.manual = false;

	// Start with at least two seconds (three at the default 16 kHz), retaining
	// the complete former capacity for refills during normal menu operations.
	const unsigned initial = std::min<unsigned>(STREAMING_BUF_LENGTH,
		std::max<unsigned>(48000, stream.sampling_rate * 2));
	while (streaming_ring.queued < initial) {
		const unsigned before = streaming_ring.queued;
		fillStream(initial - before);
		if (streaming_ring.queued == before) break;
	}
}

void SoundControl::reloadSfxData() {
	if (sfxDataLoaded || !readSfxBank()) return;
	if (sfxFilePath) mmSoundBankInFiles(sfxFilePath);
	else mmSoundBankInMemory((mm_addr)SFX_DATA);
	loadedEffects = 0;
	loadSfxEffects();
	sfxDataLoaded = true;
}

void SoundControl::unloadSfxData() {
	if (!sfxDataLoaded) return;
	sfxDataLoaded = false;

	fifoSendValue32(FIFO_USER_02, 0x4F444E53); // 'SNDO': Stop all sound channels.
	swiWaitForVBlank(); // Let ARM7 stop playback before the file loader frees samples.
	for (unsigned id = 0; id < MSL_NSAMPS; ++id) {
		if (loadedEffects & BIT(id)) mmUnloadEffect(id);
	}
	loadedEffects = 0;
	delete[] SFX_DATA;
	SFX_DATA = nullptr;
}

bool SoundControl::loadEffect(unsigned id) {
	if (id >= MSL_NSAMPS || id >= sfxSampleCount) return false;
	if (!(loadedEffects & BIT(id))) {
		if (mmLoadEffect(id) != 0) return false;
		DC_FlushAll(); // ARM7 reads the newly loaded sample directly from main RAM.
		loadedEffects |= BIT(id);
	}
	return true;
}

mm_sfxhand SoundControl::playEffect(mm_sound_effect &effect, u8 panning) {
	if (!sfxDataLoaded || !loadEffect(effect.id)) return MM_SFXHAND_INVALID;
	effect.panning = panning;
	return mmEffectEx(&effect);
}

mm_sfxhand SoundControl::playLaunch(u8 panning)  { return playEffect(snd_launch, panning); }
mm_sfxhand SoundControl::playSelect(u8 panning)  { return playEffect(snd_select, panning); }
mm_sfxhand SoundControl::playBack(u8 panning)    { return playEffect(snd_back, panning); }
mm_sfxhand SoundControl::playSwitch(u8 panning)  { return playEffect(snd_switch, panning); }
mm_sfxhand SoundControl::playStartup(u8 panning) { return playEffect(mus_startup, panning); }
mm_sfxhand SoundControl::playStop(u8 panning)    { return playEffect(snd_stop, panning); }
mm_sfxhand SoundControl::playWrong(u8 panning)   { return playEffect(snd_wrong, panning); }
mm_sfxhand SoundControl::playLidClose(u8 panning) { return sfxHasLidSounds() ? playEffect(snd_lidClose, panning) : MM_SFXHAND_INVALID; }
mm_sfxhand SoundControl::playLidOpen(u8 panning)  { return sfxHasLidSounds() ? playEffect(snd_lidOpen, panning) : MM_SFXHAND_INVALID; }

void SoundControl::beginStream() {
	if (!stream_source || stream_is_playing) return;

	// open the stream
	stream_is_playing = true;
	mmStreamOpen(&stream);
	SetYtrigger(0);
}

void SoundControl::stopStream() {
	if (!stream_source || !stream_is_playing) return;

	stream_is_playing = false;
	mmStreamClose();
}

void SoundControl::unloadStream() {
	if (stream_is_playing) mmStreamClose();
	stream_is_playing = false;
	if (stream_start_source) fclose(stream_start_source);
	if (stream_source) fclose(stream_source);
	stream_start_source = stream_source = nullptr;
	free_streaming_buf();
}

void SoundControl::fadeOutStream(bool fast) {
	fifoSendValue32(FIFO_USER_01, fast ? 2 : 1); // Fade out on ARM7 side
}

void SoundControl::cancelFadeOutStream() {
	fifoSendValue32(FIFO_USER_01, 0);
}

void SoundControl::setStreamDelay(u32 delay) {
	sample_delay_count = delay;
}


static volatile bool stopSoundPending = false;

void queueStopSound() {
	stopSoundPending = true;
}

void SoundControl::fillStream(unsigned frames) {
	const unsigned space = twlmAudioRingWriteSpace(&streaming_ring);
	frames = std::min(frames, space);
	if (!frames) return;
	const unsigned got = pcmStream.read(twlmAudioRingWriteData(&streaming_ring), frames);
	// IRQ playback may have consumed older frames during the read. Publish only
	// the completed write, with interrupts masked for the pointer/count update.
	const int irqState = enterCriticalSection();
	twlmAudioRingCommit(&streaming_ring, got);
	leaveCriticalSection(irqState);
}

void SoundControl::updateStream() {
	const int irqState = enterCriticalSection();
	const bool playQueuedStop = stopSoundPending;
	stopSoundPending = false;
	leaveCriticalSection(irqState);
	if (playQueuedStop) playStop();
	if (stream_is_playing) fillStream(STREAMING_REFILL_FRAMES);
}
