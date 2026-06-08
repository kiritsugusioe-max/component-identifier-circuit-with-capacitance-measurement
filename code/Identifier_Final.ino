/* Component Identifier Final Test
   - Switch ON  -> one measurement
   - Capacitor  -> display C[nF] and T[us]
   - Resistor   -> display RESISTOR and V
   - Open       -> display OPEN and V
*/

#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

SSD1306AsciiWire oled;

const uint8_t OLED_ADDRESS = 0x3C;

// Pin assignment
const uint8_t SWITCH_PIN = 3;        // PD3 / physical pin 5 / Arduino D3
const uint8_t SIG_FREQUENCY_PIN = 8; // PB0 / ICP1 / physical pin 14 / Arduino D8
const uint8_t SIG_VOLTAGE_PIN = A1;  // PC1 / ADC1 / physical pin 24 / Arduino A1

// Switch logic
// External pull-up:
// switch OFF -> HIGH
// switch ON  -> LOW
const uint8_t SWITCH_ON_STATE  = LOW;
const uint8_t SWITCH_OFF_STATE = HIGH;
const unsigned long DEBOUNCE_MS = 100;

// Measurement settings
const uint16_t NUM_SAMPLES = 200;

// Timer1 prescaler = 1 at 16 MHz
// 1 count = 0.0625 us
const float TIMER_TICK_US = 0.0625;

// C[nF] = T[us] / CAP_COEFF_US_PER_NF
const float CAP_COEFF_US_PER_NF = 23.085;

// Valid period range
// 1 us = 16 counts
// 3750 us = 60000 counts
const uint16_t MIN_PERIOD_COUNTS = 16;
const uint16_t MAX_PERIOD_COUNTS = 60000;

// ADC settings
const float ADC_REF_VOLTAGE = 4.93;      // measured VCC
const float R_OPEN_THRESHOLD_V = 3.75;   // R/OPEN threshold

// Shared sample array
// Used for both period counts and ADC values
uint16_t samples[NUM_SAMPLES];

enum State {
  WAITING,
  MEASURING,
  DISPLAY_RESULT
};

State currentState = WAITING;

// -------------------- Switch --------------------
// ON/OFFの状態判定。100(ms)を基準に設定。
bool isSwitchStable(uint8_t targetState) {
  if (digitalRead(SWITCH_PIN) != targetState) {
    return false;
  }

  unsigned long startTime = millis();

  while (millis() - startTime < DEBOUNCE_MS) {
    if (digitalRead(SWITCH_PIN) != targetState) {
      return false;
    }
  }

  return true;
}

// -------------------- OLED --------------------

void showWaiting() {
  oled.clear();
  oled.setCursor(0, 0);     //初期設定
  oled.println("WAITING");
  oled.println();
  oled.println("Switch ON");
  oled.println("to measure");
}

void showMeasuring() {
  oled.clear();
  oled.setCursor(0, 0);
  oled.println("MEASURING...");
}

void showCapResult(float capacitanceNf, float periodUs) {
  oled.clear();
  oled.setCursor(0, 0);

  oled.println("CAPACITOR");
  oled.println();

  oled.print("C = ");
  oled.print(capacitanceNf, 2);  //Cの測定値：有効数字２桁
  oled.println(" nF");

  oled.print("T = ");
  oled.print(periodUs, 2);       //Tの測定値：有効数字2桁
  oled.println(" us");
}

void showResistorResult(float voltage) {
  oled.clear();
  oled.setCursor(0, 0);

  oled.println("RESISTOR");
  oled.println();

  oled.print("V = ");
  oled.print(voltage, 3);        //RのVoltage測定値：有効数字3桁
  oled.println(" V");
}

void showOpenResult(float voltage) {
  oled.clear();
  oled.setCursor(0, 0);

  oled.println("OPEN");
  oled.println();

  oled.print("V = ");
  oled.print(voltage, 3);       //OPENのVoltage測定値：有効数字3桁
  oled.println(" V");
}

// -------------------- Sort and average --------------------
//昇順ソート
void sortSamples(uint16_t arr[], uint16_t n) {
  for (uint16_t i = 1; i < n; i++) {
    uint16_t key = arr[i];
    int j = i - 1;

    while (j >= 0 && arr[j] > key) {
      arr[j + 1] = arr[j];
      j--;
    }

    arr[j + 1] = key;
  }
}

//中央20サンプルの平均値をとる処理
float getCentralAverage() {
  sortSamples(samples, NUM_SAMPLES);

  uint32_t sum = 0;

  // For 200 samples, use central 20 samples:
  // index 90 to 109
  for (uint16_t i = 90; i <= 109; i++) {
    sum += samples[i];
  }

  return sum / 20.0;
}

// -------------------- Timer1 Input Capture --------------------
//rising edgesを測定するための初期設定
void setupTimer1() {
  pinMode(SIG_FREQUENCY_PIN, INPUT);

  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  // ICES1 = 1: rising edge capture
  // CS10  = 1: prescaler = 1　：一番小さな分解能を利用する。
  TCCR1B = (1 << ICES1) | (1 << CS10);

  // Clear Input Capture Flag
  //flagを0にする処理であることに注意
  TIFR1 = (1 << ICF1);
}

//500(ms)測定し、rising edgeの時刻取得を試みる関数
bool waitForCapture(uint16_t &capturedValue, unsigned long timeoutMs) {
  unsigned long startTime = millis();

  while (millis() - startTime < timeoutMs) {
    if (TIFR1 & (1 << ICF1)) {           //目的信号がきたか&演算でif文確認している。
      capturedValue = ICR1;              //ICR1:到達時刻を記録しているレジスタ。
      TIFR1 = (1 << ICF1);               // clear flag
      return true;
    }
  }

  return false;
}

//1回分の周期を取得する関数
bool measureOnePeriod(uint16_t &periodCounts) {
  uint16_t t1;
  uint16_t t2;

//1回目のedge取得試み
  if (!waitForCapture(t1, 500)) {
    return false;
  }

//2回目のedge取得試み
  if (!waitForCapture(t2, 500)) {
    return false;
  }

  periodCounts = t2 - t1;
  return true;
}

bool collectPeriods() {
  uint16_t count = 0;
  unsigned long startTime = millis();

  TCNT1 = 0;
  TIFR1 = (1 << ICF1);  // clear capture flag

  while (millis() - startTime < 500) {
    uint16_t periodCounts;

    if (measureOnePeriod(periodCounts)) {
      //有効周期かどうかの判定(保守性を高める目的)
      if (periodCounts >= MIN_PERIOD_COUNTS &&
          periodCounts <= MAX_PERIOD_COUNTS) {

        samples[count] = periodCounts;
        count++;

        if (count >= NUM_SAMPLES) {
          return true;
        }
      }
    }
  }

  return false;       //この場合は"R" or "OPEN"
}

// -------------------- ADC voltage measurement --------------------

void collectAdcSamples() {
  for (uint16_t i = 0; i < NUM_SAMPLES; i++) {
    samples[i] = analogRead(SIG_VOLTAGE_PIN);
    delayMicroseconds(200);
  }
}

float adcToVoltage(float adcValue) {
  return adcValue * ADC_REF_VOLTAGE / 1023.0;
}

// -------------------- Measurement flow --------------------

void performMeasurement() {
  showMeasuring();

  // First, try capacitor detection by period measurement
  if (collectPeriods()) {
    float avgCounts = getCentralAverage();          //200サンプルの中央20サンプルの平均値を使う
    float periodUs = avgCounts * TIMER_TICK_US;
    float capacitanceNf = periodUs / CAP_COEFF_US_PER_NF;

    showCapResult(capacitanceNf, periodUs);
    return;
  }

  // If no valid periodic signal, measure SIG_VOLTAGE for R/OPEN
  collectAdcSamples();

  float adcAvg = getCentralAverage();
  float voltage = adcToVoltage(adcAvg);             //200サンプルの中央20サンプルの平均値を使う

  if (voltage > R_OPEN_THRESHOLD_V) {               //OPEN:~3.75(V), R:3.75(V)~
    showResistorResult(voltage);
  } else {
    showOpenResult(voltage);
  }
}

// -------------------- setup and loop --------------------

void setup() {
  Wire.begin();

  oled.begin(&Adafruit128x64, OLED_ADDRESS);
  oled.setFont(Adafruit5x7);

  pinMode(SWITCH_PIN, INPUT);       // external pull-up is used
  pinMode(SIG_VOLTAGE_PIN, INPUT);

  analogReference(DEFAULT);         // ADC reference = VCC

  setupTimer1();

  delay(1000);                      // avoid power-on transient

  showWaiting();
  currentState = WAITING;
}

void loop() {
  if (currentState == WAITING) {
    if (isSwitchStable(SWITCH_ON_STATE)) {
      currentState = MEASURING;             //実質不要だが、保守性向上と拡張時の問題を防ぐことを考え、あえて丁寧に現在の状態を更新している。
      performMeasurement();
      currentState = DISPLAY_RESULT;
    }
  }

  else if (currentState == DISPLAY_RESULT) {
    if (isSwitchStable(SWITCH_OFF_STATE)) {
      showWaiting();
      currentState = WAITING;
    }
  }
}