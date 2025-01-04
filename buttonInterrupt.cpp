#include <buttonInterrupt.h>

// Define buttons
Button prevB = {19, false};
Button pauseB = {18, false};
Button nextB = {5, false};

// Define shared variables
unsigned long currentPress = 0;
unsigned long lastPress = 0;
const int debouncingDelay = 250;

void IRAM_ATTR prevSP()
{
    currentPress = millis();
    if (currentPress - lastPress > debouncingDelay)
    {
        lastPress = currentPress;
        prevB.pressed = true;
    }
}
void IRAM_ATTR pauseSP()
{
    currentPress = millis();
    if (currentPress - lastPress > debouncingDelay)
    {
        lastPress = currentPress;
        pauseB.pressed = true;
    }
}
void IRAM_ATTR nextSP()
{
    currentPress = millis();
    if (currentPress - lastPress > debouncingDelay)
    {
        lastPress = currentPress;
        nextB.pressed = true;
    }
}
