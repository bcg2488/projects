
#include <ICM_20948.h>
#include <Wire.h>
#include <Adafruit_ICM20948.h>
#include "Adafruit_VL53L0X.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_ICM20X.h>
#include <Servo.h>

// defines
#define SEALEVELPRESSURE_HPA (1013.25)

// Analog Current Sensor Calibration ------------------------------------------------------------------------
#define currentPin A1 // current sensor out1
#define voltagePin A0 //voltage divider pin

// voltage divider constants
#define R1 10000.0
#define R2 4700.0
#define DIVIDER_RATIO ((R1 + R2) / R2)  // ~3.12

#define VCC 5.0
#define ADC_RES 1023.0  
#define SENSITIVITY 0.040     // from the datasheet
#define ZERO_CURRENT_V 2.512  // calibrated value from 0 load

//battery attributes
#define CELLS 2
#define CELL_FULL 4.20
#define CELL_EMPTY 3.20
#define BATTERY_CAPACITY_mAh 5200.0

// filtering values
#define NUM_SAMPLES 8        // average samples
//#define IDLE_CURRENT_THRESHOLD 0.05 
#define PERCENT_SMOOTH_ALPHA 0.2    

// global Varaibles for current sensor
unsigned long lastTime = 0;
float mAhUsed = 0.0;
float smoothPercent = -1.0; //filtering varaible
//end current sensor stuff----------------------------------------------------------------------------------

#define WIRE_PORT Wire // desired Wire port.
#define AD0_VAL 1 // value of the last bit of the I2C address.

// Globals and variables
// Global sensor objects to avoid re-instantiation
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
Adafruit_ICM20948 icm;
ICM_20948_I2C imu;

// offsets and scale factors for accelerometer and magnetometer
float A_B[3]
 { -783.42,  638.76, -722.58};

float A_Ainv[3][3]
{{  1.41134,  0.01512,  0.05307},
{  0.01512,  1.15732,  0.01353},
{  0.05307,  0.01353,  0.92942}};

float M_B[3]
{  -84.89,  -39.30, -251.86};

float M_Ainv[3][3]
{{  1.25750, -0.10879, -0.09635},
{ -0.10879,  1.10039, -0.19479},
{ -0.09635, -0.19479,  0.79003}};

// local magnetic declination in degrees
//float declination = 2.81; // UTA
float declination = 3.17; // Home

float p[] = {1, 0, 0};  //X marking on sensor board points toward yaw = 0

volatile bool timerFlag = false;

Servo left;
Servo right;  
Servo backleft; 
Servo backright; 

int Speed;


// filtering function for current sensor / battery percent calculations: simple average filtering (8 samples)
float readAveragePin(int pin) {

  long sum = 0;

  for (int i = 0; i < NUM_SAMPLES; ++i) {

    sum += analogRead(pin);
    
    delay(2);
  }

  return (float)sum / NUM_SAMPLES;

}



void setup() {

  // Initialize serial communication at 115200 baud rate ---------------------------
  Serial.begin(115200);
  while (!Serial) delay(10); // Wait for serial console

  // ----------- Configure motor control -----------------------------------------
  pinMode(LED_BUILTIN, OUTPUT);

  // attach pin to ESC
  left.attach(2,1000,2000);
  right.attach(4,1000,2000);
  backleft.attach(6,1000,2000);
  backright.attach(8,1000,2000);

  // turn off motors at the start
  left.write(0);
  right.write(0);
  backleft.write(0);
  backright.write(0);

  //frontleft motor setup
  frontleft.attach(7,1000,2000);
  frontleft.write(0);
  // ------------------------------------------------------------------------------


  // Connect to sensors via i2c
  if (!lox.begin()) {
    Serial.println(F("LIDAR Error"));
    while (1);
  }

  //while (!Serial); //wait for connection
  WIRE_PORT.begin();
  WIRE_PORT.setClock(400000);
  imu.begin(WIRE_PORT, AD0_VAL);
  if (imu.status != ICM_20948_Stat_Ok) {
    Serial.println(F("ICM_20948 not detected"));
    while (1);
  }
  //if (!icm.begin_I2C()) {
    //Serial.println(F("IMU Error"));
    //while (1);
  //}

  // ----------- Set up timer 3 to interrupt every second -----------------------
  // Clear and reset for refresh
  TCCR3A = 0;   // timer counter control register A
  TCCR3B = 0;   // timer counter control register B
  TCNT3 = 0;    // timer counter set to 0

  // Set timer3 to CTC (Clear timer on Compare Match) mode
  // timer counts up to the value in OCR3A, then resets 0 and triggers an interrupt
  TCCR3B |= (1 << WGM32);

  // Set prescaler to 1024 to slow down timer efficiently
  TCCR3B |= (1 << CS32) | (1 << CS30);

  // set output compare value (1 second interrupt)
  // Calculation: (CPU_Frequency / Prescaler) / Desired_Frequency_Hz - 1
  //              (16,000,000 Hz / 1024) / 1 Hz - 1 = 15625 - 1 = 15624
  OCR3A = 15624;

  // 3 second interrupt
  //OCR3A = 47347;

  // enable timer 3 interrupt
  TIMSK3 |= (1 << OCIE3A);

  //current sensor setup
  lastTime = millis(); //initialize lastTime

  sei();

}

// Timer 3 Interrupt Service Routine
ISR(TIMER3_COMPA_vect)
{
  timerFlag = true;
}

void loop() {

  //check for incoming data
  //Serial.println("waiting for message...");

  if (timerFlag)
  {
    timerFlag = false;
    
    static float Axyz[3], Mxyz[3]; //centered and scaled accel/mag data

    // Update the sensor values whenever new data is available
    if ( imu.dataReady() ) imu.getAGMT();

    get_scaled_IMU(Axyz, Mxyz);

    Mxyz[1] = -Mxyz[1]; //align magnetometer with accelerometer (reflect Y and Z)
    Mxyz[2] = -Mxyz[2];

    //  get heading in degrees
    Serial.print("Heading: ");
    Serial.println(get_heading(Axyz, Mxyz, p, declination));

    // Determine compass (cardinal) direction
    /*
    String direction;
    if (headingDegrees >= 337.5 || headingDegrees < 22.5) {
    direction = "N";
    } else if (headingDegrees < 67.5) {
      direction = "NE";
    } else if (headingDegrees < 112.5) {
      direction = "E";
    } else if (headingDegrees < 157.5) {
      direction = "SE";
    } else if (headingDegrees < 202.5) {
      direction = "S";
    } else if (headingDegrees < 247.5) {
      direction = "SW";
    } else if (headingDegrees < 292.5) {
      direction = "W";
    } else {
      direction = "NW";
    }
  */

    // Read LIDAR
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);

    if (measure.RangeStatus != 4) 
    {
      // millimeters (mm)
      Serial.print(F("Lidar_Distance:")); Serial.println(measure.RangeMilliMeter);
    } 
    else 
    {
      //Serial.println(F("LIDAR_senses_no_obstacle(s)"));
      Serial.print(F("Lidar_Distance:")); Serial.println(0);
    }

    // current and battery calculations ----------------------------------------------------------------------------------
    //print current sensor data
    // Get ADC reading for a0
    //get battery voltage
    float rawV = readAveragePin(voltagePin);
    float vMeasured = (rawV * VCC) / ADC_RES;
    float batteryVoltage = vMeasured * DIVIDER_RATIO;

    //get battery current (from current sensor)
    float rawI = readAveragePin(currentPin);
    float vCurrent = (rawI * VCC) / ADC_RES;
    float current = (vCurrent - ZERO_CURRENT_V) / SENSITIVITY; 

    //remove small current values (noise)
    if(fabs(current) < 0.05)
    {
      current = 0.0;
    }

    //coulomb counting
    unsigned long now = millis(); // get current time for delta time
    float dt = (now - lastTime) / 1000.0f; // calculate delta time in ms
    lastTime = now;
    
    //count mAh used if time has passed.
    if (current > 0.0 && dt > 0.0) {
      mAhUsed += (current * dt) / 3.6f; // (A * s) -> mAh
    }

    //get battery value as a function of battery voltage (ADC value)
    float perCellV = batteryVoltage / CELLS;
    float percent = (perCellV - CELL_EMPTY) / (CELL_FULL - CELL_EMPTY) * 100.0;

    //clamp the output
    if(percent > 100.0) 
      percent = 100.0;
    if(percent < 0.0)
      percent = 0.0;


    if(smoothPercent < 0)
      smoothPercent = percent;

    //apply smoothing filter
    smoothPercent = PERCENT_SMOOTH_ALPHA * percent + (1 - PERCENT_SMOOTH_ALPHA) * smoothpercent;


    //current and battery calculations ---------------------------------------------------------------------
    //print results as integers
    Serial.print("Battery:");
    Serial.println((int)smoothPercent);
    Serial.print("Current:");
    Serial.println((int)current);
    
  }


  // Reading for input================================================================================================================
  if(Serial.available() > 0)
  {
    cli();  // clear interrupts
    //
    String message = Serial.readStringUntil('\n');
    message.trim();
    // Serial.print("got message");
    Serial.print(message);
    Serial.print('\n');

    int colonDex = message.indexOf(':');

    if(colonDex != -1)
    {
      //getting command from message
      String Command = message.substring(0, colonDex);
      Command.trim();

      //getting value
      String value = message.substring(colonDex + 1);
      value.trim();


      //printing command
      Serial.print("command = ");
      Serial.println(Command);

      //printing value
      Serial.print("value = ");
      Serial.println(value);

      //controlling speed
      if(Command.equals("left"))
      {
        Speed = value.toInt();
        int pulse = map(Speed, 0, 100, 1065, 2000);
        left.writeMicroseconds(pulse);

      }

      if(Command.equals("right"))
      {
        Speed = value.toInt();
        int pulse = map(Speed, 0, 100, 1055, 2000);
        right.writeMicroseconds(pulse);

      }

      if(Command.equals("backleft"))
      {
        Speed = value.toInt();
        int pulse = map(Speed, 0, 100, 1060, 2000);
        backleft.writeMicroseconds(pulse);

      }

      if(Command.equals("backright"))
      {
        Speed = value.toInt();
        int pulse = map(Speed, 0, 100, 1182, 1500);
        backright.writeMicroseconds(pulse);

      }

      if(Command.equals("stop")) //stop:1 stops all motors
      {
        right.write(0);
        left.write(0);
        backright.write(0);
        backleft.write(0);

      }

    }
    sei();  // set interrupts
  }

}
// Returns a heading (in degrees) given an acceleration vector a due to gravity, a magnetic vector m, and a facing vector p.
// applies magnetic declination
int get_heading(float acc[3], float mag[3], float p[3], float magdec)
{
  float W[3], N[3]; //derived direction vectors

  // cross "Up" (acceleration vector, g) with magnetic vector (magnetic north + inclination) with  to produce "West"
  vector_cross(acc, mag, W);
  vector_normalize(W);

  // cross "West" with "Up" to produce "North" (parallel to the ground)
  vector_cross(W, acc, N);
  vector_normalize(N);

  // compute heading in horizontal plane, correct for local magnetic declination in degrees

  float h = -atan2(vector_dot(W, p), vector_dot(N, p)) * 180 / M_PI; //minus: conventional nav, heading increases North to East
  int heading = round(h + magdec);
  heading = (heading + 720) % 360; //apply compass wrap
  return heading;
}

// subtract offsets and correction matrix to accel and mag data

void get_scaled_IMU(float Axyz[3], float Mxyz[3]) {
  byte i;
  float temp[3];
  Axyz[0] = imu.agmt.acc.axes.x;
  Axyz[1] = imu.agmt.acc.axes.y;
  Axyz[2] = imu.agmt.acc.axes.z;
  Mxyz[0] = imu.agmt.mag.axes.x;
  Mxyz[1] = imu.agmt.mag.axes.y;
  Mxyz[2] = imu.agmt.mag.axes.z;
  //apply offsets (bias) and scale factors from Magneto
  for (i = 0; i < 3; i++) temp[i] = (Axyz[i] - A_B[i]);
  Axyz[0] = A_Ainv[0][0] * temp[0] + A_Ainv[0][1] * temp[1] + A_Ainv[0][2] * temp[2];
  Axyz[1] = A_Ainv[1][0] * temp[0] + A_Ainv[1][1] * temp[1] + A_Ainv[1][2] * temp[2];
  Axyz[2] = A_Ainv[2][0] * temp[0] + A_Ainv[2][1] * temp[1] + A_Ainv[2][2] * temp[2];
  vector_normalize(Axyz);

  //apply offsets (bias) and scale factors from Magneto
  for (int i = 0; i < 3; i++) temp[i] = (Mxyz[i] - M_B[i]);
  Mxyz[0] = M_Ainv[0][0] * temp[0] + M_Ainv[0][1] * temp[1] + M_Ainv[0][2] * temp[2];
  Mxyz[1] = M_Ainv[1][0] * temp[0] + M_Ainv[1][1] * temp[1] + M_Ainv[1][2] * temp[2];
  Mxyz[2] = M_Ainv[2][0] * temp[0] + M_Ainv[2][1] * temp[1] + M_Ainv[2][2] * temp[2];
  vector_normalize(Mxyz);
}

// basic vector operations
void vector_cross(float a[3], float b[3], float out[3])
{
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

float vector_dot(float a[3], float b[3])
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void vector_normalize(float a[3])
{
  float mag = sqrt(vector_dot(a, a));
  a[0] /= mag;
  a[1] /= mag;
  a[2] /= mag;
}