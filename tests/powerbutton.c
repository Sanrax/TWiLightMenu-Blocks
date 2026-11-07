#include <assert.h>
#include "common/powerButton.h"

static unsigned reboots;
static bool requestDuringReboot;
static int criticalDepth;
int enterCriticalSection(void) { return criticalDepth++; }
void leaveCriticalSection(int old) { assert(criticalDepth == old + 1); criticalDepth--; }
static void reboot(void) {
    assert(criticalDepth == 1);
    reboots++;
    if (requestDuringReboot) requestPowerButtonReboot();
}

int main(void) {
    processPowerButtonReboot(reboot);
    assert(reboots == 0);
    requestPowerButtonReboot();
    requestPowerButtonReboot();
    assert(reboots == 0); // The I2C IRQ only queues the request.
    processPowerButtonReboot(reboot);
    assert(reboots == 1);
    processPowerButtonReboot(reboot);
    assert(reboots == 1);
    requestDuringReboot = true;
    requestPowerButtonReboot();
    processPowerButtonReboot(reboot);
    assert(reboots == 2 && powerButtonRebootPending);
    requestDuringReboot = false;
    processPowerButtonReboot(reboot);
    assert(reboots == 3 && !powerButtonRebootPending);
    assert(criticalDepth == 0);
}
