#include "iconHandler.h"
#include "ThemeTextures.h"
#include "common/logging.h"
#include "common/tonccpy.h"

bool initialized;
static int iconTexID[NDS_ICON_BANK_COUNT];
static void* iconPixels[NDS_ICON_BANK_COUNT];
static void* iconPalettes[NDS_ICON_BANK_COUNT];
alignas(4) static u16 paletteCache[NDS_ICON_BANK_COUNT][16];
static glImage ndsIcon[NDS_ICON_BANK_COUNT][TWL_ICON_FRAMES];
static const u8 clearTiles[32 * TWL_TEX_HEIGHT / 2] = {};
static const u16 blackPalette[16] = {};

const glImage* getIcon(int num) {
    return BAD_ICON_IDX(num) || !initialized ? nullptr : ndsIcon[num];
}

void glReloadIconPalette(int num) {
    if (BAD_ICON_IDX(num) || !iconPalettes[num]) return;
    const u32 banks = vramSetBanks_EFG(VRAM_E_LCD, VRAM_F_LCD, VRAM_G_LCD);
    tonccpy(iconPalettes[num], paletteCache[num], sizeof(paletteCache[num]));
    vramRestoreBanks_EFG(banks);
}

void glLoadPalette(int num, const u16* palette) {
    if (BAD_ICON_IDX(num)) return;
    tonccpy(paletteCache[num], palette, sizeof(paletteCache[num]));
    glReloadIconPalette(num);
}

void reloadIconPalettes() {
    for (int i = 0; i < NDS_ICON_BANK_COUNT; ++i) glReloadIconPalette(i);
}

void glLoadIcon(int num, const u16* palette, const u8* tiles, int texHeight) {
    if (BAD_ICON_IDX(num) || !iconPixels[num]
        || texHeight <= 0 || texHeight > TWL_TEX_HEIGHT) return;

    // Banks keep their full eight-frame allocation. Runtime updates run in
    // VBlank and must not allocate/free GL textures or palettes: that can
    // interrupt a main-thread allocation and corrupt the allocator's state.
    const u32 banks = vramSetPrimaryBanks(VRAM_A_LCD, VRAM_B_LCD, VRAM_C_LCD, VRAM_D_LCD);
    tonccpy(iconPixels[num], tiles, 32 * texHeight / 2);
    vramRestorePrimaryBanks(banks);
    glLoadPalette(num, palette);
}

void glClearIcon(int num) {
    glLoadIcon(num, blackPalette, clearTiles, TWL_TEX_HEIGHT);
}

void iconManagerInit() {
    logPrint("iconManagerInit()\n");
    tex().loadIconUnknownTexture();
    glGenTextures(NDS_ICON_BANK_COUNT, iconTexID);
    for (int i = 0; i < NDS_ICON_BANK_COUNT; ++i) {
        glBindTexture(0, iconTexID[i]);
        if (!glTexImage2D(0, 0, GL_RGB16, TEXTURE_SIZE_32, TEXTURE_SIZE_256, 0,
                          TEXGEN_OFF | GL_TEXTURE_COLOR0_TRANSPARENT, nullptr)
            || !glColorTableNtr(16, nullptr)) {
            logPrint("Out of texture VRAM for icon bank %d!\n", i);
            continue;
        }
        iconPixels[i] = glGetTexturePointer(iconTexID[i]);
        iconPalettes[i] = glGetColorTablePointer(iconTexID[i]);
        for (int frame = 0; frame < TWL_ICON_FRAMES; ++frame) {
            ndsIcon[i][frame].width = 32;
            ndsIcon[i][frame].height = 32;
            ndsIcon[i][frame].u_off = 0;
            ndsIcon[i][frame].v_off = frame * 32;
            ndsIcon[i][frame].textureID = iconTexID[i];
        }
        glClearIcon(i);
        // The unknown icon contains one frame, not the eight-frame capacity
        // reserved above. Never read beyond its texture allocation.
        glLoadIcon(i, tex().iconUnknownTexture()->palette(), tex().iconUnknownTexture()->bytes(), 32);
    }
    initialized = true;
}
