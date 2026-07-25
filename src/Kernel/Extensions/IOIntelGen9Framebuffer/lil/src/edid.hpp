#pragma once

#include <lil/intel.h>

typedef struct EdidTiming {
    uint8_t resolution;
    uint8_t frequency;
} EdidTiming;

typedef struct DetailTiming {
    uint16_t pixelClock;
    uint8_t horzActive;
    uint8_t horzBlank;
    uint8_t horzActiveBlankMsb;
    uint8_t vertActive;
    uint8_t vertBlank;
    uint8_t vertActiveBlankMsb;
    uint8_t horzSyncOffset;
    uint8_t horzSyncPulse;
    uint8_t vertSync;
    uint8_t syncMsb;
    uint8_t dimensionWidth;
    uint8_t dimensionHeight;
    uint8_t dimensionMsb;
    uint8_t horzBorder;
    uint8_t vertBorder;
    uint8_t features;
} DetailTiming;

typedef struct DisplayData {
    uint8_t magic[8];
    uint16_t vendorId;
    uint16_t productId;
    uint32_t serialNumber;
    uint8_t manufactureWeek;
    uint8_t manufactureYear;
    uint8_t structVersion;
    uint8_t structRevision;
    uint8_t inputParameters;
    uint8_t screenWidth;
    uint8_t screenHeight;
    uint8_t gamma;
    uint8_t features;
    uint8_t colorCoordinates[10];
    uint8_t estTimings1;
    uint8_t estTimings2;
    uint8_t vendorTimings;
    EdidTiming standardTimings[8];
    DetailTiming detailTimings[4];
    uint8_t numExtensions;
    uint8_t checksum;
} DisplayData;

#ifdef __cplusplus
extern "C" {
#endif

void edid_timing_to_mode(DisplayData* edid, DetailTiming timing, LilModeInfo* mode);

#ifdef __cplusplus
}
#endif
