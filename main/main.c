#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_spi_flash.h"
#include "post_notifier.h"


extern int wake_count;

void app_main(void)
{
    printf("Hello world!\n");

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_TIMER:
        printf("Wakeup from timer\n");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        printf("Wakeup from ULP\n");
        break;

    default:
        printf("Not a wakeup or undefined reason.\n");
        break;
    }

    printf("current wake count: %d\n", wake_count);

    for (int i = 3; i >= 0; i--)
    {
        printf("Sleeping in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Sleeping now.\n");
    fflush(stdout);
    esp_sleep_enable_timer_wakeup(3 * SEC_TO_USEC);
    esp_deep_sleep_start();
}
