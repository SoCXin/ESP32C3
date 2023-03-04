#include "digital_leds.h"
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include "esp_log.h"
#include "esp_attr.h"
#include "driver/rmt.h"


#define WS2812_T0H_NS (350)
#define WS2812_T0L_NS (1000)
#define WS2812_T1H_NS (1000)
#define WS2812_T1L_NS (350)
#define WS2812_RESET_US (280)

const char *DIGITAL_LED_LOG_TAG = "RGB_LED";


/** @brief LED Strip Device Type */
typedef void *ledStripDevice;

/** @brief LED Strip Configuration Type */
typedef struct {
    uint32_t max_leds;   /*!< Maximum LEDs in a single strip */
    ledStripDevice dev; /*!< LED strip device (e.g. RMT channel, PWM channel, etc) */
} ledStripConfig;

typedef struct {
    rmt_channel_t rmtChn;
    uint32_t numLeds;
    uint8_t pixelBuffer[MAX_NUM_LEDS * BYTES_PER_LED];
} ws2812bLedStrip;


static ws2812bLedStrip strip = {0};
static uint32_t ws2812_t0h_ticks = 0;
static uint32_t ws2812_t1h_ticks = 0;
static uint32_t ws2812_t0l_ticks = 0;
static uint32_t ws2812_t1l_ticks = 0;
static const uint8_t RMT_CLK_DIV = 2;


/**
 * @brief Convert RGB data to RMT format.
 *
 * @note For WS2812, R,G,B each contains 256 different choices (i.e. uint8_t)
 *
 * @param[in] src: source data, to converted to RMT format
 * @param[in] dest: place where to store the convert result
 * @param[in] src_size: size of source data
 * @param[in] wanted_num: number of RMT items that want to get
 * @param[out] translated_size: number of source data that got converted
 * @param[out] item_num: number of RMT items which are converted from source data
 */
static void IRAM_ATTR ws2812_rmt_adapter(const void *src,
                                         rmt_item32_t *dest,
                                         size_t src_size,
                                         size_t wanted_num,
                                         size_t *translated_size,
                                         size_t *item_num)
{
    if (src == NULL || dest == NULL) {
        *translated_size = 0;
        *item_num = 0;
        return;
    }

    const rmt_item32_t bit0 = {{{ ws2812_t0h_ticks, 1, ws2812_t0l_ticks, 0 }}}; //Logical 0
    const rmt_item32_t bit1 = {{{ ws2812_t1h_ticks, 1, ws2812_t1l_ticks, 0 }}}; //Logical 1
    size_t size = 0;
    size_t num = 0;
    uint8_t *psrc = (uint8_t *)src;

    rmt_item32_t *pdest = dest;

    while (size < src_size && num < wanted_num) {
        for (int i = 0; i < 8; i++) {
            // MSB first
            if (*psrc & (1 << (7 - i))) {
                pdest->val =  bit1.val;
            } else {
                pdest->val =  bit0.val;
            }

            num++;
            pdest++;
        }

        size++;
        psrc++;
    }

    *translated_size = size;
    *item_num = num;
}


static esp_err_t updatePixelBuffer(uint32_t index, uint32_t red, uint32_t green, uint32_t blue) {
    uint32_t start = index * 3;

    if (index >= strip.numLeds) {
        ESP_LOGE(DIGITAL_LED_LOG_TAG, "Cannot set pixel beyond strip length!");
        return PIXEL_SET_FAILURE;
    }

    // In the order of GRB
    strip.pixelBuffer[start + 0] = green & 0xFF;
    strip.pixelBuffer[start + 1] = red & 0xFF;
    strip.pixelBuffer[start + 2] = blue & 0xFF;

    return ESP_OK;
}


static esp_err_t transmitPixelBuffer(uint32_t timeout_ms) {
    uint32_t pixelDataSize = strip.numLeds * BYTES_PER_LED;

    if (rmt_write_sample(strip.rmtChn, strip.pixelBuffer, pixelDataSize, true) != ESP_OK) {
        ESP_LOGE(DIGITAL_LED_LOG_TAG, "Failed to write pixel data to RMT!");
        return PIXEL_SET_FAILURE;
    }

    return rmt_wait_tx_done(strip.rmtChn, pdMS_TO_TICKS(timeout_ms));
}


static void clearPixelBuffer() {
    uint32_t pixelDataSize = strip.numLeds * BYTES_PER_LED;

    // Write zero to turn off all leds
    memset(strip.pixelBuffer, 0, pixelDataSize);
}


static esp_err_t newStripFromCfg(const ledStripConfig *config) {
    uint32_t counterClkHz = 0;
    float nsToTickratio = 0.0;

    if (config == NULL) {
        ESP_LOGE(DIGITAL_LED_LOG_TAG, "Invalid strip config!");
        return STRIP_CFG_ERR;
    }

    if (rmt_get_counter_clock((rmt_channel_t)config->dev, &counterClkHz) != ESP_OK) {
        ESP_LOGE(DIGITAL_LED_LOG_TAG, "Unable to get RMT counter clock!");
        return STRIP_CFG_ERR;
    }

    // ns -> ticks
    nsToTickratio = (float)counterClkHz / 1e9;
    ws2812_t0h_ticks = (uint32_t)(nsToTickratio * WS2812_T0H_NS);
    ws2812_t0l_ticks = (uint32_t)(nsToTickratio * WS2812_T0L_NS);
    ws2812_t1h_ticks = (uint32_t)(nsToTickratio * WS2812_T1H_NS);
    ws2812_t1l_ticks = (uint32_t)(nsToTickratio * WS2812_T1L_NS);

    // set ws2812 to rmt adapter
    rmt_translator_init((rmt_channel_t)config->dev, ws2812_rmt_adapter);

    strip.rmtChn = (rmt_channel_t)config->dev;
    strip.numLeds = config->max_leds;

    return ESP_OK;
}


esp_err_t initializeLedStrip() {
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(CONFIG_DIGITAL_LED_OUTPUT_PIN, RMT_CHANNEL_0);
    config.clk_div = RMT_CLK_DIV;

    if (rmt_config(&config) != ESP_OK) {
        ESP_LOGE(DIGITAL_LED_LOG_TAG, "Unable to config RMT!");
        return RMT_CFG_ERR;
    }

    if (rmt_driver_install(config.channel, 0, 0) != ESP_OK) {
        ESP_LOGE(DIGITAL_LED_LOG_TAG, "Unable to install RMT driver!");
        return RMT_DRVR_INST_ERR;
    }

    ledStripConfig strip_config = { CONFIG_NUMBER_OF_LEDS, (ledStripDevice)config.channel };

    if (newStripFromCfg(&strip_config) != ESP_OK) {
        return STRIP_CFG_ERR;
    }

    return ESP_OK;
}


esp_err_t clearLedStrip() {
    clearPixelBuffer();

    return transmitPixelBuffer(100);
}


esp_err_t setLedStripPixel(uint32_t pixelNum, RgbValue color, float brightness) {
    uint8_t r = color.red * brightness;
    uint8_t g = color.green * brightness;
    uint8_t b = color.blue * brightness;

    if (updatePixelBuffer(pixelNum, r, g, b) != ESP_OK) {
       ESP_LOGE(DIGITAL_LED_LOG_TAG, "Failed setting color!");
       return PIXEL_SET_FAILURE;
    }

    return transmitPixelBuffer(100);
}


esp_err_t setFullLedStrip(RgbValue color, float brightness) {
    for (uint32_t i = 0; i < strip.numLeds; i++) {
        if (setLedStripPixel(i, color, brightness) != ESP_OK) {
            return PIXEL_SET_FAILURE;
        }
    }

    return ESP_OK;
}