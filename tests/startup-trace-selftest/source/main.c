#include "../../../universal/source/common/startupTrace.c"
int main(void) {
    startupTraceBegin();
    startupTraceMark("watchdog self-test");
    beginIo("simulated SD wait", 123, 1);
    for (;;) swiWaitForVBlank();
}
