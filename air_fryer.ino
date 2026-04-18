#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/io.h>

// Распиновка (физические ножки):
// PB0 (5) - OUT1, PB1 (6) - OUT2, PB2 (7) - UI, PB3 (2) - IN1, PB4 (3) - IN2

uint8_t mode = 1;
uint32_t lastTick = 0;
uint16_t flashCounter = 0;
bool ledState = false;
uint8_t pressCounter = 0;
bool isWaitingRelease = false;

uint32_t lastModeChange = 0;
bool needSave = false;

uint32_t lastImpulse1 = 0; 
uint32_t lastImpulse2 = 0;
uint32_t offTimer = 0;
bool pendingOff = false;

uint8_t EEMEM eeprom_mode_addr = 1;

// Работа с выходами (Аппаратный CTC 1кГц)
void setOutput(bool ch1, bool ch2) {
    uint8_t reg = (1 << WGM01);
    if (ch1) reg |= (1 << COM0A0); else PORTB &= ~(1 << PB0);
    if (ch2) reg |= (1 << COM0B0); else PORTB &= ~(1 << PB1);
    TCCR0A = reg;
}

void setup() {
    DDRB = (1 << PB0) | (1 << PB1) | (1 << PB2); // Выходы
    PORTB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2)); // Все в LOW

    mode = eeprom_read_byte(&eeprom_mode_addr);
    if (mode < 1 || mode > 4) mode = 1;

    _delay_ms(200);

    TCCR0A = (1 << WGM01); 
    TCCR0B = (1 << CS01) | (1 << CS00); // делитель 64
    OCR0A = 74; // 1 кГц
    
    lastImpulse1 = lastImpulse2 = 0;
    offTimer = 0;
}

void loop() {
    uint32_t ms = millis();

    // 1. ДЕТЕКТОР ВХОДА (Прямое чтение пинов)
    if (PINB & (1 << PB3)) lastImpulse1 = ms;
    if (PINB & (1 << PB4)) lastImpulse2 = ms;

    bool s1 = (ms - lastImpulse1 < 25);
    bool s2 = (ms - lastImpulse2 < 25);

    // 2. КНОПКА И ИНДИКАЦИЯ (Раз в 50мс)
    if (ms - lastTick >= 50) {
        lastTick = ms;
        
        // Опрос кнопки
        PORTB &= ~(1 << PB2); DDRB &= ~(1 << PB2); // INPUT
        PORTB |= (1 << PB2); // PULLUP
        _delay_us(100);
        bool btnNow = !(PINB & (1 << PB2)); // LOW = нажато
        DDRB |= (1 << PB2); // OUTPUT обратно

        if (btnNow) {
            if (!isWaitingRelease) {
                if (++pressCounter >= 5) {
                    if (++mode > 4) mode = 1;
                    lastModeChange = ms;
                    needSave = true;
                    isWaitingRelease = true;
                }
            }
        } else { pressCounter = 0; isWaitingRelease = false; }

        if (needSave && (ms - lastModeChange >= 5000)) {
            eeprom_update_byte(&eeprom_mode_addr, mode);
            needSave = false;
        }

        // LED логика
        flashCounter++;
        if (mode == 1) PORTB &= ~(1 << PB2);
        else if (mode == 2) PORTB |= (1 << PB2);
        else if (mode == 3) {
            if (flashCounter >= 10) { ledState = !ledState; flashCounter = 0; }
            if (ledState) PORTB |= (1 << PB2); else PORTB &= ~(1 << PB2);
        } else if (mode == 4) {
            if (flashCounter < 4 || (flashCounter >= 6 && flashCounter < 10)) PORTB |= (1 << PB2);
            else PORTB &= ~(1 << PB2);
            if (flashCounter >= 20) flashCounter = 0;
        }
    }

    // 3. РЕЖИМЫ
    bool t1 = false, t2 = false;
    if (mode == 1) { t1 = s1; t2 = s2; }
    else if (mode == 2) { t1 = s1; t2 = s1; }
    else if (mode == 3) { bool any = (s1 || s2); t1 = any; t2 = any; }
    else if (mode == 4) { t1 = s1; t2 = (s1 || s2); }

    // 4. ВЫВОД С ПАУЗОЙ
    if (!t1 && !t2) {
        if (!pendingOff) { offTimer = ms; pendingOff = true; }
        if (ms - offTimer >= 2000) setOutput(false, false);
        else {
            if (offTimer > 500) setOutput(true, true);
            else setOutput(false, false);
        }
    } else {
        pendingOff = false;
        setOutput(t1, t2);
    }
}
