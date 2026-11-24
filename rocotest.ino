#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define DEAD_SPACE_CORRECTION_FACTOR 1.04f
#define SCALE_250_RPM_A0 8.6f              // polynom Fit a0 to norm values to 250rpm
#define SCALE_250_RPM_A1 -0.048f         // polynom Fit a1 to norm values to 250rpm
#define SCALE_250_RPM_A2 0.000054f       // polynom Fit a2 to norm values to 250rpm

#define BAR_TO_PSI 14.5038f

#define SENSOR_PIN A0
#define ADC_RESOLUTION 1024.0f
#define SENSOR_SAMPLES 4                   // analog reads averaged per pressure sample
#define PEAK_THRESHOLD_PSI 5.0f            // decay/rise needed to confirm a peak/trough
#define MAX_DISPLAY_PSI 999.9f

// Cranking & stability bounds
#define MIN_VALID_RPM 120
#define MAX_VALID_RPM 500
#define PULSE_TIMEOUT_MS 1000UL
#define RPM_STABILITY_TOLERANCE 15         // max RPM spread across 3 cycles
#define PSI_STABILITY_TOLERANCE 5.0f       // max PSI spread per face across 3 cycles
#define HOLD_QUIET_MS 1500UL               // pulse-free time required to release a held result

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/*Rotary Compression Tester
This code assumes you're using a 0-200 psi - 0.5-4.5 vdc pressure transducer connected to the A0 pin.
You can use sensors with other pressure or voltage ranges but you'll need to modify the code.

Originally distributed by John Doss 2017.
refactored by Patrick McQuay, in 2020.
heavily modified using correction factors from code by Miro Bogner, in 2022.
*/

struct compressionRecord
{
    float facePressures[3];
    int rpm;
};

typedef struct compressionRecord CompressionRecord;

float ambientPressure = 0.0f;
CompressionRecord history[3];
int validStreak = 0;
int shownProgress = -1;  // cycle count currently drawn by displayProgress; -1 = not drawn

void setup() {
  Serial.begin(19200);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  display.display();

  delay(500);
  
  for(int i = 0; i < 10; i++) {
    ambientPressure += mapRawToGaugePressure(analogRead(SENSOR_PIN));
    delay(10);
  }
  ambientPressure /= 10;

  displayIntro();
  displayProgress(0);
}

void loop() {
  CompressionRecord rec;

  if (!performTest(rec)) {
    // Cranking stopped or RPM was out of bounds; reset streak
    validStreak = 0;
    displayProgress(validStreak);
    return;
  }

  // Shift new valid cycle into the 3-cycle rolling buffer
  history[0] = history[1];
  history[1] = history[2];
  history[2] = rec;

  if (validStreak < 3) {
    validStreak++;
  }
  displayProgress(validStreak);

  // Latch once 3 consecutive cycles agree within tolerance
  if (validStreak == 3 && isStableThreeCycles(history)) {
    displayResults(history[2]);
    validStreak = 0;

    // Hold result until cranking stops: no pressure swing larger than PEAK_THRESHOLD_PSI
    // for HOLD_QUIET_MS. Watching the swing rather than the absolute level means a
    // zeroing error or sensor drift can't leave us stuck here.
    unsigned long quietStart = millis();
    float lo = readPressure();
    float hi = lo;
    while (millis() - quietStart < HOLD_QUIET_MS) {
      float p = readPressure();
      lo = min(lo, p);
      hi = max(hi, p);
      if ((hi - lo) > PEAK_THRESHOLD_PSI) {
        // Still pulsing; restart the quiet window
        quietStart = millis();
        lo = hi = p;
      }
    }

    // Empty markers back on screen = ready for another run
    displayProgress(0);
  }
}

float mapRawToGaugePressure(int rawValue) {
  return mapfloat((float)(rawValue), (ADC_RESOLUTION * 0.5f / 5.0f), (ADC_RESOLUTION * 4.5f / 5.0f), 0.0f, 200.0f);
}

float scaleTo250Rpm(float pressure, int rpm) {
  rpm = constrain(rpm, MIN_VALID_RPM, MAX_VALID_RPM);
  float barCorrection = SCALE_250_RPM_A0 + (SCALE_250_RPM_A1 * rpm) + (SCALE_250_RPM_A2 * sq((float)rpm));
  return pressure + (barCorrection * BAR_TO_PSI);
}

float scaleSensorRead(int rawValue) {
  return (mapRawToGaugePressure(rawValue) - ambientPressure) * DEAD_SPACE_CORRECTION_FACTOR;
}

float readPressure(void) {
  float sum = 0.0f;
  for (int i = 0; i < SENSOR_SAMPLES; i++) {
    sum += scaleSensorRead(analogRead(SENSOR_PIN));
  }
  return sum / SENSOR_SAMPLES;
}

bool waitForTrough(unsigned long timeoutMs) {
  float currentPressure = readPressure();
  float minimum = currentPressure;
  unsigned long startTime = millis();

  while ((currentPressure - minimum) < PEAK_THRESHOLD_PSI) {
    if (millis() - startTime > timeoutMs) {
      return false;
    }
    currentPressure = readPressure();
    if (currentPressure < minimum) {
      minimum = currentPressure;
    }
  }
  return true;
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void displayIntro(void) {
  display.clearDisplay();
  display.invertDisplay(true);

  display.setTextSize(2);             
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(" Welcome!");
  display.println("  ROTARY");
  display.println("  COMP.");
  display.println("  TESTER");
  display.display();

  delay(2000);
  
  display.invertDisplay(false);
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(" Waiting.");
  display.println("  CRANK");
  display.println("  ENGINE");
  display.println("  NOW");
  display.display();
}

void displayResults(CompressionRecord rec) {
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print(F("RPM: "));
  display.println(min(9999, rec.rpm));

  display.print(F("F1: "));
  display.println(min(MAX_DISPLAY_PSI, rec.facePressures[0]), 1);

  display.print(F("F2: "));
  display.println(min(MAX_DISPLAY_PSI, rec.facePressures[1]), 1);

  display.print(F("F3: "));  
  display.println(min(MAX_DISPLAY_PSI, rec.facePressures[2]), 1);

  display.display();
  shownProgress = -1;  // screen was cleared, so the progress markers are gone
}

// Three small circles in the top-right corner, one per valid cycle collected.
// Empty circles mean ready and waiting for cranking; they fill in as consecutive
// valid cycles come in. Hidden while a result is held (until cranking stops).
// Only redraws when the count changes, to keep I2C traffic out of the sampling loop.
void displayProgress(int filled) {
  if (filled == shownProgress) {
    return;
  }

  // Clear the corner (right of the widest top-line text on any screen)
  display.fillRect(SCREEN_WIDTH - 20, 0, 20, 16, SSD1306_BLACK);

  for (int i = 0; i < 3; i++) {
    int x = SCREEN_WIDTH - 17 + (i * 7);
    if (i < filled) {
      display.fillCircle(x, 7, 2, SSD1306_WHITE);
    } else {
      display.drawCircle(x, 7, 2, SSD1306_WHITE);
    }
  }

  display.display();
  shownProgress = filled;
}

bool performTest(CompressionRecord &rec) {
  unsigned long peakTimes[3];

  if (!waitForTrough(PULSE_TIMEOUT_MS)) return false;

  for (int i = 0; i < 3; i++) {
    rec.facePressures[i] = 0.0f;
    float currentPressure = readPressure();
    unsigned long startTime = millis();

    while ((rec.facePressures[i] - currentPressure) <= PEAK_THRESHOLD_PSI) {
      if (millis() - startTime > PULSE_TIMEOUT_MS) {
        return false; // Cranking stopped mid-pulse
      }
      if (currentPressure > rec.facePressures[i]) {
        rec.facePressures[i] = currentPressure;
      }
      currentPressure = readPressure();
    }

    peakTimes[i] = millis();

    if (i < 2) {
      if (!waitForTrough(PULSE_TIMEOUT_MS)) return false;
    }
  }

  unsigned long elapsed = peakTimes[2] - peakTimes[0];
  rec.rpm = (elapsed > 0) ? (int)(120000UL / elapsed) : 0;

  if (rec.rpm < MIN_VALID_RPM || rec.rpm > MAX_VALID_RPM) {
    return false;
  }

  for (int i = 0; i < 3; i++) {
    rec.facePressures[i] = scaleTo250Rpm(rec.facePressures[i], rec.rpm);
  }

  return true;
}

bool isStableThreeCycles(CompressionRecord h[3]) {
  int minRpm = min(h[0].rpm, min(h[1].rpm, h[2].rpm));
  int maxRpm = max(h[0].rpm, max(h[1].rpm, h[2].rpm));

  if ((maxRpm - minRpm) > RPM_STABILITY_TOLERANCE) {
    return false;
  }

  for (int face = 0; face < 3; face++) {
    float minP = min(h[0].facePressures[face],
                     min(h[1].facePressures[face], h[2].facePressures[face]));
    float maxP = max(h[0].facePressures[face],
                     max(h[1].facePressures[face], h[2].facePressures[face]));

    if ((maxP - minP) > PSI_STABILITY_TOLERANCE) {
      return false;
    }
  }

  return true;
}