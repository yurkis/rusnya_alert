#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define SOUND_VOLUME_NO_DIV (0)
#define SOUND_VOLUME_DIV2X  (1)
#define SOUND_VOLUME_DIV4X  (2)
#define SOUND_VOLUME_DIV8X  (3)

bool soundSetup();
void soundSetVolumeDiv(uint8_t div);
bool soundPlayFile(const char* filename);
bool soundPlayBuffer(const void* pBuff, size_t size);
