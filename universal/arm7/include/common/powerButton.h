#pragma once
#include <stdbool.h>
#include <nds/interrupts.h>

// A DSi power-button request must reboot the console, not return from ARM7
// main() into the SDK's homebrew return-to-loader stub. Queue it from the
// I2C IRQ so the existing firmware reboot runs after any active IRQ finishes.
static volatile bool powerButtonRebootPending;

static inline void requestPowerButtonReboot(void) {
    powerButtonRebootPending = true;
}

static inline void processPowerButtonReboot(void (*reboot)(void)) {
    if (powerButtonRebootPending) {
        powerButtonRebootPending = false;
        const int oldIME = enterCriticalSection();
        reboot();
        leaveCriticalSection(oldIME);
    }
}
