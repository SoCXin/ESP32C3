#ifndef DIGITAL_LEDS_H
#define DIGITAL_LEDS_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NUM_LEDS    100
#define BYTES_PER_LED   3

#define RMT_CFG_ERR -1
#define RMT_DRVR_INST_ERR -2
#define PIXEL_SET_FAILURE -3
#define STRIP_CFG_ERR   -4

/** PUBLIC API */
extern const char *DIGITAL_LED_LOG_TAG;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RgbValue;

#define BLUE (RgbValue){ 0, 0, 255 }
#define RED (RgbValue){ 255, 0, 0 }
#define AMBER (RgbValue){ 255, 100, 0 }
#define GREEN (RgbValue){ 0, 255, 0 }

esp_err_t initializeLedStrip();
esp_err_t clearLedStrip();
esp_err_t setLedStripPixel(uint32_t pixelNum, RgbValue color, float brightness);
esp_err_t setFullLedStrip(RgbValue color, float brightness);

#ifdef __cplusplus
}
#endif

#endif
