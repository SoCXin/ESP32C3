/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "digital_leds.h"

#define EVER    ;;
#define PI      (3.1415926)
#define TWO_PI  ((double)(2.0 * PI))
#define NUM_STEPS   100
#define BLUE_OFFSET (120.0 * (PI / 180.0))
#define GREEN_OFFSET (240.0 * (PI / 180.0))


double normalizedStepValue(uint8_t step) {
    double valPerStep = TWO_PI / (double)NUM_STEPS;

    return valPerStep * (double)step;
}


RgbValue rgbForStep(uint8_t stepNum) {
    double normalizedStep = normalizedStepValue(stepNum);
    uint8_t red = (uint8_t)(((1.0 + sin(normalizedStep)) / 2.0) * UINT8_MAX);
    uint8_t green = (uint8_t)(((1.0 + sin(normalizedStep + GREEN_OFFSET)) / 2.0) * UINT8_MAX);
    uint8_t blue = (uint8_t)(((1.0 + sin(normalizedStep + BLUE_OFFSET)) / 2.0) * UINT8_MAX);

    return (RgbValue){red, green, blue};
}


void ledMgmtTask(void *args) {
    initializeLedStrip();
    clearLedStrip();

    for(EVER) {
        for (uint8_t i = 0; i < NUM_STEPS; i++) {
            setFullLedStrip(rgbForStep(i), 0.1);
            vTaskDelay(15 / portTICK_PERIOD_MS);
        }
    }
}


void app_main(void) {
    printf("Running Esp32-C3 Sandbox!\n");

    xTaskCreate(ledMgmtTask, "ledMgmtTask", 1024, NULL, 10, NULL);
}
