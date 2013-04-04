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

#define int32 long

int32 cx;                       // current pos
int32 cy;
int32 cz;

int32 driftsZ[MAX_DRIFTS];      // drift on x changing as we move on z axis, as all numbers in 0.1mm scale
int32 driftsX[MAX_DRIFTS];      // drift on x changing as we move on z axis, as all numbers in 0.1mm scale
int32 lastDrift;
int32 currDriftX;

int32 tx;                       // target pos
int32 ty;
int32 tz;

int32 sdelayX;                  // start delay - it decreases with each motor step until it reaches tdelay
int32 tdelayX;                  // target deleay between steps (smaller number is higher speed)
int32 sdelayY;                  // start delay - it decreases with each motor step until it reaches tdelay
int32 tdelayY;                  // target deleay between steps (smaller number is higher speed)
int32 sdelayZ;                  // start delay - it decreases with each motor step until it reaches tdelay
int32 tdelayZ;                  // target deleay between steps (smaller number is higher speed)
int32 delayStep;                // with this step is delay increased/decreased
int32 delayX;                   // current delay on x
int32 delayY;                   // current delay on y
int32 delayZ;                   // current delay on z

char cmd;                       // current command (a=axis, x,y,z=pos, r=driftx in current z, s=sdelayX, w=tdelayX, h=sdelayY, n=tdelayY, a=sdelayZ, q=tdelayZ, z=delay step, m=start motion, set current pos, q=queue start, e=execute queue)
int32 arg;                      // argument for current commands

char cmds[MAX_CMDS];            // queued commands
int32 args[MAX_CMDS];           // arguments for queued commands
int32 cmdIndex;
int32 cmdCount;
int32 queueId;

char b;
char buf[9];
int32 bufPos;

int32 lastAxis;
int32 lastAxis2;

int limitsY[80];                // limit values on y axis, 80 steps are for 360 degree turn
int32 lastOkY;                  // 

int32 rmCount;                  // are we removing material?

int32 getDriftX(int32 z)
{
    int32 i;
    int32 bestDelta = 0xfffffff;
    int32 delta;
    int32 res = 0;
    for (i = 0; i < lastDrift; i++) {
        delta = abs(driftsZ[i] - z);
        if (delta > bestDelta)
            continue;

        bestDelta = delta;
        res = driftsX[i];
    }
    return res;
}

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

int32 delayAndCheckLimit(int32 delayUs, int32 tdelay, int32 axis, bool slow)
{
    delayMicroseconds(delayUs);
    if(slow) {
        delayMicroseconds(delayUs);
    }

    if (axis != 0 && lastAxis != 0) {
        delayX = sdelayX;
        xOff();
    }
    if (axis != 1 && lastAxis != 1) {
        delayY = sdelayY;
        yOff();
    }
    if (axis != 2 && lastAxis != 2) {
        delayZ = sdelayZ;
        zOff();
    }
    if ((lastAxis == axis || lastAxis2 == axis) && delayUs > tdelay) {
        delayUs -= delayStep;   // accelerate if two of last 3 moves are on the same axis
    }
    lastAxis2 = lastAxis;
    lastAxis = axis;
    return delayUs;
}

// One step to x
void moveX(bool slow)
{
    int32 r = (cx + 0x1000000) % 8; // 0x1000000 to handle negative values

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
    delayX = delayAndCheckLimit(delayX, tdelayX, 0, slow);
}

// One step to y
void moveY()
{
    int32 r = (cy + 0x1000000) % 8;

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
    delayY = delayAndCheckLimit(delayY, tdelayY, 1, false);
}

void safeMoveY()
{
    moveY();

    int limit = analogRead(A0);
    int32 r = (cy + 80000) % 80;
    if (limitsY[r] < 0) {
        delayY = 14000;
        limitsY[r] = limit;
        return;
    }

    if ((limit > 1020) == (limitsY[r] > 1020)) {
        lastOkY = cy;
        return;
    }

    // give limit switch some tolerance
    for(int i = 1; i < 4; i++) {
        if ((limit > 1020) == (limitsY[(r+i) % 80] > 1020))
            return;

        if ((limit > 1020) == (limitsY[(r+80-i) % 80] > 1020))
            return;
    }

    int savedCy = cy;
    for (;;) {
        if (savedCy > lastOkY)
            cy++;
        else
            cy--;

        moveY();
        limit = analogRead(A0);
        if ((limit > 1020) == (limitsY[r] > 1020)) {
            cy = savedCy;
            return;
        }
    }
}

// One step to z
void moveZ()
{
    int32 r = (cz + 0x1000000) % 8;

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
    delayZ = delayAndCheckLimit(delayZ, tdelayZ, 2, false);

    currDriftX = getDriftX(cz);
}

// draw line using Bresenham's line algorithm
void drawLine(int32 x0, int32 y0, int32 x1, int32 y1)
{
    int32 dx = abs(x1 - x0);
    int32 dy = abs(y1 - y0);
    int32 sx, sy;
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
    int32 err = dx - dy;
    int32 e2;

    for (;;) {
        // move to x0,y0
        if (cx != x0) {
            bool slow = rmCount && (cx < x1);
            cx = x0;
            moveX(slow);
        }
        if (cy != y0) {
            cy = y0;
            safeMoveY();
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

void setupDelays()
{
    if(rmCount) {
        delayX = sdelayX = 5000;
        tdelayX = 4800;

        sdelayY = 3800;
        tdelayY = 3200;
    }
    else {
        delayX = sdelayX = 4000;
        tdelayX = 3200;

        delayY = sdelayY = 3600;
        tdelayY = 2600;
    }

    delayZ = sdelayZ = 12000;
    tdelayZ = 8000;

    delayStep = 50;

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

    rmCount = 1;
    setupDelays();
    delayY = 14000;    // we will probe limit during first revolution

    lastAxis = lastAxis2 = -1;

    for (int i = 0; i < 80; i++) {
        limitsY[i] = -1;
    }

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

            delayX = sdelayX;
            delayY = sdelayY;
            delayZ = sdelayZ;
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
        return;
    }
    if (cmd == 'x') {
        tx = (1250 * arg) / 109;    // 5000 x-steps = 43.6 mm
    } else if (cmd == 'y') {
        ty = (1250 * arg) / 109;    // 5000 y-steps = 43.6 mm
    } else if (cmd == 'z') {
        tz = (847 * arg) / 10;  // 874 steps = 1mm
    } else if (cmd == 'c') {
        cz = tz;
        currDriftX = getDriftX(cz);

//        if(cx != tx + currDriftX || cy != ty)
//            qDebug() << "cx=" << cx << ", tx=" << tx << ", cy=" << cy << ", ty=" << ty;

        cx = tx + currDriftX;
        cy = ty;

    } else if (cmd == 'r') {
        if (arg >= MAX_DRIFTS) {
            Serial.print("max drifts reached!");
        } else {
            lastDrift = arg;
            driftsX[lastDrift] = tx;
            driftsZ[lastDrift] = tz;
        }
    } else if (cmd == 's') {
        sdelayX = arg;
    } else if (cmd == 'w') {
        tdelayX = arg;
    } else if (cmd == 'h') {
        sdelayY = arg;
    } else if (cmd == 'n') {
        tdelayY = arg;
    } else if (cmd == 'a') {
        sdelayZ = arg;
    } else if (cmd == 'q') {
        tdelayZ = arg;
    } else if (cmd == 'p') {
        delayStep = arg;
    } else if (cmd == 'v') {
        rmCount = arg;
    } else {
        Serial.print("error: unknown command ");
        Serial.println(cmd);
    }
    cmd = 0;
}
