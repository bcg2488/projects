
#include <ICM_20948.h>
#include <Wire.h>
#include <Adafruit_ICM20948.h>
#include "Adafruit_VL53L0X.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_ICM20X.h>
#include <Servo.h>

// defines
#define SEALEVELPRESSURE_HPA (1013.25)
// Analog Current Sensor Calibration
#define currentPin A0    //Analog input pin
#define VCC 5.0         //Refernce voltage 
#define adcResolution 1023  //ADC resolution 10 bit
#define zeroCurrentV 2.5    //Voltage at 0A - calibrate
#define sensitivity 0.040 //40 mV per Amp for ACS758-050B
#define batteryCapacity 5200.0 // Total Battery copacity - change if we get bigger or smaller battery
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
Servo Right;  
Servo frontleft; 
Servo frontright; 
Servo backleft; 
Servo backright; 

int Speed;

// global Varaibles for current sensor
unsigned long lastTime = 0;
float mAhUsed = 0; // Total mAh used

void setup() {

  // Initialize serial communication at 115200 baud rate ---------------------------
  Serial.begin(115200);
  while (!Serial) delay(10); // Wait for serial console

  // ----------- Configure motor control -----------------------------------------
  pinMode(LED_BUILTIN, OUTPUT);

  // attach pin to ESC
  left.attach(9,1000,2000);

  // Calibrate motors
  //left motor setup
  left.attach(9,1000,2000);
  left.write(0);

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

    //print current sensor data
    // Get ADC reading for a0
    int raw = analogRead(currentPin);
    float voltage = (raw * VCC) / adcResolution;

    // Convert to current (Amps)
    float current = (voltage - zeroCurrentV) / sensitivity;

    // Time since last reading
    unsigned long now = millis();
    float dt = (now - lastTime) / 1000.0; // seconds delta time
    lastTime = now; // update lastTime

    // Coulomb counting
    mAhUsed += (current * dt) / 3.6;

    float batteryPercent = 100.0 * (1.0 - (mAhUsed / batteryCapacity));
    if (batteryPercent < 0) batteryPercent = 0;

    //print results as integers
    Serial.print("Battery:");
    Serial.println((int)batteryPercent);
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
      // Serial.print("command = ");
      // Serial.println(Command);

      // //printing value
      // Serial.print("value = ");
      // Serial.println(value);

      //controlling speed
      if(Command.equals("left"))
      {
        Speed = value.toInt();
        left.write(Speed);

      }

      if(Command.equals("right"))
      {
        Speed = value.toInt();
        //right.write(Speed);

      }

      if(Command.equals("frontleft"))
      {
        Speed = value.toInt();
        frontleft.write(Speed);

      }

      if(Command.equals("frontright"))
      {
        Speed = value.toInt();
        //frontright.write(Speed);

      }

      //just in case
      if(Command.equals("backleft"))
      {
        Speed = value.toInt();
        //backleft.write(Speed);

      }

      //just in case
      if(Command.equals("backright"))
      {
        Speed = value.toInt();
        //backright.write(Speed);

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