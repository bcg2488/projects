#include <Wire.h>
#include <Adafruit_ICM20948.h>
#include "Adafruit_VL53L0X.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_ICM20X.h>
#include <Servo.h>
#define SEALEVELPRESSURE_HPA (1013.25)

// Global sensor objects to avoid re-instantiation
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
Adafruit_ICM20948 icm;

volatile bool timerFlag = false;

Servo left;
Servo Right;  
Servo frontleft; 
Servo frontright; 
Servo backleft; 
Servo backright; 

int Speed;

void setup() {

  // Initialize serial communication at 115200 baud rate ---------------------------
  Serial.begin(115200);
  while (!Serial) delay(10); // Wait for serial console

  // ----------- Configure motor control -----------------------------------------
  pinMode(LED_BUILTIN, OUTPUT);

  // attach pin to ESC
  left.attach(9,1000,2000);

  // Calibrate motors
  digitalWrite(LED_BUILTIN, LOW);
  left.write(180);
  delay(3000);
  left.write(0);
  delay(3000);
  digitalWrite(LED_BUILTIN, HIGH);
  // ------------------------------------------------------------------------------

  // Connect to sensors via i2c
  if (!lox.begin()) {
    Serial.println(F("LIDAR Error"));
    while (1);
  }

  if (!icm.begin_I2C()) {
    Serial.println(F("IMU Error"));
    while (1);
  }

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

  // set output compare value
  // Calculation: (CPU_Frequency / Prescaler) / Desired_Frequency_Hz - 1
  //              (16,000,000 Hz / 1024) / 1 Hz - 1 = 15625 - 1 = 15624
  OCR3A = 15624;

  // enable timer 3 interrupt
  TIMSK3 |= (1 << OCIE3A);

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

    // Get IMU data 
    sensors_event_t accel, gyro, mag, temp;

    icm.getEvent(&accel, &gyro, &temp, &mag);

    //Serial.println(F("Accelerometer in m/s^2"));
    Serial.print("Accel_X:");
    Serial.println(accel.acceleration.x);
    Serial.print("Accel_Y:");
    Serial.println(accel.acceleration.y);
    Serial.print("Accel_Z:");
    Serial.println(accel.acceleration.z);

    //Serial.println(F("Magnetometer in microtesla (uT)"));
    Serial.print("Mag_X:");
    Serial.println(mag.magnetic.x);
    Serial.print("Mag_Y:");
    Serial.println(mag.magnetic.y);
    Serial.print("Mag_Z:");
    Serial.println(mag.magnetic.z);

    //Serial.println(F("Gyro in radians per second"));
    Serial.print("Gyro_X:");
    Serial.println(gyro.gyro.x);
    Serial.print("Gyro_Y:");
    Serial.println(gyro.gyro.y);
    Serial.print("Gyro_Z:");
    Serial.println(gyro.gyro.z);

    //delay(1000);

    // Read LIDAR
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);

    if (measure.RangeStatus != 4) 
    {
      Serial.print(F("Lidar_Distance:")); Serial.println(measure.RangeMilliMeter); //Serial.println(F(" mm"));
    } 
    else 
    {
      //Serial.println(F("LIDAR_senses_no_obstacle(s)"));
      Serial.print(F("Lidar_Distance:")); Serial.println(0);
    }
  }

  if(Serial.available() > 0)
  {
    // Disable interrupt
    //TIMSK3 &= ~(1 << OCIE3A);
    cli();

    String message = Serial.readStringUntil('\n');
    message.trim();
    Serial.print("got message");
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
        //frontleft.write(Speed);

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
    // Re-enable interrupt
    // Reset counter
    //TIMSK3 = (1 << OCIE3A);
    //TCNT3 = 0;
    sei();
  }

}
