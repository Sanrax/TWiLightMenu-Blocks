#pragma once
#define FIFO_PM 0
#define FIFO_FIRMWARE 1
void fifoSetValue32Handler(int channel, void (*handler)(u32, void *), void *data);
void fifoSetDatamsgHandler(int channel, void (*handler)(int, void *), void *data);
