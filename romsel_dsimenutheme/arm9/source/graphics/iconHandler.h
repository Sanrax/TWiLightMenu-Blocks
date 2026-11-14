#include <gl2d.h>
#include <nds.h> // REG_VCOUNT, for vramSafeToUnmap() below

#pragma once

// Half-width of the icon grid's draw window, in boxes either side of the
// cursor. The draw loop in graphics.cpp samples list entries
// [CURPOS - ICON_GRID_MAX_OFFSET, CURPOS + ICON_GRID_MAX_OFFSET].
#define ICON_GRID_MAX_OFFSET 3

// One texture bank per box the grid can draw in a single frame. This MUST stay
// >= the width of that window, or two entries drawn in the same frame alias
// onto one bank and the box further from the cursor renders the other entry's
// icon (and steals its palette in loadDeferredIconPalettes).
#define ICON_GRID_BANKS (2 * ICON_GRID_MAX_OFFSET + 1)

// Slot reserved in every per-title cache array (gameTid[], bnrSysSettings[],
// customIcon[], bnriconTile[], ... -- all sized 41) for the app being carried in
// move mode. It is NOT the app'''s index in the list: that keeps counting past 40
// on later pages, and indexing the caches with it reads out of bounds.
#define MOVING_APP_SLOT 40

// Dedicated bank for the app being dragged in move mode (cache MOVING_APP_SLOT).
#define ICON_MOVING_BANK ICON_GRID_BANKS

#define NDS_ICON_BANK_COUNT (ICON_GRID_BANKS + 1)

// Maps a list position (0..39) to the bank holding its icon. Every load site
// and every draw site must agree on this one expression.
#define ICON_GRID_BANK(pos) ((pos) % ICON_GRID_BANKS)

#define TWL_ICON_FRAMES 8
#define TWL_TEX_HEIGHT 256

// The 3D renderer starts reading textures at line 214, during VBlank, 48 lines
// before display resumes (GBATEK: DS 3D Rendering Engine). Remapping VRAM later
// in VBlank can therefore corrupt icons even though the copy finishes before
// visible scanout. Reserve time for a full 4 KiB upload, then the small palette
// updates. Check each copy: storage can delay entry into the VBlank handler.
#define VRAM_UNMAP_LAST_SAFE_LINE 200
#define VRAM_PALETTE_LAST_SAFE_LINE 208
static inline bool vramSafeToUnmap(bool paletteOnly = false) {
	const int line = REG_VCOUNT;
	return line >= 192 && line <= (paletteOnly ? VRAM_PALETTE_LAST_SAFE_LINE : VRAM_UNMAP_LAST_SAFE_LINE);
}

// Checks if the icon is a bad index
#define BAD_ICON_IDX(i) (i < 0 || i > (NDS_ICON_BANK_COUNT - 1))

/**
 * Gets the current icon stored at the specified index.
 * If the index is out of bounds or the icon manager is not
 * initialized, returns null.
 */
const glImage* getIcon(int num);

/**
 * Allocates and initializes the VRAM locations for
 * icons. Must be called before the icon manager is used.
 */
void iconManagerInit();

/**
 * Loads an icon into one of the NDS_ICON_BANK_COUNT existing banks,
 * overwritting the previous data.
 * num must be in the range [0, NDS_ICON_BANK_COUNT - 1] else this
 * function does nothing.
 * 
 * Reuses the pixel and palette allocations made by iconManagerInit().
 * No allocator or GL state changes occur during VBlank updates.
 * texHeight is the number of rows to copy (up to TWL_TEX_HEIGHT).
 */
void glLoadIcon(int num, const u16 *palette, const u8 *tiles, int texHeight = 32);

/**
 * Loads an icon's palette into one of the NDS_ICON_BANK_COUNT existing
 * banks, overwritting the previous data.
 * num must be in the range [0, NDS_ICON_BANK_COUNT - 1] else this
 * function does nothing.
 */
void glLoadPalette(int num, const u16 *palette);

/** Whether the bank's last uploaded palette matches these colors. */
bool iconPaletteMatches(int num, const u16 *palette);

/**
 * Clears an icon in the bank.
 */
void glClearIcon(int num);

/**
 * Reloads the palette of the icon in the 
 * numth slot, if it has been corrupted.
 */
void glReloadIconPalette(int num);

/**
 * Reloads the palette of all the icons in a slot, if
 * they have been corrupted.
 */
void reloadIconPalettes();
