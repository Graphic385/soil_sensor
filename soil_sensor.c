#include "hardware/adc.h"
#include "pico/printf.h"
#include "pico/stdlib.h"

// Define analog input pin (on Pico A0 = GP26)
#define SENSOR_PIN 26
#define CHANNEL_NUMBER 0

// Calibration values (may need adjusting)
#define DRY 2754 // ADC value in air
#define WET 1070 // ADC value in water

#define NUM_READINGS 1000 // Number of readings to average

#define motor_driver_pin 21

int main() {
    stdio_init_all(); // Initialize all standard I/O (USB Serial)
    adc_init();

    // Configure adc pin
    adc_gpio_init(SENSOR_PIN);
    adc_select_input(CHANNEL_NUMBER);

    gpio_init(motor_driver_pin);
    gpio_set_dir(motor_driver_pin, true);

    while (true) {
        uint32_t sum = 0;

        // Take NUM_READINGS readings, 1ms apart
        for (int i = 0; i < NUM_READINGS; i++) {
            uint16_t raw = adc_read();
            sum += raw;
            sleep_ms(1);
        }

        // Calculate average
        float avg_raw = sum / (float)NUM_READINGS;
        float percent = 100.0f * (avg_raw - DRY) / (WET - DRY);

        // Print to USB Serial
        printf("Raw ADC: %f Percent Wet: %.2f%%\n", avg_raw, percent);

        if (percent < 20) {
            printf("Running pump\n");
            gpio_put(motor_driver_pin, true);
            sleep_ms(3000); // 3 second
            gpio_put(motor_driver_pin, false);
        }

        // Time to wait between readings
        sleep_ms(2 * 1000);
    }

    return 0;
}