#include <Servo.h>

Servo left;
Servo Right;  
Servo frontleft; 
Servo frontright; 
Servo backleft; 
Servo backright; 

int Speed; 

void setup(){
  pinMode(LED_BUILTIN, OUTPUT);

  //attching pin to ESC
  left.attach(9,1000,2000);
  //right.attach(...);


  // //setup UART
  Serial.begin(115200);
  Serial.println("hi there");

   //calibrate motors
  digitalWrite(LED_BUILTIN, LOW);

  left.write(180);
  //right.write(180...);

  delay(3000);

  left.write(0);
  //right.write(0);

  delay(3000);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop(){
  //check for incoming data
  if(Serial.available() > 0)
  {
    //
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
    
  }
  
}