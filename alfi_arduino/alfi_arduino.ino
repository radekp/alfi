/*
  Alfi plotter/milling machine controlled with Arduino UNO board

 Alfi has 3 stepper motors connected to digital 2..13 pins
 and 3 home switches connected to A0..A2 pins
 
 You can control motors and inputs through on serial port from PC.
 The protocol is very simple, e.g this example:
 
 p123 m 1
 
 p is command, one of a,P,p,d,D,m
 123 is argument to command
 
 Our example sets target position to 123 and starts motion on
 default axis with default speed.

*/

int axis;       // selected axis number
int cpos;       // absolute current position - used to compute which pin will be next
int tpos;       // absolute target position 
int sdelay;     // start delay - it decreases with each motor step until it reaches tdelay
int tdelay;     // target deleay between steps (smaller number is higher speed)
int delayStep;  // with this step is delay increased/decreased
int cdelay;

char cmd;       // command we are currenly reading (a=axis, p=cpos, t=tpos, s=sdelay, d=tdelay, z=delay step, m=start motion)
char b;
char buf[9];
int bufPos;
int val;
int limit;      // last value of limit switch

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


  // initialize the serial communication
  Serial.begin(9600);
  
  cmd = 0;
  bufPos = 0;
  axis = 0;
  cpos = 0;
  tpos = 0;  
  sdelay = 6000;
  cdelay = 6000;
  tdelay = 3000;
  delayStep = 500;
  
  Serial.println("arduino init ok");
}

void loop()                     
{ 
  if(cmd == 'M')
  {
    if(cpos == tpos)
    {
      Serial.print("done ");
      Serial.println(val);
      cmd = 0;    // we are done, read next command from serial
      return;
    }
    
    int oldLimit = limit;
  
    // motion handling
    if(axis == 0)
    {
      if(cpos < tpos)  // 3 2 4 5 ->
      {
        digitalWrite(5, HIGH);
        digitalWrite(3, HIGH);    // 53
        delayMicroseconds(cdelay);
        digitalWrite(5, LOW);     // 3
        delayMicroseconds(cdelay);
        digitalWrite(2, HIGH);    // 32
        delayMicroseconds(cdelay);
        digitalWrite(3, LOW);     // 2
        delayMicroseconds(cdelay);
        digitalWrite(4, HIGH);    // 24
        delayMicroseconds(cdelay);
        digitalWrite(2, LOW);     // 4
        delayMicroseconds(cdelay);      
        digitalWrite(5, HIGH);    // 45
        delayMicroseconds(cdelay);
        digitalWrite(4, LOW);     // 5
  
        cpos++;
      }
      else  // <- 4 2 3 5
      {
        digitalWrite(5, HIGH);
        digitalWrite(4, HIGH);    // 54
        delayMicroseconds(cdelay);
        digitalWrite(5, LOW);     // 4
        delayMicroseconds(cdelay);
        digitalWrite(2, HIGH);    // 42
        delayMicroseconds(cdelay);
        digitalWrite(4, LOW);     // 2
        delayMicroseconds(cdelay);
        digitalWrite(3, HIGH);    // 23
        delayMicroseconds(cdelay);
        digitalWrite(2, LOW);     // 3
        delayMicroseconds(cdelay);
        digitalWrite(5, HIGH);    // 35
        delayMicroseconds(cdelay);
        digitalWrite(3, LOW);     // 5
        
        cpos--;
      }
      limit = analogRead(A2);    // read the limit switch
    }    
    else if(axis == 1)
    {
      if(cpos < tpos)  // ^ 8 7 9 6
      {
        digitalWrite(6, HIGH);
        digitalWrite(8, HIGH);  // 68
        delayMicroseconds(cdelay);
        digitalWrite(6, LOW);   // 8
        delayMicroseconds(cdelay);
        digitalWrite(7, HIGH);  // 87
        delayMicroseconds(cdelay);
        digitalWrite(8, LOW);   // 7
        delayMicroseconds(cdelay);
        digitalWrite(9, HIGH);  // 79
        delayMicroseconds(cdelay);
        digitalWrite(7, LOW);   // 9
        delayMicroseconds(cdelay);
        digitalWrite(6, HIGH);  // 96
        delayMicroseconds(cdelay);
        digitalWrite(9, LOW);   // 6
        
        cpos++;
      }
      else    // 9 7 8 6
      {
        digitalWrite(6, HIGH);
        digitalWrite(9, HIGH);  // 69
        delayMicroseconds(cdelay);        
        digitalWrite(6, LOW);   // 9
        delayMicroseconds(cdelay);
        digitalWrite(7, HIGH);  // 97
        delayMicroseconds(cdelay);
        digitalWrite(9, LOW);   // 7
        delayMicroseconds(cdelay);
        digitalWrite(8, HIGH);  // 78
        delayMicroseconds(cdelay);
        digitalWrite(7, LOW);   // 8
        delayMicroseconds(cdelay);
        digitalWrite(6, HIGH);  // 86
        delayMicroseconds(cdelay);
        digitalWrite(8, LOW);   // 6
        
        cpos--;
      }
      limit = analogRead(A0);    // read the limit switch
    }
    else if(axis == 2)
    {
      if(cpos < tpos)  // 13 12 10 11
      {
        digitalWrite(11, HIGH);
        digitalWrite(13, HIGH);   // 11 13
        delayMicroseconds(cdelay);
        digitalWrite(11, LOW);    // 13
        delayMicroseconds(cdelay);
        digitalWrite(12, HIGH);   // 13 12
        delayMicroseconds(cdelay);
        digitalWrite(13, LOW);    // 12
        delayMicroseconds(cdelay);
        digitalWrite(10, HIGH);   // 12 10
        delayMicroseconds(cdelay);
        digitalWrite(12, LOW);    // 10
        delayMicroseconds(cdelay);  
        digitalWrite(11, HIGH);   // 10 11
        delayMicroseconds(cdelay);
        digitalWrite(10, LOW);    // 11
  
        cpos++;
      }
      else          // 10 12 13 11
      {
        digitalWrite(11, HIGH);
        digitalWrite(10, HIGH);   // 11 10
        delayMicroseconds(cdelay);
        digitalWrite(11, LOW);    // 10
        delayMicroseconds(cdelay);
        digitalWrite(12, HIGH);   // 10 12
        delayMicroseconds(cdelay);
        digitalWrite(10, LOW);    // 12
        delayMicroseconds(cdelay);
        digitalWrite(13, HIGH);   // 12 13
        delayMicroseconds(cdelay);
        digitalWrite(12, LOW);    // 13
        delayMicroseconds(cdelay);
        digitalWrite(11, HIGH);   // 13 11
        delayMicroseconds(cdelay);
        digitalWrite(13, LOW);    // 11
        
        cpos--;
      }
      limit = analogRead(A1);    // read the limit switch
    }
    else
    {
      Serial.println("error: unknown axis");
      cmd = 0;
      return;
    }
    
    // stop motion if limit switch value changes
    if(oldLimit > 0)
    {
      int deltaL = abs(limit - oldLimit);
      if(deltaL > 512)
      {
        Serial.print("limit ");
        Serial.print(oldLimit);
        Serial.print("->");
        Serial.println(limit);
        cmd = 0;
        return;
      }
    }
    
    // Handle acceleration/decceleration
    int deltaX = abs(tpos - cpos);
    int deltaD = (sdelay - tdelay) / delayStep;
    if(deltaX > deltaD)
    {
      if(cdelay > tdelay)
      {
        cdelay -= delayStep;  // we are far from target, we can accelerate
      }
    } 
    else if(cdelay < sdelay)
    {
      cdelay += delayStep;    // we are closing to target so deccelerate
    }
    return;    
  }

  // if not moving, stop current on all motor wirings
  if(cmd == 0)
  {
    digitalWrite(13, LOW);
    digitalWrite(12, LOW);
    digitalWrite(11, LOW);
    digitalWrite(10, LOW);
    digitalWrite(9, LOW);
    digitalWrite(8, LOW);
    digitalWrite(7, LOW);
    digitalWrite(6, LOW);
    digitalWrite(5, LOW);
    digitalWrite(4, LOW);
    digitalWrite(3, LOW);
    digitalWrite(2, LOW);
  }

  // check if data has been sent from the computer:
  if (Serial.available()) {
    
    // read command
    if(cmd == 0)
    {
      cmd = Serial.read();
      bufPos = 0;
      return;
    }
        
    // read integer argument
    b = Serial.read();

    if(b != ' ')
    {
      buf[bufPos] = b;
      bufPos++;
      return;
    }
    buf[bufPos] = '\0';
    val = atoi(buf);

//    Serial.print("command ");
//    Serial.print(cmd);
//    Serial.print(" ");
//    Serial.println(val);

    if(cmd == 'm')
    {
      cmd = 'M';
      cdelay = sdelay;      // set delays, init limit and start motion
      limit = -1;
      return;
    }
    if(cmd == 'a')
    {
       axis = val;
    }
    else if(cmd == 'p')
    {
        cpos = val;
    }
    else if(cmd == 't')
    {
        tpos = val;
    }
    else if(cmd == 's')
    {
        sdelay = val;
    }    
    else if(cmd == 'd')
    {
        tdelay = val;
    }
    else if(cmd == 'z')
    {
        delayStep = val;
    }
    else {
      Serial.print("error: unknown command ");
      Serial.println(cmd);
    }
    cmd = 0;
    return;
  }  
}

