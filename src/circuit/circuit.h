#ifndef CSIM_CIRCUIT_H
#define CSIM_CIRCUIT_H
#include <stdbool.h>
#include <stddef.h>

typedef enum { SIM_HIZ, SIM_LOW, SIM_HIGH, SIM_CONFLICT } sim_signal_t;
typedef enum { COMPONENT_MCU, COMPONENT_RESISTOR, COMPONENT_LED, COMPONENT_GND } component_kind_t;
typedef struct { component_kind_t kind; const char *name; unsigned ohms; } component_t;
typedef struct { unsigned component; const char *name; sim_signal_t drive; unsigned net; } pin_t;
typedef struct { unsigned a, b; } wire_t;
typedef struct { sim_signal_t signal; } net_t;
enum { PIN_PA5, PIN_RES_A, PIN_RES_B, PIN_LED_A, PIN_LED_K, PIN_GND, PIN_COUNT };
typedef struct {
    component_t components[4];
    pin_t pins[PIN_COUNT];
    wire_t wires[3];
    size_t wire_count;
    net_t nets[PIN_COUNT];
    bool led_on;
    bool resistor_valid;
} circuit_t;
sim_signal_t net_resolve(sim_signal_t a, sim_signal_t b);
bool led_is_on(sim_signal_t anode, sim_signal_t cathode);
void circuit_init(circuit_t *circuit);
void circuit_update(circuit_t *circuit, sim_signal_t pa5);
#endif
