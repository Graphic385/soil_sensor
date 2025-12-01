#include "hardware/adc.h"
#include "pico/printf.h"
#include "pico/sleep.h"
#include "pico/stdlib.h"

// Define analog input pin (on Pico A0 = GP26)
#define SENSOR_PIN 26
#define CHANNEL_NUMBER 0
#define sensor_power_pin 20
#define motor_driver_pin 21

// Calibration values (may need adjusting)
#define DRY 2754 // ADC value in air
#define WET 1070 // ADC value in water

#define NUM_READINGS 10 // Number of readings to average
#define SENSOR_SETTLE_MS 30
#define BETWEEN_READ_SEC 10
#define PUMP_TIME_MS 3000
#define THRESHOLD 20

int main() {
    stdio_init_all();

    // Configure adc pin
    adc_init();
    adc_gpio_init(SENSOR_PIN);
    adc_select_input(CHANNEL_NUMBER);

    // init power control pin
    gpio_init(sensor_power_pin);
    gpio_set_dir(sensor_power_pin, GPIO_OUT);
    gpio_put(sensor_power_pin, 0);

    // init motor pin
    gpio_init(motor_driver_pin);
    gpio_set_dir(motor_driver_pin, GPIO_OUT);
    gpio_put(motor_driver_pin, 0);

    static int dry_count = 0;

    while (true) {
        printf("test1\n");
        // Power sensor on
        gpio_put(sensor_power_pin, 1);
        // Wait for sensor and ADC to settle
        sleep_ms(SENSOR_SETTLE_MS);

        uint32_t sum = 0;
        for (int i = 0; i < NUM_READINGS; i++) {
            sum += adc_read();
            sleep_ms(3);
        }

        printf("test2\n");

        // Turn sensor off
        gpio_put(sensor_power_pin, 0);

        printf("test2.2\n");
        // Calculate average
        // uint16_t avg_raw = sum / NUM_READINGS;
        uint16_t avg_raw = sum / 10;

        printf("test2.4\n");
        // int32_t percent = (100 * (int32_t)(avg_raw - DRY)) / (WET - DRY);
        int32_t percent = (100 * (int32_t)(avg_raw - 2754)) / (1070 - 2754);

        printf("Wet Percent%%: %d\n", percent);

        // Clamp percent
        if (percent < 0)
            percent = 0;
        if (percent > 100)
            percent = 100;

        if (percent < 20) {
            dry_count++;
            if (dry_count >= 2) {
                gpio_put(motor_driver_pin, true);
                printf("Running pump");
                sleep_ms(PUMP_TIME_MS); // 3 second
                gpio_put(motor_driver_pin, false);
                dry_count = 0;
            }
        } else {
            dry_count = 0;
        }
        printf("test3\n");

        // Time to wait between readings
        absolute_time_t target = delayed_by_ms(get_absolute_time(), BETWEEN_READ_SEC * 1000);

        struct timespec ts;
        us_to_timespec(target, &ts);

        printf("Sleeping\n");
        sleep_goto_sleep_until(&ts, NULL);
    }

    return 0;
}