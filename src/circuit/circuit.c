#include "circuit.h"
#include <string.h>

sim_signal_t net_resolve(sim_signal_t a, sim_signal_t b)
{
    if (a == SIM_HIZ) return b;
    if (b == SIM_HIZ || a == b) return a;
    return SIM_CONFLICT;
}
bool led_is_on(sim_signal_t anode, sim_signal_t cathode)
{
    return anode == SIM_HIGH && cathode == SIM_LOW;
}
void circuit_init(circuit_t *c)
{
    memset(c, 0, sizeof(*c));
    c->components[0] = (component_t){COMPONENT_MCU, "STM32F446RE", 0};
    c->components[1] = (component_t){COMPONENT_RESISTOR, "R1", 330};
    c->components[2] = (component_t){COMPONENT_LED, "D1", 0};
    c->components[3] = (component_t){COMPONENT_GND, "GND", 0};
    c->pins[PIN_PA5] = (pin_t){0, "PA5", SIM_HIZ, 0};
    c->pins[PIN_RES_A] = (pin_t){1, "A", SIM_HIZ, 0};
    c->pins[PIN_RES_B] = (pin_t){1, "B", SIM_HIZ, 0};
    c->pins[PIN_LED_A] = (pin_t){2, "Anode", SIM_HIZ, 0};
    c->pins[PIN_LED_K] = (pin_t){2, "Cathode", SIM_HIZ, 0};
    c->pins[PIN_GND] = (pin_t){3, "GND", SIM_LOW, 0};
    c->wires[0] = (wire_t){PIN_PA5, PIN_RES_A};
    c->wires[1] = (wire_t){PIN_RES_B, PIN_LED_A};
    c->wires[2] = (wire_t){PIN_LED_K, PIN_GND};
    c->wire_count = 3;
    circuit_update(c, SIM_HIZ);
}
static void connect(unsigned groups[PIN_COUNT], unsigned a, unsigned b)
{
    unsigned from = groups[b], to = groups[a];
    for (unsigned i = 0; i < PIN_COUNT; ++i)
        if (groups[i] == from) groups[i] = to;
}
void circuit_update(circuit_t *c, sim_signal_t pa5)
{
    unsigned groups[PIN_COUNT];
    for (unsigned i = 0; i < PIN_COUNT; ++i) {
        groups[i] = i;
        c->nets[i].signal = SIM_HIZ;
    }
    c->pins[PIN_PA5].drive = pa5;
    for (size_t i = 0; i < c->wire_count; ++i)
        connect(groups, c->wires[i].a, c->wires[i].b);
    /* Digital abstraction: a positive resistor passes a logic level; its
     * resistance is metadata. No voltage drop/current/brightness calculation. */
    c->resistor_valid = c->components[1].ohms > 0U;
    if (c->resistor_valid) connect(groups, PIN_RES_A, PIN_RES_B);
    for (unsigned i = 0; i < PIN_COUNT; ++i) {
        c->pins[i].net = groups[i];
        net_t *net = &c->nets[groups[i]];
        net->signal = net_resolve(net->signal, c->pins[i].drive);
    }
    c->led_on = led_is_on(c->nets[groups[PIN_LED_A]].signal,
                          c->nets[groups[PIN_LED_K]].signal);
}
