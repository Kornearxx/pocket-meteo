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

    // Проверяем, находится ли стик по центру по конкретной оси
    bool x_centered = (x > JOY_CENTER_LO && x < JOY_CENTER_HI);
    bool y_centered = (y > JOY_CENTER_LO && y < JOY_CENTER_HI);

    if (btn) {
        current_state = JoyState::PRESS;
    } else if (y < JOY_THRESH_LO && x_centered) {
        current_state = JoyState::UP;
    } else if (y > JOY_THRESH_HI && x_centered) {
        current_state = JoyState::DOWN;
    } else if (x < JOY_THRESH_LO && y_centered) {
        current_state = JoyState::LEFT;
    } else if (x > JOY_THRESH_HI && y_centered) {
        current_state = JoyState::RIGHT;
    }

    bool trigger_action = false;
    
    if (current_state != last_joy_state) {
        last_joy_state = current_state;
        joy_last_ms = millis();
        trigger_action = true;
    } else if (current_state == JoyState::LEFT || current_state == JoyState::RIGHT) {
        // Плавный скролл при удержании джойстика
        if (millis() - joy_last_ms >= 150) { // Скорость прокрутки: шаг каждые 150 мс
            joy_last_ms = millis();
            trigger_action = true;
        }
    }

    if (trigger_action && current_state != JoyState::NONE) {
        // Переключение экранов только движением Вверх
        if (current_state == JoyState::UP) {
            weatherModel->display_mode = (weatherModel->display_mode == 0) ? 1 : 0;
            displayManager->setNeedsRedraw();
        } else if (current_state == JoyState::LEFT) {
            if (weatherModel->display_mode == 0) { // Только для графика
                weatherModel->graph_offset_sec += 60; // Сдвигаем камеру Влево (в прошлое)
                if (weatherModel->graph_offset_sec > 3000) weatherModel->graph_offset_sec = 3000;
                displayManager->setNeedsRedraw();
            }
        } else if (current_state == JoyState::RIGHT) {
            if (weatherModel->display_mode == 0) {
                if (weatherModel->graph_offset_sec >= 60) weatherModel->graph_offset_sec -= 60; // Возврат Вправо (к настоящему)
                else weatherModel->graph_offset_sec = 0;
                displayManager->setNeedsRedraw();
            }
        } else if (current_state == JoyState::DOWN) {
            Serial.println("[JOY] Down - задел для будущего меню");
        } else if (current_state == JoyState::PRESS) {
            Serial.println("[JOY] Pressed - вызов меню или подтверждение");
        }
    }
}