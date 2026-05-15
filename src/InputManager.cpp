#include "InputManager.h"

InputManager::InputManager() 
    : weatherModel(nullptr), displayManager(nullptr), last_joy_state(JoyState::NONE), joy_last_ms(0) {}

void InputManager::begin(WeatherModel* model, DisplayManager* display) {
    weatherModel = model;
    displayManager = display;
    
    pinMode(JOY_BTN_PIN, INPUT_PULLUP);
    // Аналоговые пины (X, Y) не требуют явного вызова pinMode на ESP32
}

void InputManager::handleInput() {
    if (!weatherModel || !displayManager) return;
    if (millis() - joy_last_ms < DEBOUNCE_MS) return;

    int x = analogRead(JOY_X_PIN);
    int y = analogRead(JOY_Y_PIN);
    bool btn = (digitalRead(JOY_BTN_PIN) == LOW);

    JoyState current_state = JoyState::NONE;

    if (btn) {
        current_state = JoyState::PRESS;
    } else if (y < JOY_THRESH_LO) {
        current_state = JoyState::UP;
    } else if (y > JOY_THRESH_HI) {
        current_state = JoyState::DOWN;
    } else if (x < JOY_THRESH_LO) {
        current_state = JoyState::LEFT;
    } else if (x > JOY_THRESH_HI) {
        current_state = JoyState::RIGHT;
    }

    if (current_state != last_joy_state) {
        last_joy_state = current_state;
        joy_last_ms = millis();

        // Переключение экранов только движением Вверх
        if (current_state == JoyState::UP) {
            weatherModel->display_mode = (weatherModel->display_mode == 0) ? 1 : 0;
            displayManager->setNeedsRedraw();
        } else if (current_state == JoyState::DOWN || current_state == JoyState::LEFT || current_state == JoyState::RIGHT) {
            Serial.println("[JOY] Down/Left/Right - задел для будущего меню");
        } else if (current_state == JoyState::PRESS) {
            Serial.println("[JOY] Pressed - вызов меню или подтверждение");
        }
    }
}