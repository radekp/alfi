#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/types.h>
#include <fcntl.h>

// Alfi binary protocol:
// byte no:
//
// 0 - scale
// 1..x - commands
//
// Alfi binary commands
//
// 0 - write 0 to lpt and stop print
// 1..8 - directions:
//
//   8 1 2
//   7   3
//   6 5 4

#define PRN_WIDTH 2047
#define PRN_HEIGHT 2047

#define QUEUE_LEN 30

QFile *outFile = NULL;

uchar *prnBits;

void openOutFile(QString name)
{
    if (outFile) {
        outFile->close();
        delete outFile;
    }
    outFile = new QFile(name);
    outFile->open(QFile::WriteOnly | QFile::Truncate);
}

void closeOutFile()
{
    outFile->close();
    outFile = NULL;
}

static void MkPrnImg(QImage & img, int width, int height, uchar ** imgBits)
{
    img = QImage(width, height, QImage::Format_Indexed8);

    img.setNumColors(2);
    img.setColor(0, qRgb(255, 255, 255));
    img.setColor(1, qRgb(255, 0, 0));
    img.setColor(2, qRgb(0, 255, 0));
    img.setColor(3, qRgb(0, 0, 255));
    img.setColor(4, qRgb(0, 0, 0));
    img.setColor(5, qRgb(255, 0, 0));
    img.setColor(6, qRgb(0, 255, 0));
    img.setColor(7, qRgb(0, 0, 255));
    img.setColor(8, qRgb(0, 0, 0));
    img.setColor(9, qRgb(255, 0, 0));
    img.setColor(10, qRgb(0, 255, 0));
    img.setColor(11, qRgb(0, 0, 255));
    img.setColor(12, qRgb(0, 0, 0));
    img.setColor(13, qRgb(255, 0, 0));
    img.setColor(14, qRgb(0, 255, 0));
    img.setColor(15, qRgb(0, 0, 255));
    img.setColor(16, qRgb(0, 0, 0));
    img.setColor(17, qRgb(255, 0, 0));
    img.setColor(18, qRgb(0, 255, 0));
    img.setColor(19, qRgb(0, 0, 255));
    img.setColor(20, qRgb(0, 0, 0));
    img.setColor(21, qRgb(0, 255, 0));
    img.setColor(22, qRgb(0, 0, 255));
    img.setColor(23, qRgb(0, 0, 0));
    img.setColor(24, qRgb(255, 0, 0));
    img.setColor(25, qRgb(0, 255, 0));
    img.setColor(26, qRgb(0, 0, 255));
    img.setColor(27, qRgb(0, 0, 0));
    img.setColor(28, qRgb(255, 0, 0));
    img.setColor(29, qRgb(0, 255, 0));
    img.setColor(30, qRgb(0, 0, 255));
    img.setColor(31, qRgb(0, 0, 0));
    uchar *bits = *imgBits = img.bits();
    memset(bits, 0, width * height);
}

MainWindow::MainWindow(QWidget * parent)
:  
QMainWindow(parent), ui(new Ui::MainWindow), port("/dev/arduino", 115200),
moveNo(0), cmdQueue(), milling(false), movesCount(0), curZ(0)
{
    ui->setupUi(this);
    imgFile = QString::null;
    if (!port.open(QFile::ReadWrite)) {
        ui->tbSerial->setText(port.errorString());
    }
    MkPrnImg(prn, PRN_WIDTH, PRN_HEIGHT, &prnBits);
    readSerial();
}

MainWindow::~MainWindow()
{
    delete ui;
}

uchar getSetPixel(uchar * bits, int x, int y, bool get, uchar val)
{
    int index = y * (PRN_WIDTH + 1) + x;
    if (get) {
        return bits[index];
    }
    bits[index] = val;
    return 0;
}

uchar getPixel(uchar * bits, int x, int y)
{
    return getSetPixel(bits, x, y, true, 0);
}

void setPixel(uchar * bits, int x, int y, uchar val)
{
    getSetPixel(bits, x, y, false, val);
}


//static void drawLine(uchar * bits, int x0, int y0, int x1, int y1, int color)
//{
//    int dx = abs(x1 - x0);
//    int dy = abs(y1 - y0);
//    int sx, sy;
//    if (x0 < x1) {
//        sx = 1;
//    } else {
//        sx = -1;
//    }
//    if (y0 < y1) {
//        sy = 1;
//    } else {
//        sy = -1;
//    }
//    int err = dx - dy;
//    int e2;

//    for (;;) {
//        setPixel(bits, x0, y0, color);
//        if (x0 == x1 && y0 == y1) {
//            break;
//        }
//        e2 = 2 * err;
//        if (e2 > -dy) {
//            err = err - dy;
//            x0 = x0 + sx;
//        }
//        if (e2 < dx) {
//            err = err + dx;
//            y0 = y0 + sy;
//        }
//    }
//}

//
// Milling machine simulator
//

#define LOW 0
#define HIGH 1
#define A0 0
#define A1 1
#define A2 2
#define OUTPUT 0

class ArduinoSimSerial
{
public:
    QString cmd;
    int pos;

    ArduinoSimSerial()
    {
    }
    ~ArduinoSimSerial()
    {
    }
    void load(QString cmd)
    {
        this->cmd = cmd;
        pos = 0;
    }
    void begin(int)
    {
    };
    void print(const char *)
    {
    };
    void print(int)
    {
    };
    void println(int)
    {
    };
    void println(const char *)
    {
    };
    int available()
    {
        return pos < cmd.length();
    };
    char read()
    {
        return cmd.at(pos++).toAscii();
    };
    void write(char)
    {
    };
};
ArduinoSimSerial Serial;

int gpioVal;
int machineX = 0;
int machineY = 0;
int machineZ = 0;

int newMachineX = 0;
int newMachineY = 0;
int newMachineZ = 0;


void delayMicroseconds(int)
{
    machineX = newMachineX;
    machineY = newMachineY;
    machineZ = newMachineZ;

    //qDebug() << " move " << machineX << "," << machineY << "," << machineZ;
}

int analogRead(int)
{
    return 0;
}

//     A
//     + B
//
//
//         7 0 1
// dirs:   6   2
//         5 4 3
//
int machineMove(int coord, int newGpio, int gpio0, int gpio2, int gpio4, int gpio6)
{
    int oldDir = coord % 8;

    int is0 = newGpio & (1 << gpio0);
    int is2 = newGpio & (1 << gpio2);
    int is4 = newGpio & (1 << gpio4);
    int is6 = newGpio & (1 << gpio6);

    int newDir;
    if(is0 && is2)       newDir = 1;
    else if(is2 && is4)  newDir = 3;
    else if(is4 && is6)  newDir = 5;
    else if(is6 && is0)  newDir = 7;
    else if(is0)         newDir = 0;
    else if(is2)         newDir = 2;
    else if(is4)         newDir = 4;
    else if(is6)         newDir = 6;
    else return coord;              // all gpio powered off

    if(newDir == oldDir)
        return coord;

    int delta = newDir - oldDir;
    if(delta == 7)
        delta = -1;
    else if(delta == -7)
        delta = 1;
    else if(delta == 6)
        delta = -2;
    else if(delta == -6)
        delta = 2;

    return coord + delta;
}

void digitalWrite(int gpio, int value)
{
    int oldVal = gpioVal;

    if(value)
        gpioVal |= (1 << gpio);
    else
        gpioVal &= ~(1 << gpio);

    if(oldVal == gpioVal)
        return;

    QString str;
    for(int i = 13; i >= 0; i--) {
        if(gpioVal & (1 << i))
            str+="1";
        else
            str+="0";
    }
    qDebug() << "gpioVal " << str;

    if(gpio >= 2 && gpio <= 5)      // x axis, gpios 3 2 4 5
    {
        newMachineX = machineMove(machineX, gpioVal, 3, 2, 4, 5);
    }
}

void pinMode(int, int)
{
}

#include "../alfi_arduino/alfi_arduino.ino"

void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.drawImage(0, 0, prn);
    return;

    if (img.isNull()) {
        return;
    }
    p.drawImage(0, 50, img);
    p.drawImage(0, 100, prn);
}

// Send cmd queue to arduino. Returns after arduino received it
void MainWindow::writeCmdQueue()
{
    QString cmd = "q";
    for (int i = 0; i < cmdQueue.count(); i++) {
        cmd += " ";
        cmd += cmdQueue.at(i);
    }
    cmd += " e" + QString::number(++moveNo) + " ";
    cmdQueue.clear();

    // Execute on milling machine simulator
    Serial.load(cmd);
    int extraLoops = 100;
    while(Serial.available() || --extraLoops > 0)
    {
        loop();
        update();
        QApplication::processEvents();
    }
    return;

    qDebug() << "cmd=" << cmd;
    QByteArray cmdBytes = cmd.toAscii();
    int remains = cmdBytes.length();
    for (int i = 0; remains > 0; i += 64) {
        int count = (remains >= 64 ? 64 : remains);
        port.write(cmdBytes.constData() + i, count);

        for (;;) {
            int avail = port.bytesAvailable();
            if (avail < count) {
                port.waitForReadyRead(5);
                continue;
            }
            QByteArray echoBytes = port.read(count);
            qDebug() << "echo=" << echoBytes;
            for (int j = 0; j < count; j++) {
                if (echoBytes.at(j) != cmdBytes.at(i + j)) {
                    qDebug() << "send data failed!!!";
                    exit(1);
                }
            }
            break;
        }
        remains -= count;
    }
    //    port.write(cmd.toAscii());
}

// Wait until arduino finishes all command sent
void MainWindow::waitCmdDone()
{
    return;

    QString expect = "qdone" + QString::number(moveNo);
    qDebug() << "expect=" << expect;
    for (;;) {
        port.waitForReadyRead(10);
        QByteArray str = port.read(1024);
        if (str.length() == 0) {
            continue;
        }
        qDebug() << "serial in=" << str;
        for (int i = 0; i < str.count(); i++) {
            char ch = str.at(i);
            if ((ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') || ch == ' ') {
                serialLog.append(ch);
            }
        }
        qDebug() << "serialLog=" << serialLog;
        ui->tbSerial->append(str);
        ui->tbSerial->update();

        int index = serialLog.lastIndexOf(expect);
        if (index >= 0) {
            return;
        }
        if (serialLog.lastIndexOf("limit") >= 0) {      // limit switch
            QString tail =
                serialLog.count() < 1024 ? serialLog : serialLog.right(1024);
            QMessageBox::information(this, "Limit reached", tail);
            return;
        }
    }
}

void MainWindow::sendCmd(QString cmd, bool flush)
{
    cmdQueue.append(cmd);
    if (flush) {
        writeCmdQueue();
        waitCmdDone();
    }
}

void MainWindow::move(int axis, int pos, int target, bool justSetPos,
                      bool flush)
{
    moves[movesCount++] = axis;
    moves[movesCount++] = pos;
    moves[movesCount++] = target;
    if (movesCount >= MILL_LOG_LEN) {
        movesCount = 0;
    }

    QString cmd = "a" + QString::number(axis) +
        " p" + QString::number(pos) + " t" + QString::number(target);

    if (!justSetPos) {
        cmd += " m" + QString::number(++moveNo);
        if (cmdQueue.count() > QUEUE_LEN) {
            flush = true;
        }
    }
    sendCmd(cmd, flush);
}

void MainWindow::readSerial()
{
    if (milling) {
        return;
    }
    int avail = port.bytesAvailable();
    if (avail <= 0) {
        QTimer::singleShot(100, this, SLOT(readSerial()));
        return;
    }
    QByteArray data = port.readAll();
    qDebug() << data;
    ui->tbSerial->append(data);
    QTimer::singleShot(100, this, SLOT(readSerial()));
}

void MainWindow::on_bSendSerial_clicked()
{
    //port.write(ui->tbSendSerial->text().toAscii());
    sendCmd(ui->tbSendSerial->text());
}

void MainWindow::on_bXMinus_clicked()
{
    move(0, 0, ui->spinBox->value());
}

void MainWindow::on_bXPlus_clicked()
{
    move(0, ui->spinBox->value(), 0);
}

void MainWindow::on_bYMinus_clicked()
{
    move(1, 0, ui->spinBox->value());
}

void MainWindow::on_bYPlus_clicked()
{
    move(1, ui->spinBox->value(), 0);
}

void MainWindow::on_bZMinus_clicked()
{
    move(0, 24, 0);
    move(2, 437, 0);
}

void MainWindow::on_bZPlus_clicked()
{
    move(2, 0, 437);

    move(2, 437, 437 - 128);    // move up & down so that the gear does not slip ;-)
    move(2, 437 - 128, 437);

    move(0, 0, 24);
}

void MainWindow::on_bMill_clicked()
{
    milling = true;

    if(!QFile::exists("remaining.txt")) {
        if(!QFile::copy("shape.txt", "remaining.txt")) {
            QMessageBox::critical(this, "Error", "Failed to copy shape.txt -> remaining.txt");
            return;
        }
    }

    QFile f("remaining.txt");
    if (!f.open(QFile::ReadWrite)) {
        QMessageBox::critical(this, "Error", "failed to load " + f.fileName() + ": " + f.errorString());
        return;
    }

    for(;;)
    {
        f.seek(0);
        QByteArray line = f.readLine().trimmed();
        QByteArray rest = f.readAll();

        if(line.length() == 0)
        {
            if(rest.length() == 0)
            {
                break;
            }
            continue;
        }

        //qDebug() << line;
        cmdQueue.append(line);
        writeCmdQueue();

        f.resize(rest.length());
        f.seek(0);
        if(f.write(rest) != rest.length())
        {
            QMessageBox::critical(this, "Error", "Failed write to remaining.txt");
        }
        waitCmdDone();
     }
    f.close();
    QFile::remove("remaining.txt");
    QMessageBox::information(this, "milling", "done!");
}
