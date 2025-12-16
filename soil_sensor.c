#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/powman.h"
#include "hardware/structs/usb.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include <stdio.h>

// ----------- USER SETTINGS --------------
#define SENSOR_PIN 26
#define CHANNEL_NUMBER 0
#define SENSOR_POWER_PIN 20
#define MOTOR_PIN 21

#define DRY 2754
#define WET 1070

#define NUM_READINGS 10
#define SENSOR_SETTLE_MS 30
#define PUMP_TIME_MS 3000
#define WAKE_INTERVAL_MS 10000 // how long to sleep (10 sec)
#define THRESHOLD_PERCENT 20

static powman_power_state off_state;
static powman_power_state on_state;

// Initialise everything
void powman_init(uint64_t abs_time_ms) {
    // start powman and set the time
    powman_timer_start();
    powman_timer_set_ms(abs_time_ms);

    // Allow power down when debugger connected
    powman_set_debug_power_request_ignored(true);

    // Power states
    powman_power_state P1_7 = POWMAN_POWER_STATE_NONE;

    powman_power_state P0_3 = POWMAN_POWER_STATE_NONE;
    P0_3 = powman_power_state_with_domain_on(P0_3, POWMAN_POWER_DOMAIN_SWITCHED_CORE);
    P0_3 = powman_power_state_with_domain_on(P0_3, POWMAN_POWER_DOMAIN_XIP_CACHE);

    off_state = P1_7;
    on_state = P0_3;
}

// Initiate power off
static int powman_off(void) {
    // Get ready to power off
    stdio_flush();

    // Set power states
    bool valid_state = powman_configure_wakeup_state(off_state, on_state);
    if (!valid_state) {
        return PICO_ERROR_INVALID_STATE;
    }

    // reboot to main
    powman_hw->boot[0] = 0;
    powman_hw->boot[1] = 0;
    powman_hw->boot[2] = 0;
    powman_hw->boot[3] = 0;

    // Switch to required power state
    int rc = powman_set_power_state(off_state);
    if (rc != PICO_OK) {
        return rc;
    }

    // Power down
    while (true)
        __wfi();
}

// Power off chip for a given time in milliseconds
int powman_off_for_ms(uint64_t duration_ms) {

    // Get current time from powman timer
    uint64_t now_ms = powman_timer_get_ms();
    uint64_t wake_time_ms = now_ms + duration_ms;

    // Set powman alarm to wake at the target time
    powman_enable_alarm_wakeup_at_ms(wake_time_ms);

    // Enter powman OFF mode — this never returns
    return powman_off();
}

static void disable_usb() {
    usb_hw->phy_direct = USB_USBPHY_DIRECT_TX_PD_BITS |
                         USB_USBPHY_DIRECT_RX_PD_BITS |
                         USB_USBPHY_DIRECT_DM_PULLDN_EN_BITS |
                         USB_USBPHY_DIRECT_DP_PULLDN_EN_BITS;

    usb_hw->phy_direct_override =
        USB_USBPHY_DIRECT_RX_DM_BITS | USB_USBPHY_DIRECT_RX_DP_BITS |
        USB_USBPHY_DIRECT_RX_DD_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_TX_DIFFMODE_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_DM_PULLUP_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_TX_FSSLEW_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_TX_PD_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_RX_PD_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_TX_DM_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_TX_DP_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_TX_DM_OE_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_TX_DP_OE_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_DM_PULLDN_EN_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_DP_PULLDN_EN_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_DP_PULLUP_EN_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_DM_PULLUP_HISEL_OVERRIDE_EN_BITS |
        USB_USBPHY_DIRECT_OVERRIDE_DP_PULLUP_HISEL_OVERRIDE_EN_BITS;
}

int main() {
    // ---- POWMAN BOILERPLATE (required) ----
    set_sys_clock_48mhz();    // run from pll_usb, disable pll_sys
    gpio_set_dir_all_bits(0); // all GPIO inputs
    for (int i = 2; i < NUM_BANK0_GPIOS; ++i) {
        gpio_set_function(i, GPIO_FUNC_SIO);
        if (i > NUM_BANK0_GPIOS - NUM_ADC_CHANNELS) {
            gpio_disable_pulls(i);
            gpio_set_input_enabled(i, false);
        }
    }
    hw_set_bits(&powman_hw->vreg_ctrl,
                POWMAN_PASSWORD_BITS | POWMAN_VREG_CTRL_UNLOCK_BITS);

    stdio_init_all();

    disable_usb();

    // initialize powman example helper (time doesn't matter)
    powman_init(1704067200000);
    // ----------------------------------------

    // Persistent variable across resets (powman scratch register)
    // scratch[0]   = dry_count
    // scratch[1]   = boot counter (just for debugging)
    uint32_t dry_count = powman_hw->scratch[0];
    powman_hw->scratch[1]++;

    printf("\nWakeup #%u  dry_count=%u\n",
           powman_hw->scratch[1], dry_count);

    // ----------- SETUP HARDWARE -------------
    adc_init();
    adc_gpio_init(SENSOR_PIN);
    adc_select_input(CHANNEL_NUMBER);

    gpio_init(MOTOR_PIN);
    gpio_set_dir(MOTOR_PIN, GPIO_OUT);
    gpio_put(MOTOR_PIN, 0);

    gpio_init(SENSOR_POWER_PIN);
    gpio_set_dir(SENSOR_POWER_PIN, GPIO_OUT);
    gpio_put(SENSOR_POWER_PIN, 0);
    // ----------------------------------------

    // ----- READ SENSOR -----
    gpio_put(SENSOR_POWER_PIN, 1);
    sleep_ms(SENSOR_SETTLE_MS);

    uint32_t sum = 0;
    for (int i = 0; i < NUM_READINGS; i++) {
        sum += adc_read();
        sleep_ms(3);
    }
    gpio_put(SENSOR_POWER_PIN, 0);

    uint16_t avg_raw = sum / NUM_READINGS;
    int32_t percent = (100 * (int32_t)(avg_raw - DRY)) / (WET - DRY);

    printf("Raw ADC: %u  Percent Wet: %d%%\n", avg_raw, percent);

    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;

    // ----- DRY LOGIC -----
    if (percent < THRESHOLD_PERCENT) {
        dry_count++;
        if (dry_count >= 2) {
            printf("Pump ON\n");
            gpio_put(MOTOR_PIN, 1);
            sleep_ms(PUMP_TIME_MS);
            gpio_put(MOTOR_PIN, 0);
            dry_count = 0;
        }
    } else {
        dry_count = 0;
    }

    // Save back to powman scratch RAM
    powman_hw->scratch[0] = dry_count;

    printf("Sleeping for %d ms (powman OFF)\n", WAKE_INTERVAL_MS);
    sleep_ms(10);

    // ----- ENTER POWMAN-OFF SLEEP -----
    int rc = powman_off_for_ms(WAKE_INTERVAL_MS);
    hard_assert(rc == PICO_OK);

    hard_assert(false); // never returns
    return 0;
}