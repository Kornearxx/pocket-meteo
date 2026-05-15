#pragma once

#include <Arduino.h>
#include "Config.h"
#include "WeatherModel.h"
#include "DisplayManager.h"

enum class JoyState { NONE, UP, DOWN, LEFT, RIGHT, PRESS };

class InputManager {
public:
    InputManager();
    void begin(WeatherModel* model, DisplayManager* display);
    void handleInput();

private:
    WeatherModel* weatherModel;
    DisplayManager* displayManager;
    
    JoyState last_joy_state;
    uint32_t joy_last_ms;
    
    // Максимально мягкие пороги: джойстик сработает даже если не доходит до конца
    static constexpr int JOY_THRESH_HI = 2800; 
    static constexpr int JOY_THRESH_LO = 1200;
    // Расширенная зона покоя для исключения блокировки осей
    static constexpr int JOY_CENTER_HI = 3200;
    static constexpr int JOY_CENTER_LO = 800;
};