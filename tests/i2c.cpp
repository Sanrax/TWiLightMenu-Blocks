#include <cassert>
#include <array>
#include <nds/ndstypes.h>

static unsigned depth, transactions, deliveredIRQs;
static bool pendingIRQ, selected, needsRegister;
static u8 currentRegister;
static std::array<u8, 256> registers;

int enterCriticalSection(void) { return depth++; }
void leaveCriticalSection(int old) {
    assert(depth == unsigned(old) + 1);
    depth--;
    if (!depth && pendingIRQ) {
        // The native IRQ can select another I2C register only after the
        // interrupted transaction has consumed its result.
        selected = false;
        pendingIRQ = false;
        deliveredIRQs++;
    }
}
void swiDelay(unsigned cycles) { (void)cycles; assert(depth); }
void i2cWaitBusy(void) { assert(depth); }

struct Data {
    u8 value = 0;
    operator u8() const { assert(depth); return value; }
    Data &operator=(u8 data) { assert(depth); value = data; return *this; }
} testData;

struct Control {
    u8 value = 0;
    operator u8() const { assert(depth); return value; }
    Control &operator=(u8 control) {
        assert(depth); // A power IRQ must never interleave the byte transfers.
        transactions++;
        pendingIRQ = true;
        if (control & 2) {
            selected = (testData.value & 0xFE) == 0x4A;
            needsRegister = true;
        } else if (control & 0x20) {
            if (selected) testData.value = registers[currentRegister++];
        } else if (!(control & 1) && selected) {
            if (needsRegister) { currentRegister = testData.value; needsRegister = false; }
            else registers[currentRegister++] = testData.value;
        }
        value = (control & ~0x90) | (selected ? 0x10 : 0);
        return *this;
    }
} testControl;

// Exercise the actual local driver, including its DS-mode-compatible copy of
// the I2C routines. Replacing it with SDK TWL-only code would lose that access.
#include "../universal/arm7/source/my_i2c.c"

int main(void) {
    registers[0x40] = 0x1F;
    assert(my_i2cReadRegister(0x4A, 0x40) == 0x1F);
    assert(deliveredIRQs == 1 && !depth);
    assert(my_i2cWriteRegister(0x4A, 0x41, 3) == 1 && registers[0x41] == 3);
    assert(deliveredIRQs == 2 && !depth);
    assert(my_i2cReadRegister(0x70, 0) == 0xFF);
    assert(deliveredIRQs == 3 && !depth);
    assert(my_i2cWriteRegister(0x70, 0, 1) == 0);
    assert(deliveredIRQs == 4 && !depth);

    // The same helpers run from IRQ handlers with interrupts already masked.
    const int old = enterCriticalSection();
    assert(my_i2cReadRegister(0x4A, 0x41) == 3);
    assert(my_i2cWriteRegister(0x4A, 0x41, 4) == 1 && registers[0x41] == 4);
    assert(depth == 1 && deliveredIRQs == 4 && pendingIRQ);
    leaveCriticalSection(old);
    assert(!depth && deliveredIRQs == 5 && transactions > 20);
}
