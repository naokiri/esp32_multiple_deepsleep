// This file name should be treated special to put to RTC memory or wakeup stub won't work.
#include "esp_rom_sys.h"
#include "esp_sleep.h"
#include "soc/timer_group_reg.h"
#include "soc/uart_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc.h"
#include "rom/rtc.h"
#include "soc/wdev_reg.h"
#include "post_notifier.h"

int wake_count;

// copied from sleep_modes.c since they are not exposed in header
#ifdef CONFIG_IDF_TARGET_ESP32
#define DEFAULT_CPU_FREQ_MHZ CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ
#define DEFAULT_SLEEP_OUT_OVERHEAD_US (212)
#define DEFAULT_HARDWARE_OUT_OVERHEAD_US (60)
#elif CONFIG_IDF_TARGET_ESP32S2
#define DEFAULT_CPU_FREQ_MHZ CONFIG_ESP32S2_DEFAULT_CPU_FREQ_MHZ
#define DEFAULT_SLEEP_OUT_OVERHEAD_US (147)
#define DEFAULT_HARDWARE_OUT_OVERHEAD_US (28)
#elif CONFIG_IDF_TARGET_ESP32S3
#define DEFAULT_CPU_FREQ_MHZ CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ
#define DEFAULT_SLEEP_OUT_OVERHEAD_US (0)
#define DEFAULT_HARDWARE_OUT_OVERHEAD_US (0)
#elif CONFIG_IDF_TARGET_ESP32C3
#define DEFAULT_CPU_FREQ_MHZ CONFIG_ESP32C3_DEFAULT_CPU_FREQ_MHZ
#define DEFAULT_SLEEP_OUT_OVERHEAD_US (105)
#define DEFAULT_HARDWARE_OUT_OVERHEAD_US (37)
#elif CONFIG_IDF_TARGET_ESP32H2
#define DEFAULT_CPU_FREQ_MHZ CONFIG_ESP32H2_DEFAULT_CPU_FREQ_MHZ
#define DEFAULT_SLEEP_OUT_OVERHEAD_US (105)
#define DEFAULT_HARDWARE_OUT_OVERHEAD_US (37)
#endif
#define DEEP_SLEEP_TIME_OVERHEAD_US (250 + 100 * 240 / DEFAULT_CPU_FREQ_MHZ)

void reset_timer(uint64_t duration_us)
{
    // Feed watchdog
    REG_WRITE(TIMG_WDTFEED_REG(0), 1);
    ets_printf("Resleeping. NIDONE.\n");

    // Get RTC calibration
    uint32_t period = REG_READ(RTC_SLOW_CLK_CAL_REG);
    // Calculate sleep duration in microseconds
    int64_t sleep_duration = (int64_t)duration_us - (int64_t)DEEP_SLEEP_TIME_OVERHEAD_US;
    if (sleep_duration < 0)
    {
        sleep_duration = 0;
    }
    // Convert microseconds to RTC clock cycles
    int64_t rtc_count_delta = (sleep_duration << RTC_CLK_CAL_FRACT) / period;
    // Feed watchdog
    REG_WRITE(TIMG_WDTFEED_REG(0), 1);
    // Get current RTC time
    SET_PERI_REG_MASK(RTC_CNTL_TIME_UPDATE_REG, RTC_CNTL_TIME_UPDATE);
    while (GET_PERI_REG_MASK(RTC_CNTL_TIME_UPDATE_REG, RTC_CNTL_TIME_VALID) == 0)
    {
        ets_delay_us(1);
    }
    SET_PERI_REG_MASK(RTC_CNTL_INT_CLR_REG, RTC_CNTL_TIME_VALID_INT_CLR);
    uint64_t now = READ_PERI_REG(RTC_CNTL_TIME0_REG);
    // // Print only 32bit for nano I/O
    // ets_printf("now=%ld\n", (long) now);
    // ets_printf("delta=%ld\n", (long) rtc_count_delta);    
    now |= ((uint64_t)READ_PERI_REG(RTC_CNTL_TIME1_REG)) << 32;

    // Set wakeup time
    uint64_t future = now + rtc_count_delta;    
    WRITE_PERI_REG(RTC_CNTL_SLP_TIMER0_REG, future & UINT32_MAX);
    WRITE_PERI_REG(RTC_CNTL_SLP_TIMER1_REG, future >> 32);

    // Wait for print
    while (REG_GET_FIELD(UART_STATUS_REG(0), UART_ST_UTX_OUT))
    {
    }

    // Start RTC deepsleep timer
    REG_SET_FIELD(RTC_CNTL_WAKEUP_STATE_REG, RTC_CNTL_WAKEUP_ENA, RTC_TIMER_TRIG_EN); // Wake up on timer
    WRITE_PERI_REG(RTC_CNTL_SLP_REJECT_CONF_REG, 0);                                  // Clear sleep rejection cause
}

void resleep()
{
    // Set the pointer of the wake stub function.
    REG_WRITE(RTC_ENTRY_ADDR_REG, (uint32_t)&esp_wake_deep_sleep);

    // Go to sleep.
    CLEAR_PERI_REG_MASK(RTC_CNTL_STATE0_REG, RTC_CNTL_SLEEP_EN);
    SET_PERI_REG_MASK(RTC_CNTL_STATE0_REG, RTC_CNTL_SLEEP_EN);
    // A few CPU cycles may be necessary for the sleep to start...
    while (true)
    {
        ;
    }
    // never reaches here.
}

void esp_wake_deep_sleep()
{
    wake_count++;
    // Enough with pseudo-random. But if ADC is not turned off for touch to wakeup this seems like a hw random.
    uint32_t rnd = REG_READ(WDEV_RND_REG);    
    // ets_printf("rnd=%d \n", rnd);
    if (rnd % 3 == 0)
    {       
        reset_timer(3 * SEC_TO_USEC);
        resleep();
    }
    else
    {
        esp_default_wake_deep_sleep();
        return;
    }
}
