
#include <ICM_20948.h>
#include <Wire.h>
#include <Adafruit_ICM20948.h>
#include "Adafruit_VL53L0X.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_ICM20X.h>
#include <Servo.h>
#include "SR04.h"

//ultrasonic
#define ECHO 12
#define TRIG 11
#define MAX_DISTANCE 900
#define SAMPLES 5
SR04 sensor = SR04(ECHO,TRIG);
long distance[SAMPLES];
int IND = 0;
float filteredDist;

// defines
#define SEALEVELPRESSURE_HPA (1013.25)

// Analog Current Sensor Calibration ------------------------------------------------------------------------
#define voltagePin A0 //voltage divider pin

// voltage divider constants
#define R1 10000.0
#define R2 4700.0
#define DIVIDER_RATIO ((R1 + R2) / R2)  // ~3.12

#define VCC 5.0
#define ADC_RES 1023.0  
//battery variables
#define BATTERY_CAPACITY_mAh 5200.0
float batteryPercent = 0.0;

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
Servo frontleft;
Servo frontright; 

int Speed;

//FILTERED DISTANCE 
float filteredDistance()
{
  distance[IND] = sensor.Distance();
  IND = (IND+1) % SAMPLES;

  float sum = 0;
  for(int l = 0; l < SAMPLES; l++)
  {
      sum += distance[l];
  }

  float avg = sum / SAMPLES;

  return avg;
}

float readBatVoltage(int voltagePin)
{
  float rawV = analogRead(voltagePin);
  float vMeasured = (rawV * VCC) / ADC_RES;
  float batteryVoltage = vMeasured * DIVIDER_RATIO;
  return batteryVoltage;
}



void setup() {

  // Initialize serial communication at 115200 baud rate ---------------------------
  Serial3.begin(115200);
  while (!Serial3) delay(10); // Wait for Serial3 console

  // ----------- Configure motor control -----------------------------------------
  pinMode(LED_BUILTIN, OUTPUT);

  // attach pin to ESC
  left.attach(1,1000,2000);
  right.attach(3,1000,2000);
  backleft.attach(6,1000,2000);
  backright.attach(8,1000,2000);
  frontright.attach(4,1000,2000);
  frontleft.attach(2,1000,2000);


  // turn off motors at the start
  left.write(0);
  right.write(0);
  backleft.write(0);
  backright.write(0);
  frontleft.write(0);
  frontright.write(0);

  //frontleft motor setup
  //frontleft.attach(7,1000,2000);
  //frontleft.write(0);
  // ------------------------------------------------------------------------------


  // Connect to sensors via i2c
  if (!lox.begin()) {
    Serial3.println(F("LIDAR Error"));
    while (1);
  }

  //while (!Serial3); //wait for connection
  WIRE_PORT.begin();
  WIRE_PORT.setClock(400000);
  imu.begin(WIRE_PORT, AD0_VAL);
  if (imu.status != ICM_20948_Stat_Ok) {
    Serial3.println(F("ICM_20948 not detected"));
    while (1);
  }
  //if (!icm.begin_I2C()) {
    //Serial3.println(F("IMU Error"));
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


  //ultra sonic filter setup
    //Sensor 1 filter fill
  for(int i = 0; i < SAMPLES; i++)
  {
    distance[i] = sensor.Distance();
  }

  sei();

}

// Timer 3 Interrupt Service Routine
ISR(TIMER3_COMPA_vect)
{
  timerFlag = true;
}

void loop() {

  //check for incoming data
  //Serial3.println("waiting for message...");

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
    Serial3.print("imu-heading: ");
    Serial3.println(get_heading(Axyz, Mxyz, p, declination));

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
      Serial3.print(F("distance:")); Serial3.println(measure.RangeMilliMeter);
    } 
    else 
    {
      //Serial3.println(F("LIDAR_senses_no_obstacle(s)"));
      Serial3.print(F("distance:")); Serial3.println(0);
    }

    // current and battery calculations ----------------------------------------------------------------------------------
    //print current sensor data
    // Get ADC reading for a0
    //get battery voltage




    //current and battery calculations ---------------------------------------------------------------------
    //print results as integers
    // Serial3.print("battery:");
    // Serial3.println((int)batteryPercent);

    //ultra sonic output
    Serial3.print("ultrasonic-altitude:");
    Serial3.println(filteredDist);

    
  }

  filteredDist = filteredDistance();

  float rawV = readBatVoltage(voltagePin);

  Serial3.print("ultrasonic-altitude:");
  Serial3.println(filteredDist);


    
  if(rawV >= 8.4)
  {
    batteryPercent = 100.0;
  }
  else if(rawV <= 6.4)
  {
    batteryPercent = 0.0;
  }
  else
  {
    batteryPercent = ((rawV - 6.4) / (8.4 - 6.4)) * 100.0;
  }

  // Reading for input================================================================================================================
  if(Serial3.available() > 0)
  {
   // cli();  // clear interrupts
    //
    String message = Serial3.readStringUntil('\n');
    message.trim();
    // Serial3.print("got message");
    // Serial3.println(message);

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
      // Serial3.print("command = ");
      // Serial3.println(Command);

      // //printing value
      // Serial3.print("value = ");
      // Serial.println(value);

      //controlling speed
      if(Command.equals("left-motor"))
      {
        Speed = value.toInt();
        int pulse = map(Speed, 0, 100, 1060, 2000);
        left.writeMicroseconds(pulse);

      }

      if(Command.equals("right-motor"))
      {
        Speed = value.toInt();
        int pulse = map(Speed, 0, 100, 1050, 2000);
        right.writeMicroseconds(pulse);

      }

      if(Command.equals("front-motors"))
      {
        Speed = value.toInt();

        int pulse = map(Speed, 0, 100, 1075, 2000);
        frontleft.writeMicroseconds(pulse);
        pulse = map(Speed, 0, 100, 1075, 2000);
        frontright.writeMicroseconds(pulse);

        
      }

      if(Command.equals("back-motors"))
      {
        Speed = value.toInt();

        int pulse = map(Speed, 0, 100, 1075, 2000);
        backright.writeMicroseconds(pulse);
        pulse = map(Speed, 0, 100, 1075, 2000);
        backleft.writeMicroseconds(pulse);
        
      }


      if(Command.equals("stop")) //stop:1 stops all motors
      {

        if(value.toInt() == 1)
        {
          right.write(0);
          left.write(0);
          backright.write(0);
          backleft.write(0);
          frontright.write(0);
          frontleft.write(0);
        }
      }

    }
   // sei();  // set interrupts
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