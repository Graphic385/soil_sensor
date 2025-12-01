#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/powman.h"
#include "hardware/structs/usb.h"
#include "pico/stdlib.h"
#include "powman_example.h" // gives powman_example_init() and powman_example_off_for_ms()
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
// ----------------------------------------

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
    powman_example_init(1704067200000ULL);
    // ----------------------------------------

    // Persistent variable acro
