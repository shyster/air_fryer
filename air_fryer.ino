#include <util/delay.h>

#define IN1  3    // PB3 (Вывод 2)
#define IN2  4    // PB4 (Вывод 3)
#define UI   2    // PB2 (Вывод 7)
#define OUT1 0    // PB0 (Вывод 5)
#define OUT2 1    // PB1 (Вывод 6)

int mode = 1;
unsigned long lastTick = 0;
int flashCounter = 0;
bool ledState = false;
int pressCounter = 0;
bool isWaitingRelease = false;

// Инициализируем таймеры так, чтобы таймаут уже истек (минус 1000 мс)
unsigned long lastImpulse1 = -1000; 
unsigned long lastImpulse2 = -1000;
const int pulseTimeout = 25;

unsigned long offTimer = -2001; // Устанавливаем значение в прошлом
bool pendingOff = false;

void setOutput(bool ch1, bool ch2) {
  uint8_t reg = (1 << WGM01); 
  if (ch1) reg |= (1 << COM0A0); 
  else { reg &= ~(1 << COM0A0); digitalWrite(OUT1, LOW); }
  
  if (ch2) reg |= (1 << COM0B0); 
  else { reg &= ~(1 << COM0B0); digitalWrite(OUT2, LOW); }
  TCCR0A = reg;
}

void setup() {
  // Принудительно гасим выходы сразу
  pinMode(OUT1, OUTPUT);
  pinMode(OUT2, OUTPUT);
  digitalWrite(OUT1, LOW);
  digitalWrite(OUT2, LOW);
  
  pinMode(IN1, INPUT);
  pinMode(IN2, INPUT);
  pinMode(UI, OUTPUT);
  digitalWrite(UI, LOW);

  // Ждем стабилизации питания и входов (0.2 сек)
  _delay_ms(200);

  // Настройка Таймера 0 (Режим CTC)
  TCCR0A = (1 << WGM01); 
  TCCR0B = (1 << CS01) | (1 << CS00); 
  OCR0A = 74; // 1 кГц
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. ДЕТЕКТОР ВХОДА
  if (digitalRead(IN1) == HIGH) lastImpulse1 = currentMillis;
  if (digitalRead(IN2) == HIGH) lastImpulse2 = currentMillis;

  bool s1 = (currentMillis - lastImpulse1 < pulseTimeout);
  bool s2 = (currentMillis - lastImpulse2 < pulseTimeout);

  // 2. КНОПКА И LED (раз в 50 мс)
  if (currentMillis - lastTick >= 50) {
    lastTick = currentMillis;
    digitalWrite(UI, LOW);
    pinMode(UI, INPUT_PULLUP);
    _delay_us(300);
    bool btnNow = digitalRead(UI);
    pinMode(UI, OUTPUT);

    if (btnNow == LOW) {
      if (!isWaitingRelease) {
        pressCounter++;
        if (pressCounter >= 5) {
          mode++; if (mode > 3) mode = 1;
          isWaitingRelease = true;
        }
      }
    } else { pressCounter = 0; isWaitingRelease = false; }

    flashCounter++;
    if (flashCounter >= 10) { ledState = !ledState; flashCounter = 0; }
    if (mode == 1) digitalWrite(UI, LOW);
    else if (mode == 2) digitalWrite(UI, HIGH);
    else if (mode == 3) digitalWrite(UI, ledState);
  }

  // 3. ЛОГИКА РЕЖИМОВ
  bool t1 = false, t2 = false;
  if (mode == 1) { t1 = s1; t2 = s2; }
  else if (mode == 2) { t1 = s1; t2 = s1; }
  else if (mode == 3) { bool any = (s1 || s2); t1 = any; t2 = any; }

  // 4. ВЫВОД С ПАУЗОЙ 2С
   if (!t1 && !t2) {
    if (!pendingOff) { 
      offTimer = currentMillis; 
      pendingOff = true; 
    }
    // Если прошло более 2 секунд с момента пропадания сигнала
    if (currentMillis - offTimer >= 2000) {
      setOutput(false, false);
    } else {
      // Это условие сработает только если ТЭНы РЕАЛЬНО работали перед этим
      // Проверяем, не является ли это холодным стартом
      if (offTimer > 500) setOutput(true, true); 
      else setOutput(false, false);
    }
  } else {
    pendingOff = false;
    setOutput(t1, t2);
  } 
}
