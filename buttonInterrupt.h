#ifndef BUTTON_INTERRUPT_H
#define BUTTON_INTERRUPT_H

#include <Arduino.h>

// Structure for button
struct Button
{
    const int PIN;
    bool pressed;
};

// Declare the buttons as extern
extern Button prevB;
extern Button pauseB;
extern Button nextB;

// Declare the shared variables as extern
extern unsigned long currentPress;
extern unsigned long lastPress;

// Function prototypes
void IRAM_ATTR prevSP();
void IRAM_ATTR pauseSP();
void IRAM_ATTR nextSP();

#endif // BUTTON_INTERRUPT_H
