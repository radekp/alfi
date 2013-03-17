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
#define MAX_DRIFTS 64

long cx;                        // current pos
long cy;
long cz;

long driftsZ[MAX_DRIFTS];       // drift on x changing as we move on z axis, as all numbers in 0.1mm scale
long driftsX[MAX_DRIFTS];       // drift on x changing as we move on z axis, as all numbers in 0.1mm scale
int lastDrift;
long currDriftX;

long tx;                        // target pos
long ty;
long tz;

int sdelay;                     // start delay - it decreases with each motor step until it reaches tdelay
int tdelay;                     // target deleay between steps (smaller number is higher speed)
int delayStep;                  // with this step is delay increased/decreased
long delayX;                     // current delay on x
long delayY;                     // current delay on y
long delayZ;                     // current delay on z

char cmd;                       // current command (a=axis, x,y,z=pos, r=driftx in current z, s=sdelay, d=tdelay, z=delay step, m=start motion, set current pos, q=queue start, e=execute queue)
long arg;                        // argument for current commands

char cmds[MAX_CMDS];            // queued commands
long args[MAX_CMDS];             // arguments for queued commands
int cmdIndex;
int cmdCount;
int queueId;

char b;
char buf[9];
int bufPos;
int limit;                      // last value of limit switch

int lastAxis;
int lastAxis2;

void xOff()
{
    digitalWrite(2, LOW);
    digitalWrite(3, LOW);
    digitalWrite(4, LOW);
    digitalWrite(5, LOW);
}

void yOff()
{
    digitalWrite(6, LOW);
    digitalWrite(7, LOW);
    digitalWrite(8, LOW);
    digitalWrite(9, LOW);
}

void zOff()
{
    digitalWrite(10, LOW);
    digitalWrite(11, LOW);
    digitalWrite(12, LOW);
    digitalWrite(13, LOW);
}

int delayAndCheckLimit(int delayUs, int inputNo, int axis)
{
    int oldLimit = limit;
    delayMicroseconds(delayUs);
    limit = analogRead(inputNo);    // read the limit switch

    if (oldLimit >= 0) {
        int deltaL = abs(limit - oldLimit);
        if (deltaL > 512) {
            // stop motion if limit switch value changes
            Serial.print("limit ");
            Serial.print(oldLimit);
            Serial.print("->");
            Serial.println(limit);
            cmd = 0;
            cmdIndex = cmdCount = -1;
        }
    }
    if (axis != 0 && lastAxis != 0) {
        delayX = sdelay;
        xOff();
    }
    if (axis != 1 && lastAxis != 1) {
        delayY = sdelay;
        yOff();
    }
    if (axis != 2 && lastAxis != 2) {
        delayZ = sdelay;
        zOff();
    }
    if ((lastAxis == axis || lastAxis2 == axis) && delayUs > tdelay) {
        delayUs -= delayStep;    // accelerate if two of last 3 moves are on the same axis
    }
    lastAxis2 = lastAxis;
    lastAxis = axis;
    return delayUs;
}

// One step to x
void moveX()
{
    long r = (cx + 0x1000000) % 8;          // 0x1000000 to handle negative values

    // 3 2 4 5
    switch (r) {
    case 0:
        digitalWrite(2, LOW);
        digitalWrite(5, LOW);
        digitalWrite(3, HIGH);
        break;                  // 3
    case 1:
        digitalWrite(2, HIGH);
        digitalWrite(3, HIGH);
        break;                  // 32
    case 2:
        digitalWrite(3, LOW);
        digitalWrite(4, LOW);
        digitalWrite(2, HIGH);
        break;                  // 2
    case 3:
        digitalWrite(2, HIGH);
        digitalWrite(4, HIGH);
        break;                  // 24
    case 4:
        digitalWrite(2, LOW);
        digitalWrite(5, LOW);
        digitalWrite(4, HIGH);
        break;                  // 4
    case 5:
        digitalWrite(4, HIGH);
        digitalWrite(5, HIGH);
        break;                  // 45
    case 6:
        digitalWrite(4, LOW);
        digitalWrite(3, LOW);
        digitalWrite(5, HIGH);
        break;                  // 5
    case 7:
        digitalWrite(5, HIGH);
        digitalWrite(3, HIGH);
        break;                  // 53
    }
    delayX = delayAndCheckLimit(delayX, A2, 0);
}

// One step to y
void moveY()
{
    long r = (cy + 0x1000000) % 8;

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
    delayY = delayAndCheckLimit(delayY, A0, 1);
}

// One step to z
void moveZ()
{
    long r = (cz + 0x1000000) % 8;

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
    delayZ = delayAndCheckLimit(delayZ, A1, 2);

    int i;
    long bestDelta = 0xfffffff;
    long delta;
    for(i = 0; i < lastDrift; i++) {
        delta = abs(driftsZ[i] - cz);
        if(delta > bestDelta)
            continue;

        bestDelta = delta;
        currDriftX = driftsX[i];
    }
}

// draw line using Bresenham's line algorithm
void drawLine(long x0, long y0, long x1, long y1)
{
    long dx = abs(x1 - x0);
    long dy = abs(y1 - y0);
    long sx, sy;
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
    long err = dx - dy;
    long e2;

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
    Serial.begin(115200);

    cmd = 0;
    cmdIndex = -1;
    cmdCount = -1;
    bufPos = -1;
    cx = cy = cz = tx = ty = tz = 0;
    memset(driftsX, 0, MAX_DRIFTS);
    memset(driftsZ, 0, MAX_DRIFTS);
    currDriftX = 0;
    lastDrift = -1;
    sdelay = 3600;
    tdelay = 2400;
    delayStep = 50;
    delayX = delayY = delayZ = sdelay;
    lastAxis = lastAxis2 = -1;

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
        // if not moving, stop current on all motor wirings and reset delays
        if (cmd == 0) {
            xOff();
            yOff();
            zOff();
     
            delayX = delayY = delayZ = sdelay;
            lastAxis = lastAxis2 = -1;
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
        arg = atol(buf);
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
        while (cz < tz) {
            cz++;
            moveZ();
            zOff();
        }
        while (cz > tz) {
            cz--;
            moveZ();
            zOff();
        }
        if (cx != tx + currDriftX || cy != ty) {
            drawLine(cx, cy, tx + currDriftX, ty);
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
    if (cmd == 'x') {
        tx = (1250 * arg) / 109;        // 5000 x-steps = 43.6 mm
    } else if (cmd == 'y') {
        ty = (1250 * arg) / 109;        // 5000 y-steps = 43.6 mm
    } else if (cmd == 'z') {
        tz = (847 * arg) / 10;            // 874 steps = 1mm
    } else if (cmd == 'c') {
        cx = tx;
        cy = ty;
        cz = tz;
    } else if (cmd == 'r') {
        if(arg >= MAX_DRIFTS) {
            Serial.print("max drifts reached!");
        } else {
            lastDrift = arg;
            driftsX[lastDrift] = tx;
            driftsZ[lastDrift] = tz;
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
