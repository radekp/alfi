/*
  Alfi plotter/milling machine controlled with Arduino UNO board

 Alfi has 3 stepper motors connected to digital 2..13 pins
 and 3 home switches connected to A0..A2 pins

*/

int axis;       // selected axis number
int cpos;       // absolute current position - used to compute which pin will be next
int tpos;       // absolute target position 
int sdelay;     // start delay - it decreases with each mottor step until it reaches tdelay
int tdelay;     // target deleay between steps (smaller number is higher speed)

char item;      // item we are currenly reading (a=axis, p=cpos, P=tpos, d=sdelay, D=tdelay, m=start motion)
char b;
char buf[9];
int bufPos;
int val;

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
  
  item = 0;
  bufPos = 0;
  axis = 0;
  cpos = 0;
  tpos = 0;  
  sdelay = 100;
  tdelay = 30;
  
  Serial.println("arduino init ok");
}

void loop()                     
{
  byte brightness;
  
  // check if data has been sent from the computer:
  if (Serial.available()) {

    // Which item?
    if(item == 0)
    {
      item = Serial.read();
      Serial.print("selected item: ");
      Serial.write(item);
      Serial.println();
      bufPos = 0;
      return;
    }
        
    // Read integer value
    b = Serial.read();
    Serial.write(b);
    
    if(b != ' 
    ')
    {
      buf[bufPos] = b;
      bufPos++;
      return;
    }
    Serial.println("xxx");
    
    buf[bufPos] = '\0';
    val = atoi(buf);

    if(item == 'a')
    {
       axis = val;
    }
    else if(item == 'p')
    {
        cpos = val;
    }
    else if(item == 'P')
    {
        tpos = val;
        Serial.println("target position set");
    }
    else if(item == 'd')
    {
        sdelay = val;
    }    
    else if(item == 'D')
    {
        tdelay = val;
    }
    if(item == 'm')
    {
      Serial.println("motion start");
      return;    // start motion
    }
    Serial.println("SET1");
    item = 0;
    return;
  }

  if(item != 'm')
  {
    return;
    
  }
  
  if(cpos == tpos)
  {
        Serial.println("SET2");
    item = 0;    // we are done, read next command from serial
    return;
  }
  
  if(axis == 2)
  {
    if(cpos < tpos)
    {
      digitalWrite(10, HIGH);
      delay(tdelay);
      digitalWrite(10, LOW);
    
      digitalWrite(12, HIGH);
      delay(tdelay);
      digitalWrite(12, LOW);
    
      digitalWrite(13, HIGH);
      delay(tdelay);
      digitalWrite(13, LOW);
    
      digitalWrite(11, HIGH);
      delay(tdelay);
      digitalWrite(11, LOW);

      cpos++;
    }
    else
    {
      digitalWrite(13, HIGH);
      delay(tdelay);
      digitalWrite(13, LOW);

      digitalWrite(12, HIGH);
      delay(tdelay);
      digitalWrite(12, LOW);

      digitalWrite(10, HIGH);
      delay(tdelay);
      digitalWrite(10, LOW);    
    
      digitalWrite(11, HIGH);
      delay(tdelay);
      digitalWrite(11, LOW);
      cpos--;
    }
    return;
  }

  if(axis == 1)
  {
    if(cpos < tpos)
    {
      digitalWrite(4, HIGH);
      delay(tdelay);
      digitalWrite(4, LOW);

      digitalWrite(2, HIGH);
      delay(tdelay);
      digitalWrite(2, LOW);

      digitalWrite(3, HIGH);
      delay(tdelay);
      digitalWrite(3, LOW);
        
      digitalWrite(5, HIGH);
      delay(tdelay);
      digitalWrite(5, LOW);
      
      cpos++;
    }
    else
    {
      digitalWrite(3, HIGH);
      delay(tdelay);
      digitalWrite(3, LOW);

      digitalWrite(2, HIGH);
      delay(tdelay);
      digitalWrite(2, LOW);

      digitalWrite(4, HIGH);
      delay(tdelay);
      digitalWrite(4, LOW);
        
      digitalWrite(5, HIGH);
      delay(tdelay);
      digitalWrite(5, LOW);
      
      cpos--;
    }
    return;
  }
    
  if(axis == 0)
  {
    if(cpos < tpos)
    {
      digitalWrite(9, HIGH);
      delay(tdelay);
      digitalWrite(9, LOW);

      digitalWrite(7, HIGH);
      delay(tdelay);
      digitalWrite(7, LOW);

      digitalWrite(8, HIGH);
      delay(tdelay);
      digitalWrite(8, LOW);
        
      digitalWrite(6, HIGH);
      delay(tdelay);
      digitalWrite(6, LOW);
      
      cpos++;
    }
    else
    {
      digitalWrite(8, HIGH);
      delay(tdelay);
      digitalWrite(8, LOW);

      digitalWrite(7, HIGH);
      delay(tdelay);
      digitalWrite(7, LOW);

      digitalWrite(9, HIGH);
      delay(tdelay);
      digitalWrite(9, LOW);
        
      digitalWrite(6, HIGH);
      delay(tdelay);
      digitalWrite(6, LOW);
      
      cpos--;
    }
    return;
  }    

    Serial.println("SET3");
  item = 0;
  return;
}

