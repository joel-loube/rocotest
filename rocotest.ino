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
#define ADC_MAX_VALUE 1023.0f
#define SENSOR_SAMPLES 4                   // analog reads averaged per pressure sample
#define PEAK_THRESHOLD_PSI 5.0f            // decay/rise needed to confirm a peak/trough
#define MAX_DISPLAY_PSI 999.9f

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

void setup() {
  Serial.begin(19200);      //serial speed

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Clear the buffer
  display.clearDisplay();
  display.display();

  delay(500);               //hang out for half a second
  
  for(int i = 0; i < 10; i++) {
    ambientPressure += mapRawToGaugePressure(analogRead(SENSOR_PIN));
    delay(10);
  }
  ambientPressure /= 10;

  displayIntro();
}

void loop() {
  CompressionRecord rec = performTest(); 
  displayResults(rec); 
}

float mapRawToGaugePressure(int rawValue) {
  return mapfloat((float)(rawValue), (ADC_MAX_VALUE * 0.5f / 5.0f), (ADC_MAX_VALUE * 4.5f / 5.0f), 0.0f, 200.0f);
}

float scaleTo250Rpm(float pressure, int rpm) {
  // Polynomial offset is in bar; multiply by 14.5038 to convert to PSI
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

// Blocks until the pressure has bottomed out and risen PEAK_THRESHOLD_PSI above the minimum.
void waitForTrough(void) {
  float currentPressure = readPressure();
  float minimum = currentPressure;

  while ((currentPressure - minimum) < PEAK_THRESHOLD_PSI)
  {
    currentPressure = readPressure();

    if (currentPressure < minimum) {
      minimum = currentPressure;
    }
  }
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
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

  display.setTextSize(2);             // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start at top-left corner
  display.print(F("RPM: "));
  display.println(min(9999, rec.rpm)); //Write RPM

  display.setTextSize(2);             // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  
  display.print(F("F1: "));
  display.println(min(MAX_DISPLAY_PSI, rec.facePressures[0]), 1);

  display.print(F("F2: "));
  display.println(min(MAX_DISPLAY_PSI, rec.facePressures[1]), 1);

  display.print(F("F3: "));  
  display.println(min(MAX_DISPLAY_PSI, rec.facePressures[2]), 1);
  
  display.display();
}

CompressionRecord performTest(void) {
  CompressionRecord rec;
  unsigned long peakTimes[3];

  waitForTrough();                    //skip any pulse already in progress so the first face is a full peak

  for (int i = 0; i < 3; i++)         //the following code reads the sensor (in psi), looks for a peak pulse, assigns that to one of the faces, repeats the process for the next 2 faces and then moves on to the rest of the code
  {
    rec.facePressures[i] = 0;
    
    float currentPressure = readPressure();
    
    while ((rec.facePressures[i] - currentPressure) <= PEAK_THRESHOLD_PSI) // this tests whether the current value has decayed by more than the threshold, if so, we are beyond the peak of the curve
    {
      if(currentPressure > rec.facePressures[i]) {
        rec.facePressures[i] = currentPressure;
      }
      
      currentPressure = readPressure();
    }

    peakTimes[i] = millis();          //peaks are detected at the same point in each pulse, so the spacing between them is accurate
    
    if (i < 2) {
      waitForTrough();                //similarly to above, wait until we are beyond the trough of the curve
    }
  }
  
  //one peak per crank revolution, so the first and third peaks are two revolutions apart
  unsigned long elapsed = peakTimes[2] - peakTimes[0];
  rec.rpm = (elapsed > 0) ? (int)(120000UL / elapsed) : 0;

  for (int i = 0; i < 3; i++) {
    rec.facePressures[i] = scaleTo250Rpm(rec.facePressures[i], rec.rpm);
  }
  
  return rec;
}
