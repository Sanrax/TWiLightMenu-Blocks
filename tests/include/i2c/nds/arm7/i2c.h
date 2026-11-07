#pragma once
#include <nds/ndstypes.h>
#define I2C_PM 0x4A
#define REG_I2CDATA testData
#define REG_I2CCNT testControl
void i2cWaitBusy(void);
