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
QMainWindow(parent), ui(new Ui::MainWindow), port("/dev/ttyACM0", 115200),
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

static int getCoord(QString str)
{
    QStringList list = str.split('.');
    int n = list.at(0).toInt() * 1000;
    if (list.count() == 1) {
        return n;
    }
    QString mStr = list.at(1);
    int m = mStr.toInt();
    for (int i = mStr.length();;) {
        if (i == 3) {
            if (str.indexOf('-') >= 0) {
                return n - m;
            }
            return n + m;
        }
        if (i > 3) {
            m /= 10;
            i--;
        }
        if (i < 3) {
            m *= 10;
            i++;
        }
    }
}

static void drawLine(uchar * bits, int x0, int y0, int x1, int y1, int color)
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
        setPixel(bits, x0, y0, color);
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

// Compute driller path A->B->C->D taking into account driller width.
//
//         | D
//         |
//  -------+ C
//  A      B
//
// Step 1: parallel line to AB shifted by width (same for AD)
//
//
//  E     F
//  -------        H |   | D
//                   |   |
//  -------        G |   | C
//  A     B
//
static void millingPath(qint64 ax, qint64 ay,
                        qint64 bx, qint64 by,
                        qint64 r,
                        qint64 & x0, qint64 & y0, qint64 & x1, qint64 & y1)
{
    // orthogonal line r pixels long
    qint64 w = by - ay;
    qint64 h = ax - bx;
    qint64 c = (qint64) sqrt((10000 * 10000 * r * r) / (w * w + h * h));
    w = (c * w) / 10000;
    h = (c * h) / 10000;
    x0 = ax + w;
    y0 = ay + h;
    x1 = bx + w;
    y1 = by + h;
}

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

static void drawMoves(uchar * bits, int *moves, int movesCount, int width,
                      int height, int color)
{
    memset(bits, 0, PRN_WIDTH * PRN_HEIGHT);

    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    int curX = 0;
    int curY = 0;

    for (int i = 0; i < movesCount; i += 3) {
        int axis = moves[i];
        int pos = moves[i + 1];
        int target = moves[i + 2];

        if (axis == 0) {
            curX += target - pos;
            minX = (curX < minX ? curX : minX);
            maxX = (curX > maxX ? curX : maxX);
        }
        if (axis == 1) {
            curY += target - pos;
            minY = (curY < minY ? curY : minY);
            maxY = (curY > maxY ? curY : maxY);
        }
    }

    int cx = (1000 * (width - 100)) / (maxX - minX + 1);
    int cy = (1000 * (height - 150)) / (maxY - minY + 1);

    cx = cy = (cx < cy ? cx : cy);

    curX = curY = 0;
    int prevX = 0;
    int prevY = 0;
    for (int i = 0; i < movesCount; i += 3) {
        int axis = moves[i];
        int pos = moves[i + 1];
        int target = moves[i + 2];

        prevX = curX;
        prevY = curY;
        if (axis == 0) {
            curX += target - pos;
        }
        if (axis == 1) {
            curY += target - pos;
        }

        int x1 = 50 + (cx * (prevX - minX)) / 1000;
        int y1 = 100 + (cy * (prevY - minY)) / 1000;
        int x2 = 50 + (cx * (curX - minX)) / 1000;
        int y2 = 100 + (cy * (curY - minY)) / 1000;
        drawLine(bits, x1, y1, x2, y2, color % 31);
    }
}

void MainWindow::flushQueue()
{
    update();
    QApplication::processEvents();

    drawMoves(prnBits, moves, movesCount, width(), height(), curZ);

    //cmdQueue.clear();
    //return;

    QString cmd = "q";
    for (int i = 0; i < cmdQueue.count(); i++) {
        cmd += " ";
        cmd += cmdQueue.at(i);
    }
    cmd += " e" + QString::number(++moveNo) + " ";
    cmdQueue.clear();

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
        if (serialLog.lastIndexOf("limit") >= 0) {
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
        flushQueue();
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
    port.write(ui->tbSendSerial->text().toAscii());
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

// Move plotter using svg coordinates
//
// pcbWidth = 331827 svg pixes = 94.285mm
// 5000 steps = 43.6 mm
// 10812.5 steps = 94.285mm
//
// 331827 svg pixels = 10812.5 steps
// 1step = 30.6892023121
void MainWindow::moveBySvgCoord(int axis, qint64 pos, qint64 target, int driftX,
                                bool justSetpos)
{
    int stepsPos = (pos * 1000) / 30897;
    int stepsTarget = (target * 1000) / 30897;

    if (axis == 0) {
        stepsPos += driftX;
        stepsTarget += driftX;
    }

    move(axis, stepsPos, stepsTarget, justSetpos, false);
}

static QString num2svg(qint64 num)
{
    int th = abs(num) / 1000;
    int rest = abs(num) % 1000;
    QString res = (num >= 0 ? "" : "-");
    res += QString::number(th);
    res += '.';
    if (rest <= 9) {
        res += "00";
    } else if (rest <= 99) {
        res += "0";
    }
    res += QString::number(rest);
    return res;
}

// Load svg lines to x1,y1,x2,y2 arrays and return count
static int loadSvg(QString path, qint64 * x1, qint64 * y1, qint64 * x2,
                   qint64 * y2, QStringList & lines, int maxCount,
                   qint64 & minX, qint64 & maxX, qint64 & minY, qint64 & maxY)
{
    qDebug() << "loading " << path;

    QFile f(path);
    if (!f.open(QFile::ReadOnly)) {
        qCritical() << "failed to load " << path << ": " << f.errorString();
        return 0;
    }

    int count = 0;

    QRegExp rxy("d=\"([mM]) (-?\\d+\\.?\\d*),(-?\\d+\\.?\\d*)");
    QRegExp rwh("(-?\\d+\\.?\\d*),(-?\\d+\\.?\\d*)");
    QRegExp rsci("[0123456789\\.-]+e[0123456789\\.-]+");    // scientific format, e.g. -8e-4

    // Read and parse svg file, results are in x1,x2,y1,y2 arrays
    for (;;) {
        QByteArray line = f.readLine();
        if (line.isEmpty()) {
            break;
        }
        line = line.trimmed();

        // Convert scientific small numbers to 0
        int pos = rsci.indexIn(line);
        if (pos >= 0) {
            line.replace(rsci.cap(0), "0");
            qDebug() << "removed scientific format, new line=" << line;
        }

        pos = rxy.indexIn(line);
        if (pos < 0) {
            continue;
        }
        bool abs = rxy.cap(1) == "M";
        qDebug() << "line[" << count << "]=" << line;

        QString capxy = rxy.cap(0);
        qint64 x = getCoord(rxy.cap(2));
        qint64 y = getCoord(rxy.cap(3));

        line.remove(0, pos + capxy.length());

        for (int i = 0;; i++) {
            pos = rwh.indexIn(line);
            if (pos < 0) {
                break;
            }

            QString capwh = rwh.cap(0);
            qint64 w = getCoord(rwh.cap(1));
            qint64 h = getCoord(rwh.cap(2));

            if (abs) {
                w -= x;
                h -= y;
            }

            qDebug() << "x=" << x << " y=" << y << " w=" << w << " h=" << h;

            x1[count] = x;
            y1[count] = y;
            x2[count] = x + w;
            y2[count] = y + h;

            minX = (x1[count] < minX ? x1[count] : minX);
            maxX = (x1[count] > maxX ? x1[count] : maxX);
            minX = (x2[count] < minX ? x2[count] : minX);
            maxX = (x2[count] > maxX ? x2[count] : maxX);

            minY = (y1[count] < minY ? y1[count] : minY);
            maxY = (y1[count] > maxY ? y1[count] : maxY);
            minY = (y2[count] < minY ? y2[count] : minY);
            maxY = (y2[count] > maxY ? y2[count] : maxY);

            count++;
            lines.append(line);

//            drawLine(prnBits,
//                     x / 1000,
//                     y / 1000 - 500,
//                     (x + w) / 1000,
//                     (y + h) / 1000 - 500,
//                     1);

            if (count > maxCount) {
                qWarning() << "svg file longer then maxCount=" << maxCount;
                break;
            }

            x += w;
            y += h;

            line.remove(0, pos + capwh.length());
            qDebug() << "rem=" << line;
        }
    }

    f.close();
    qDebug() << "MIN X=" << minX << " MAX X=" << maxX << " MIN Y=" << minY <<
        " MAX Y=" << maxY;

    return count;
}

static void mirror(qint64 * x1, qint64 * y1, qint64 * x2, qint64 * y2,
                   int count, qint64 minX, qint64 maxX, qint64 minY,
                   qint64 /*maxY */ )
{
    qint64 midX = (minX + maxX) / 2 - minX;
    for (int i = 0; i < count; i++) {
        x1[i] = midX - x1[i] + midX;
        x2[i] = midX - x2[i] + midX;

        // Make sure we have only positive coordinates
        x1[i] -= minX;
        x2[i] -= minX;
        y1[i] -= minY;
        y2[i] -= minY;

        if (x1[i] < 0 || x2[i] < 0 || y1[i] < 0 || y2[i] < 0) {
            qDebug() << "negative coordinate";
            exit(123);
        }
    }
}

void MainWindow::millShape(qint64 * x1, qint64 * y1, qint64 * x2, qint64 * y2,
                           int *colors, int count, int color, int driftX,
                           QStringList & /*lines */ ,
                           qint64 & lastX, qint64 & lastY, bool firstPoint)
{
    // Current and target positions on svg
    qint64 cx;
    qint64 cy;
    qint64 tx = lastX;
    qint64 ty = lastY;

    memset(colors, color ? 0 : 1, 65535);

    for (;;) {
        // Find nearest line
        qint64 ndist = 0x7fffffffffffffff;
        int nindex = -1;
        bool swap = false;
        for (int i = 0; i < count; i++) {
            qint64 w = x1[i] - tx;
            qint64 h = y1[i] - ty;
            qint64 dist1 = w * w + h * h;
            w = x2[i] - tx;
            h = y2[i] - ty;
            qint64 dist2 = w * w + h * h;
            qint64 dist = (dist1 < dist2 ? dist1 : dist2);
            //qDebug() << "i=" << i << ", dist=" << dist << "line=" << lines.at(i);
            if (dist > ndist) {
                continue;
            }
            if (colors[i] == color) {
                continue;       // already drawn
            }
            ndist = dist;
            nindex = i;
            swap = (dist2 < dist1);
            if (firstPoint)     // make sure we start from index=0 if requested
            {
                firstPoint = false;
                ndist = -1;
            }
        }

        if (nindex >= 0) {
            cx = swap ? x2[nindex] : x1[nindex];
            cy = swap ? y2[nindex] : y1[nindex];
            tx = swap ? x1[nindex] : x2[nindex];
            ty = swap ? y1[nindex] : y2[nindex];
            colors[nindex] = color;

            //qDebug() << "next line is " << lines.at(nindex);
        } else {
//            update();
//            QApplication::processEvents();
//            Sleeper::msleep(5000);

            // all done
            flushQueue();
            return;
        }

        if (cx != lastX || cy != lastY) // if lines on are not continuous
        {
//            drawLine(prnBits,
//                     lastX / 1000 + 200,
//                     lastY / 1000 - 700,
//                     cx / 1000 + 200,
//                     cy / 1000 - 700,
//                     color);

            moveBySvgCoord(0, lastX, cx, driftX, true);
            moveBySvgCoord(1, lastY, cy, driftX, false);
        }
//        drawLine(prnBits,
//                 cx / 1000 + 200,
//                 cy / 1000 - 700,
//                 tx / 1000 + 200,
//                 ty / 1000 - 700,
//                 color);

        moveBySvgCoord(0, cx, tx, driftX, true);
        moveBySvgCoord(1, cy, ty, driftX, false);

        lastX = tx;
        lastY = ty;
    }
}

// Move z axis in 0.5 mm steps
void MainWindow::moveZ(int z, int &driftX)
{
    curZ += z;
    qDebug() << "======================= Z=" << curZ;

    sendCmd("s8000 d4000");
    while (z > 0) {
        move(2, 0, 437, false, true);   // drill the shape shifted 0.5mm down
        move(0, 0, 24, false, true);    // compensate x drift

        move(2, 437, 437 - 128);    // move up & down so that the gear does not slip ;-)
        move(2, 437 - 128, 437);

        driftX += 24;
        z--;
    }
    while (z < 0) {
        move(0, 24, 0, false, true);    // compensate x drift
        move(2, 437, 0, false, true);   // drill the shape shifted 0.5mm down
        driftX -= 24;
        z++;
    }
    sendCmd("s3600 d2400");
}

void MainWindow::on_bMill_clicked()
{
    milling = true;

    qint64 minX = 0x7fffffffffffffff;
    qint64 maxX = 0;
    qint64 minY = 0x7fffffffffffffff;
    qint64 maxY = 0;

    // PCB
    QStringList pcbLines;
    static qint64 pcbX1[65535];
    static qint64 pcbY1[65535];
    static qint64 pcbX2[65535];
    static qint64 pcbY2[65535];
    static int pcbColors[65535];

    int pcbCount =
        loadSvg("/home/radek/alfi/gui/pcb_milling.svg", pcbX1, pcbY1, pcbX2,
                pcbY2, pcbLines, 65535, minX, maxX, minY, maxY);

    // Outer shape
    QStringList shapeLines;
    static qint64 shapeX1[65535];
    static qint64 shapeY1[65535];
    static qint64 shapeX2[65535];
    static qint64 shapeY2[65535];
    static int shapeColors[65535];

    int shapeCount =
        loadSvg("/home/radek/alfi/gui/shape_milling.svg", shapeX1, shapeY1,
                shapeX2, shapeY2, shapeLines, 65535, minX, maxX, minY, maxY);

    // LCD module hole
    QStringList lcmLines;
    static qint64 lcmX1[65535];
    static qint64 lcmY1[65535];
    static qint64 lcmX2[65535];
    static qint64 lcmY2[65535];
    static int lcmColors[65535];

    int lcmCount =
        loadSvg("/home/radek/alfi/gui/lcm_milling.svg", lcmX1, lcmY1, lcmX2,
                lcmY2, lcmLines, 65535, minX, maxX, minY, maxY);

    // Front display hole (a bit smaller then LCM)
    QStringList lcdLines;
    static qint64 lcdX1[65535];
    static qint64 lcdY1[65535];
    static qint64 lcdX2[65535];
    static qint64 lcdY2[65535];
    static int lcdColors[65535];

    int lcdCount =
        loadSvg("/home/radek/alfi/gui/lcd_milling.svg", lcdX1, lcdY1, lcdX2,
                lcdY2, lcdLines, 65535, minX, maxX, minY, maxY);

    mirror(pcbX1, pcbY1, pcbX2, pcbY2, pcbCount, minX, maxX, minY, maxY);
    mirror(shapeX1, shapeY1, shapeX2, shapeY2, shapeCount, minX, maxX, minY,
           maxY);
    mirror(lcmX1, lcmY1, lcmX2, lcmY2, lcmCount, minX, maxX, minY, maxY);
    mirror(lcdX1, lcdY1, lcdX2, lcdY2, lcdCount, minX, maxX, minY, maxY);

    int driftX = 0;
    qint64 lastX = shapeX1[0];
    qint64 lastY = shapeY1[0];

    // Start with outer shape just 0.5mm down
    millShape(shapeX1, shapeY1, shapeX2, shapeY2, shapeColors, shapeCount, 1, driftX, shapeLines, lastX, lastY);    // 0mm
    moveZ(1, driftX);
    millShape(shapeX1, shapeY1, shapeX2, shapeY2, shapeColors, shapeCount, 2, driftX, shapeLines, lastX, lastY);    // 0.5mm
    moveZ(-2, driftX);

    // Move to pcb -0.5 above and mill it 5mm down
    for (int i = 1; i <= 11; i++) {
        millShape(pcbX1, pcbY1, pcbX2, pcbY2, pcbColors, pcbCount, i, driftX, pcbLines, lastX, lastY);  // -0.5..5mm
        moveZ(1, driftX);
    }

    // Move to LCM -0.5 above
    moveZ(-11, driftX);

    // LCM module 5mm + 4mm down
    for (int i = 1; i <= 19; i++) {
        millShape(lcmX1, lcmY2, lcmX2, lcmY2, lcmColors, lcmCount, i, driftX, lcmLines, lastX, lastY);  // -0.5..9mm
        moveZ(1, driftX);
    }

    // Move to LCD -0.5 above
    moveZ(-19, driftX);

    // LCD display 5+4+3mm down
    for (int i = 1; i <= 25; i++) {
        millShape(lcdX1, lcdY2, lcdX2, lcdY2, lcdColors, lcdCount, i, driftX, lcdLines, lastX, lastY);  // -0.5..12mm
        moveZ(1, driftX);
    }

    // Move to outer shape -1.5 above
    moveZ(-27, driftX);

    // Outer shape 5+4+3+3mm down
    for (int i = 1; i <= 33; i++) {
        millShape(shapeX1, shapeY1, shapeX2, shapeY2, shapeColors, shapeCount, 1, driftX, shapeLines, lastX, lastY);    // -0.5..15mm
        moveZ(1, driftX);
    }

    moveZ(-33, driftX);
}

// Compute milling path taking into account driller radius
void MainWindow::on_bMillPath_clicked()
{
    QStringList lines;
    static qint64 x1[65535];
    static qint64 y1[65535];
    static qint64 x2[65535];
    static qint64 y2[65535];

    qint64 minX = 0x7fffffffffffffff;
    qint64 maxX = 0;
    qint64 minY = 0x7fffffffffffffff;
    qint64 maxY = 0;

    int count =
        loadSvg("/home/radek/alfi/gui/lcm.svg", x1, y1, x2, y2, lines, 65535,
                minX, maxX, minY, maxY);

    openOutFile("/home/radek/alfi/gui/lcm_milling.svg");

    QString millStr;
    for (int i = 0; i < count; i++) {
        qint64 cx, cy, tx, ty;
        millingPath(x1[i], y1[i], x2[i], y2[i], 9525 - 4762,    // driller radius (1.6) - some space so that pcb fits in (0.8)
                    cx, cy, tx, ty);

        millStr.append("<path d=\"m ");
        millStr.append(num2svg(cx) + "," + num2svg(cy) + " " +
                       num2svg(tx - cx) + "," + num2svg(ty - cy));
        millStr.
            append
            ("\"\nstyle=\"fill:#000000;fill-opacity:1;fill-rule:evenodd;stroke:#000000;stroke-width:0.76908362;stroke-linecap:round;stroke-linejoin:round;stroke-miterlimit:10;stroke-opacity:1;stroke-dasharray:none\"\n");
        millStr.append("id=\"path" + QString::number(x1[i] + y1[i]) + "\"\n");
        millStr.append("/>\n\n");
    }

    outFile->write(millStr.toLatin1());
    closeOutFile();
}

void MainWindow::on_bMillCover_clicked()
{
    milling = true;

    qint64 minX = 0x7fffffffffffffff;
    qint64 maxX = 0;
    qint64 minY = 0x7fffffffffffffff;
    qint64 maxY = 0;

    // PCB
    QStringList pcbLines;
    static qint64 pcbX1[65535];
    static qint64 pcbY1[65535];
    static qint64 pcbX2[65535];
    static qint64 pcbY2[65535];
    static int pcbColors[65535];

    int pcbCount =
        loadSvg("/home/radek/alfi/gui/pcb_milling.svg", pcbX1, pcbY1, pcbX2,
                pcbY2, pcbLines, 65535, minX, maxX, minY, maxY);

    // Outer shape
    QStringList shapeLines;
    static qint64 shapeX1[65535];
    static qint64 shapeY1[65535];
    static qint64 shapeX2[65535];
    static qint64 shapeY2[65535];
    static int shapeColors[65535];

    int shapeCount =
        loadSvg("/home/radek/alfi/gui/shape_milling.svg", shapeX1, shapeY1,
                shapeX2, shapeY2, shapeLines, 65535, minX, maxX, minY, maxY);

    // Battery hole
    QStringList batteryLines;
    static qint64 batteryX1[65535];
    static qint64 batteryY1[65535];
    static qint64 batteryX2[65535];
    static qint64 batteryY2[65535];
    static int batteryColors[65535];

    int batteryCount =
        loadSvg("/home/radek/alfi/gui/battery_hole_milling.svg", batteryX1, batteryY1, batteryX2,
                batteryY2, batteryLines, 65535, minX, maxX, minY, maxY);

    mirror(pcbX1, pcbY1, pcbX2, pcbY2, pcbCount, minX, maxX, minY, maxY);
    mirror(shapeX1, shapeY1, shapeX2, shapeY2, shapeCount, minX, maxX, minY,
           maxY);
    mirror(batteryX1, batteryY1, batteryX2, batteryY2, batteryCount, minX, maxX, minY, maxY);

    int driftX = 0;
    qint64 lastX = shapeX1[0];
    qint64 lastY = shapeY1[0];

    // Start with outer shape
    millShape(shapeX1, shapeY1, shapeX2, shapeY2, shapeColors, shapeCount, 1, driftX, shapeLines, lastX, lastY);    // 0mm
    moveZ(-1, driftX);

    // Battery hole 6mm down
    for (int i = 1; i <= 13; i++) {
        millShape(batteryX1, batteryY2, batteryX2, batteryY2, batteryColors, batteryCount, i, driftX, batteryLines, lastX, lastY);  // -0.5..6mm
        moveZ(1, driftX);
    }

    // Move to outer shape -0.5 above
    moveZ(-14, driftX);

    // Outer shape 10mm down
    for (int i = 1; i <= 21; i++) {
        millShape(shapeX1, shapeY1, shapeX2, shapeY2, shapeColors, shapeCount, 1, driftX, shapeLines, lastX, lastY);    // -0.5..15mm
        moveZ(1, driftX);
    }
}
