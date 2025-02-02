#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "generated/ws2812.pio.h"

#define WS2812_PIN 7
#define NUM_PIXELS 25
#define LED_RED_PIN 11
#define LED_GREEN_PIN 12
#define LED_BLUE_PIN 13
#define BUTTON_A 5
#define BUTTON_B 6

// Padrões dos números 0-9 (5x5)
const uint8_t digits[10][5][5] = {
    // 0
    {{1, 1, 1, 1, 1},
     {1, 0, 0, 0, 1},
     {1, 0, 0, 0, 1},
     {1, 0, 0, 0, 1},
     {1, 1, 1, 1, 1}},

    // 1
    {{0, 0, 1, 0, 0},
     {0, 0, 1, 1, 0},
     {0, 0, 1, 0, 0},
     {0, 0, 1, 0, 0},
     {0, 1, 1, 1, 0}},

    // 2
    {{1, 1, 1, 1, 1},
     {1, 0, 0, 0, 0},
     {1, 1, 1, 1, 1},
     {0, 0, 0, 0, 1},
     {1, 1, 1, 1, 1}},

    // 3
    {{1, 1, 1, 1, 1},
     {1, 0, 0, 0, 0},
     {1, 1, 1, 1, 0},
     {1, 0, 0, 0, 0},
     {1, 1, 1, 1, 1}},

    // 4
    {{1, 0, 0, 0, 1},
     {1, 0, 0, 0, 1},
     {1, 1, 1, 1, 1},
     {1, 0, 0, 0, 0},
     {0, 0, 0, 0, 1}},

    // 5
    {{1, 1, 1, 1, 1},
     {0, 0, 0, 0, 1},
     {1, 1, 1, 1, 1},
     {1, 0, 0, 0, 0},
     {1, 1, 1, 1, 1}},

    // 6
    {{1, 1, 1, 1, 1},
     {0, 0, 0, 0, 1},
     {1, 1, 1, 1, 1},
     {1, 0, 0, 0, 1},
     {1, 1, 1, 1, 1}},

    // 7
    {{1, 1, 1, 1, 1},
     {1, 0, 0, 0, 0},
     {0, 0, 0, 1, 0},
     {0, 0, 1, 0, 0},
     {0, 1, 0, 0, 0}},

    // 8
    {{1, 1, 1, 1, 1},
     {1, 0, 0, 0, 1},
     {1, 1, 1, 1, 1},
     {1, 0, 0, 0, 1},
     {1, 1, 1, 1, 1}},

    // 9
    {{1, 1, 1, 1, 1},
     {1, 0, 0, 0, 1},
     {1, 1, 1, 1, 1},
     {1, 0, 0, 0, 0},
     {1, 1, 1, 1, 1}}};
;

volatile int current_number = 0;
volatile uint32_t last_press_time = 0;
volatile bool update_display = false;
bool led_state = false;

PIO pio = pio0;
uint sm = 0;

void init_ws2812()
{
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, false);
}

// Defina o tipo de correção necessária
#define FLIP_HORIZONTAL 1
#define FLIP_VERTICAL 0

void update_matrix()
{
    for (int physical_row = 0; physical_row < 5; physical_row++)
    {
        for (int physical_col = 0; physical_col < 5; physical_col++)
        {
            // Aplica ambas as inversões
            int logical_row = 4 - physical_row; // Inversão vertical
            int logical_col = 4 - physical_col; // Inversão horizontal

            int index = physical_row * 5 + physical_col;
            uint32_t color = digits[current_number][logical_row][logical_col] ? 0x00FF00 : 0x000000;
            pio_sm_put_blocking(pio, sm, color << 8u);
        }
    }
}

void button_isr(uint gpio, uint32_t events)
{
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if ((now - last_press_time) > 200)
    {
        if (gpio == BUTTON_A)
            current_number = (current_number + 1) % 10;
        if (gpio == BUTTON_B)
            current_number = (current_number - 1 + 10) % 10;
        last_press_time = now;
        update_display = true;
    }
}

bool blink_callback(repeating_timer_t *timer)
{
    gpio_put(LED_RED_PIN, led_state);
    led_state = !led_state;
    return true;
}

int main()
{
    stdio_init_all();

    // Configura LEDs RGB
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, 0);
    gpio_put(LED_BLUE_PIN, 0);

    // Configura botões
    gpio_init(BUTTON_A);
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_pull_up(BUTTON_B);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &button_isr);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true);

    // Inicializa WS2812
    init_ws2812();

    // Configura timer para piscar LED vermelho
    repeating_timer_t timer;
    add_repeating_timer_ms(-100, blink_callback, NULL, &timer);

    while (true)
    {
        if (update_display)
        {
            update_matrix();
            update_display = false;
        }
        sleep_ms(10);
    }
}