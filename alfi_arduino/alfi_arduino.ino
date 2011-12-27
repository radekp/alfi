/*
  Alfi plotter/milling machine controlled with Arduino UNO board

 Alfi has 3 stepper motors connected to digital 2..13 pins
 and 3 home switches connected to A0..A2 pins

*/

void setup()   {                

  // 12 digitals outputs for 3 stepper motors
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);

  // activate pullup resistors on A0..A2  
  pinMode(A0, OUTPUT);
  digitalWrite(A0, HIGH);
  pinMode(A1, OUTPUT);
  digitalWrite(A1, HIGH);
  pinMode(A2, OUTPUT);
  digitalWrite(A2, HIGH);

}

// the loop() method runs over and over again,
// as long as the Arduino has power

// 9 11 10 8

void loop()                     
{

  digitalWrite(10, HIGH);   // set the LED on
  delay(10);                  // wait for a second
  digitalWrite(10, LOW);   // set the LED on

  digitalWrite(12, HIGH);   // set the LED on
  delay(10);                  // wait for a second
  digitalWrite(12, LOW);   // set the LED on

  digitalWrite(13, HIGH);   // set the LED on
  delay(10);                  // wait for a second
  digitalWrite(13, LOW);   // set the LED on

  digitalWrite(11, HIGH);   // set the LED on
  delay(10);                  // wait for a second
  digitalWrite(11, LOW);   // set the LED on


  digitalWrite(4, HIGH);   // set the LED on
  delay(50);                  // wait for a second
  digitalWrite(4, LOW);   // set the LED on

  digitalWrite(2, HIGH);   // set the LED on
  delay(50);                  // wait for a second
  digitalWrite(2, LOW);   // set the LED on

  digitalWrite(3, HIGH);   // set the LED on
  delay(50);                  // wait for a second
  digitalWrite(3, LOW);   // set the LED on


  digitalWrite(5, HIGH);   // set the LED on
  delay(50);                  // wait for a second
  digitalWrite(5, LOW);   // set the LED on

 
  digitalWrite(9, HIGH);   // set the LED on
  delay(10);                  // wait for a second
  digitalWrite(9, LOW);   // set the LED on

  digitalWrite(7, HIGH);   // set the LED on
  delay(10);                  // wait for a second
  digitalWrite(7, LOW);   // set the LED on


  digitalWrite(8, HIGH);   // set the LED on
  delay(10);                  // wait for a second
  digitalWrite(8, LOW);   // set the LED on

  digitalWrite(6, HIGH);   // set the LED on
  delay(10);                  // wait for a second
  digitalWrite(6, LOW);   // set the LED on
}

