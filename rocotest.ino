#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/*TR-01 Rotary Compression Tester
Originally distributed by John Doss 2017

This code assumes you're using a 0-200 psi - 0.5-4.5 vdc pressure transducer connected to the AI0 pin.
You can use sensors with other pressure or voltage ranges but you'll need to modify the code.

refactored by Patrick McQuay, in the year of our lord COVID 19, 2020.
*/

#define SENSOR 0          //Analog input pin that sensor is connected too

int face[3];              //3 faces per rotor
int v;                    //voltage from pressure sensor
int i;                    //counting faces
int minimum;              //psi to drop before new face

unsigned long time;       //time for calc RPM
unsigned long OldTime;    //OldTime for calc RPM
unsigned long RPM;        //RPM variable

float ambientPressure;

void setup() {
  Serial.begin(9600);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Clear the buffer
  display.clearDisplay();
  display.display();

  Serial.begin(19200);      //serial speed
  delay(500);               //hang out for half a second
  
  Serial.println("      TR-01 Open-Source");
  Serial.println("Rotary Engine Compression Tester");
  Serial.println("          Firmware v0.1");
  Serial.println(""); 

  for(int i = 0; i < 10; i++) {
    ambientPressure += mapRawToAbsolutePressure(analogRead(SENSOR));
    delay(10);
  }
  ambientPressure /= 10;

  RCTintro();
}

void loop() {
  RCTtest(); 
  RCTdisplay(); 
}

float mapRawToAbsolutePressure(int rawValue) {
  return mapfloat((float)(rawValue), (1024.0f * 0.5f / 5.0f), (1024.0f * 4.5f / 5.0f), 0.0f, 200.0f);
}

int scaleSensorRead(int rawValue) {
  return mapRawToAbsolutePressure(rawValue) - ambientPressure;
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void RCTintro(void) {
  display.clearDisplay();

  display.invertDisplay(true);

  display.setTextSize(2);             
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start at top-left corner
  display.println(" Welcome!");

  display.println("  ROTARY");
  display.println("  COMP.");
  display.println("  TESTER");
  
  display.display();

  delay(2000);
  display.invertDisplay(false);
}

void RCTdisplay(void) {
  display.clearDisplay();

  display.setTextSize(2);             // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start at top-left corner
  display.print(F("RPM: "));
  display.println(RPM); //Write RPM

  display.setTextSize(2);             // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  
  display.print(F("F1: "));
  display.println(face[0]);

  display.print(F("F2: "));
  display.println(face[1]);

  display.print(F("F3: "));  
  display.println(face[2]);
  
  display.display();
}

void RCTtest(void) {
  OldTime = millis();           //record cycle begining time for RPM calculation
  
  Serial.print("PSI: ");
  
  for (i = 0; i < 3; i++)         //the following code reads the sensor (in psi), looks for a peak pulse, assigns that to one of the faces, repeats the process for the next 2 faces and then moves on to the rest of the code
  {
    face[i] = 0;
    
    v = scaleSensorRead(analogRead(SENSOR));
    
    while ((face[i] - v) <= 5) // this tests whether the current value has decayed by more than 5, if so, we are beyond the peak of the curve
    {
      if(v > face[i]) {
        face[i] = v;
      }
      
      v = scaleSensorRead(analogRead(SENSOR));
    }   
    
    minimum=v;
    
    while((v - minimum) < 5) //similarly to above tests if we have risen by more than 5, if so we are beyond the trough of the curve
    {
      v = scaleSensorRead(analogRead(SENSOR));
      
      if (v < minimum) {
        minimum = v;
      }
    }

    Serial.print(face[i]);
    Serial.print(" ");
  }
  
  time = millis();
  RPM = (180000 / (time - OldTime));

  Serial.print("RPM: ");
  Serial.print(RPM);
  Serial.println();
}
