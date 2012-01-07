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

#define MAX_CMDS 128

int cx;
int tx;
int cy;
int ty;
int cz;
int tz;

int axis;                       // selected axis number
int sdelay;                     // start delay - it decreases with each motor step until it reaches tdelay
int tdelay;                     // target deleay between steps (smaller number is higher speed)
int delayStep;                  // with this step is delay increased/decreased
int delayX;                     // current delay on x
int delayY;                     // current delay on y
int delayZ;                     // current delay on z

char cmd;                       // current command (a=axis, p=cpos, t=tpos, s=sdelay, d=tdelay, z=delay step, m=start motion, q=queue start, e=execute queue)
int arg;                        // argument for current commands

char cmds[MAX_CMDS];            // queued commands
int args[MAX_CMDS];             // arguments for queued commands
int cmdIndex;
int cmdCount;
int queueId;

char b;
char buf[9];
int bufPos;
int limit;                      // last value of limit switch

void delayAndCheckLimit(int delayUs, int inputNo)
{
    int oldLimit = limit;
    delayMicroseconds(delayUs);
    limit = analogRead(inputNo);    // read the limit switch

    if (oldLimit < 0) {
        return;
    }

    int deltaL = abs(limit - oldLimit);
    if (deltaL < 512) {
        return;
    }
    // stop motion if limit switch value changes
    Serial.print("limit ");
    Serial.print(oldLimit);
    Serial.print("->");
    Serial.println(limit);
    cmd = 0;
    cmdIndex = cmdCount = -1;
}

// One step to x
void moveX()
{
    int r = cx % 8;

    // 3 2 4 5
    switch (r) {
    case 7:
        digitalWrite(2, LOW);
        digitalWrite(5, LOW);
        digitalWrite(3, HIGH);
        break;                  // 3
    case 6:
        digitalWrite(2, HIGH);
        digitalWrite(3, HIGH);
        break;                  // 32
    case 5:
        digitalWrite(3, LOW);
        digitalWrite(4, LOW);
        digitalWrite(2, HIGH);
        break;                  // 2
    case 4:
        digitalWrite(2, HIGH);
        digitalWrite(4, HIGH);
        break;                  // 24
    case 3:
        digitalWrite(2, LOW);
        digitalWrite(5, LOW);
        digitalWrite(4, HIGH);
        break;                  // 4
    case 2:
        digitalWrite(4, HIGH);
        digitalWrite(5, HIGH);
        break;                  // 45
    case 1:
        digitalWrite(4, LOW);
        digitalWrite(3, LOW);
        digitalWrite(5, HIGH);
        break;                  // 5
    case 0:
        digitalWrite(5, HIGH);
        digitalWrite(3, HIGH);
        break;                  // 53
    }
    delayAndCheckLimit(delayX, A2);
    delayY = delayZ = sdelay;
    if (delayX > tdelay) {
        delayX -= delayStep;
    }
}

// One step to y
void moveY()
{
    int r = cy % 8;

    // 8 7 9 6
    switch (r) {
    case 7:
        digitalWrite(6, LOW);
        digitalWrite(7, LOW);
        digitalWrite(8, HIGH);
        break;                  // 8
    case 6:
        digitalWrite(8, HIGH);
        digitalWrite(7, HIGH);
        break;                  // 87
    case 5:
        digitalWrite(8, LOW);
        digitalWrite(9, LOW);
        digitalWrite(7, HIGH);
        break;                  // 7
    case 4:
        digitalWrite(7, HIGH);
        digitalWrite(9, HIGH);
        break;                  // 79
    case 3:
        digitalWrite(7, LOW);
        digitalWrite(6, LOW);
        digitalWrite(9, HIGH);
        break;                  // 9
    case 2:
        digitalWrite(9, HIGH);
        digitalWrite(6, HIGH);
        break;                  // 96
    case 1:
        digitalWrite(9, LOW);
        digitalWrite(8, LOW);
        digitalWrite(6, HIGH);
        break;                  // 6
    case 0:
        digitalWrite(6, HIGH);
        digitalWrite(8, HIGH);
        break;                  // 68
    }
    delayAndCheckLimit(delayY, A0);
    delayX = delayZ = sdelay;
    if (delayY > tdelay) {
        delayY -= delayStep;
    }
}

// One step to z
void moveZ()
{
    int r = cz % 8;

    // 13 12 10 11
    switch (r) {
    case 0:
        digitalWrite(11, LOW);
        digitalWrite(12, LOW);
        digitalWrite(13, HIGH);
        break;                  // 13
    case 1:
        digitalWrite(13, HIGH);
        digitalWrite(12, HIGH);
        break;                  // 13 12
    case 2:
        digitalWrite(13, LOW);
        digitalWrite(10, LOW);
        digitalWrite(12, HIGH);
        break;                  // 12
    case 3:
        digitalWrite(12, HIGH);
        digitalWrite(10, HIGH);
        break;                  // 12 10
    case 4:
        digitalWrite(12, LOW);
        digitalWrite(11, LOW);
        digitalWrite(10, HIGH);
        break;                  // 10
    case 5:
        digitalWrite(10, HIGH);
        digitalWrite(11, HIGH);
        break;                  // 10 11
    case 6:
        digitalWrite(10, LOW);
        digitalWrite(13, LOW);
        digitalWrite(11, HIGH);
        break;                  // 11
    case 7:
        digitalWrite(11, HIGH);
        digitalWrite(13, HIGH);
        break;                  // 11 13
    }
    delayAndCheckLimit(delayZ, A1);
    delayX = delayY = sdelay;
}

// draw line using Bresenham's line algorithm
void drawLine(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx, sy;
    if (x0 < x1) {
        sx = 1;
    } else {
        sx = -1;
    }
    if (y0 < y1) {
        sy = 1;
    } else {
        sy = -1;
    }
    int err = dx - dy;
    int e2;

    for (;;) {
        // move to x0,y0
        if (cx != x0) {
            cx = x0;
            moveX();
        }
        if (cy != y0) {
            cy = y0;
            moveY();
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = 2 * err;
        if (e2 > -dy) {
            err = err - dy;
            x0 = x0 + sx;
        }
        if (e2 < dx) {
            err = err + dx;
            y0 = y0 + sy;
        }
    }
}

void setup()
{
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
    cmdIndex = -1;
    cmdCount = -1;
    bufPos = -1;
    axis = 0;
    cx = cy = cz = tx = ty = tz = 0;
    sdelay = 3000;
    tdelay = 2000;
    delayStep = 50;

    Serial.println("arduino init ok");
}

void loop()
{
    if (cmd == 0 || bufPos >= 0) {
        // read next command from queue
        if (cmdIndex >= 0) {
            if (cmdIndex >= cmdCount) {
                Serial.print("qdone");
                Serial.print(queueId);
                cmdIndex = -1;  // queue executed
                cmdCount = -1;
                return;
            }
            cmd = cmds[cmdIndex];
            arg = args[cmdIndex];
            cmdIndex++;
//            Serial.print(cmd);
//            Serial.print(" ");
//            Serial.print(arg);
            return;
        }
        // if not moving, stop current on all motor wirings
        if (cmd == 0) {
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
        if (!Serial.available()) {
            return;
        }
        // read command
        if (cmd == 0) {
            cmd = Serial.read();
            Serial.write(cmd);
            arg = 0x7fffffff;
            bufPos = 0;
            return;
        }
        // read integer argument
        b = Serial.read();
        Serial.write(b);

        if (b != ' ') {
            buf[bufPos] = b;
            bufPos++;
            return;
        }
        buf[bufPos] = '\0';
        arg = atoi(buf);
        bufPos = -1;

//        Serial.print("command ");
//        Serial.print(cmd);
//        Serial.print(" ");
//        Serial.println(arg);

        if (cmd == 'q') {
            cmdCount = 0;       // start command queue
            cmd = 0;
            return;
        }
        if (cmd == 'e') {
            queueId = arg;
            cmdIndex = 0;       // execute command queue
            cmd = 0;
            return;
        }
        if (cmdCount >= 0) {
            cmds[cmdCount] = cmd;
            args[cmdCount] = arg;
            cmdCount++;
            cmd = 0;
            return;
        }
    }
    
    // motion handling
    if (cmd == 'M') {
        delayX = delayY = delayZ = sdelay;

        if (cx != tx || cy != ty) {
            drawLine(cx, cy, tx, ty);
        }
        while (cz < tz) {
            cz++;
            moveZ();
        }
        while (cz > tz) {
            cz--;
            moveZ();
        }
        if (cmdIndex < 0) {
            Serial.print("done");
            Serial.print(arg);
        }
        cmd = 0;                // we are done, read next command from serial/queue
        return;
    }

    if (cmd == 'm') {
        if (cmdCount >= 0 && cmdIndex < 0) {    // dont execute if queueing
            return;
        }
        cmd = 'M';
        limit = -1;
        return;
    }
    if (cmd == 'a') {
        axis = arg;
    } else if (cmd == 'p') {
        if (axis == 0) {
            cx = arg;
        } else if (axis == 1) {
            cy = arg;
        } else {
            cz = arg;
        }
    } else if (cmd == 't') {
        if (axis == 0) {
            tx = arg;
        } else if (axis == 1) {
            ty = arg;
        } else {
            tz = arg;
        }
    } else if (cmd == 's') {
        sdelay = arg;
    } else if (cmd == 'd') {
        tdelay = arg;
    } else if (cmd == 'z') {
        delayStep = arg;
    } else {
        Serial.print("error: unknown command ");
        Serial.println(cmd);
    }
    cmd = 0;
}
