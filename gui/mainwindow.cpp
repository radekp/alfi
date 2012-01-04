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

#define BASEPORT 0x378 /* lp1 */

#define PRN_WIDTH 2047
#define PRN_HEIGHT 2047

QFile *outFile = NULL;

void openOutFile(QString name)
{
    if(outFile) {
        outFile->close();
        delete outFile;
    }
    outFile = new QFile(name + ".txt");
    outFile->open(QFile::WriteOnly | QFile::Truncate);
}

void closeOutFile()
{
    outFile->write("\n");
    outFile->close();
    outFile = NULL;
}

static void MkPrnImg(QImage &img, int width, int height, uchar **imgBits)
{
    img = QImage(width, height, QImage::Format_Indexed8);

    img.setNumColors(2);
    img.setColor(0, qRgb(255, 255, 255));
    img.setColor(1, qRgb(0, 0, 0));

    uchar *bits = *imgBits = img.bits();
    memset(bits, 0, width * height);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), port("/dev/ttyACM0", 9600), moveNo(0)
{
    ui->setupUi(this);
    imgFile = QString::null;
    nextImg();
    if(!port.open(QFile::ReadWrite))
    {
        ui->tbSerial->setText(port.errorString());
    }
    MkPrnImg(prn, PRN_WIDTH, PRN_HEIGHT, &prnBits);
    readSerial();
}

MainWindow::~MainWindow()
{
    delete ui;
}

uchar getSetPixel(uchar *bits, int x, int y, bool get, uchar val)
{
    int index = y * (PRN_WIDTH + 1) + x;
    if(get)
    {
        return bits[index];
    }
    bits[index] = val;
    return 0;
}

uchar getPixel(uchar *bits, int x, int y)
{
    return getSetPixel(bits, x, y, true, 0);
}

void setPixel(uchar *bits, int x, int y, uchar val)
{
    getSetPixel(bits, x, y, false, val);
}


static int getCoord(QString str)
{
    QStringList list = str.split('.');
    int n = list.at(0).toInt() * 1000;
    if(list.count() == 1)
    {
        return n;
    }
    QString mStr = list.at(1);
    int m = mStr.toInt();
    for(int i = mStr.length();;)
    {
        if(i == 3)
        {
            if(str.indexOf('-') >= 0)
            {
                return n - m;
            }
            return n + m;
        }
        if(i > 3)
        {
            m /= 10;
            i--;
        }
        if(i < 3)
        {
            m *= 10;
            i++;
        }
    }
}

static void drawLine(uchar *bits, int x0, int y0, int x1, int y1, int color)
{
    int dx = abs(x1-x0);
    int dy = abs(y1-y0);
    int sx, sy;
    if(x0 < x1)
    {
        sx = 1;
    }
    else
    {
        sx = -1;
    }
    if(y0 < y1)
    {
        sy = 1;
    }
    else
    {
        sy = -1;
    }
    int err = dx-dy;
    int e2;

    for(;;)
    {
        setPixel(bits, x0, y0, color);
        if(x0 == x1 && y0 == y1)
        {
            break;
        }
        e2 = 2*err;
        if(e2 > -dy)
        {
            err = err - dy;
            x0 = x0 + sx;
        }
        if(e2 <  dx)
        {
            err = err + dx;
            y0 = y0 + sy;
        }
    }
}

// Compute drilling path A->B->C->D taking into account driller width.
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


static void drillingPath(qint64 ax, qint64 ay,
                         qint64 bx, qint64 by,
                         qint64 cx, qint64 cy,
                         qint64 dx, qint64 dy,
                         qint64 r,
                         qint64 & x0, qint64 & y0,
                         qint64 & x1, qint64 & y1,
                         qint64 & x2, qint64 & y2,
                         qint64 & x3, qint64 & y3
                         )
{
    // orthogonal line r pixels long
    qint64 w = by - ay;
    qint64 h = ax - bx;
    qint64 c = (qint64) sqrt((1000000 * r * r) / (w * w + h * h));
    w = (c * w) / 1000;
    h = (c * h) / 1000;
    x0 = ax + w;
    y0 = ay + h;
    x1 = bx + w;
    y1 = by + h;
}

void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    p.drawImage(0, 100, prn);
    return;

    if(img.isNull())
    {
        return;
    }
    p.drawImage(0, 50, img);
    p.drawImage(0, 100, prn);
}

void redraw(MainWindow *win)
{
    win->update();
    QApplication::processEvents();
}

void MainWindow::oneUp()
{
    outFile->write("1");

    setPixel(prnBits, penX, penY, 1);
    penY--;
    setPixel(prnBits, penX, penY, 1);
}

void MainWindow::oneDown()
{
    outFile->write("5");

    setPixel(prnBits, penX, penY, 1);
    penY++;
    setPixel(prnBits, penX, penY, 1);
}

void MainWindow::oneLeft()
{
    outFile->write("7");

    setPixel(prnBits, penX, penY, 1);
    penX--;
    setPixel(prnBits, penX, penY, 1);
}

void MainWindow::oneRight()
{
    outFile->write("3");

    setPixel(prnBits, penX, penY, 1);
    penX++;
    setPixel(prnBits, penX, penY, 1);
}

void MainWindow::up(int times)
{
    for(int i = 0; i < times; i++)
    {
        oneUp();
    }
}

void MainWindow::down(int times)
{
    for(int i = 0; i < times; i++)
    {
        oneDown();
    }
}

void MainWindow::left(int times)
{
    for(int i = 0; i < times; i++)
    {
        oneLeft();
    }
}

void MainWindow::right(int times)
{
    for(int i = 0; i < times; i++)
    {
        oneRight();
    }
}

int populateRegion(uchar *bits, int x, int y, QList<QPoint> & region)
{
    if(x < 0 || x >= PRN_WIDTH || y < 0 || y >= PRN_HEIGHT || getPixel(bits, x, y) == 0)
    {
        return 0;
    }
    setPixel(bits, x, y, 0);

    int around =
            populateRegion(bits, x + 1, y, region) +
            populateRegion(bits, x - 1, y, region) +
            populateRegion(bits, x, y - 1, region) +
            populateRegion(bits, x, y + 1, region);
//            populateRegion(bits, x - 1, y - 1, region) +
//            populateRegion(bits, x + 1, y - 1, region) +
//            populateRegion(bits, x + 1, y + 1, region) +
//            populateRegion(bits, x - 1, y + 1, region);

    QPoint p(x, y);
    region.append(p);
    return 1;
}

bool findRegion(uchar *bits, QList< QList<QPoint> > & regionList)
{
    QList<QPoint> region;

    // Find first black point
    int x = 0;
    int y = 0;
    for(;;)
    {
        if(getPixel(bits, x, y))
        {
            break;
        }
        x++;
        if(x < PRN_WIDTH)
        {
            continue;
        }
        x = 0;
        y++;
        if(y >= PRN_HEIGHT)
        {
            qDebug() << "all regions found";
            return false;   // no more black pixels - we are done
        }
    }

    qDebug() << "populating region at " << x << "x" << y;
    populateRegion(bits, x, y, region);
    regionList.append(region);

    return true;
}

void fillImage(QImage & src, uchar *bits)
{
    memset(bits, 0, PRN_WIDTH * PRN_HEIGHT);
    for(int x = 0; x < src.width(); x++)
    {
        for(int y = 0; y < src.height(); y++)
        {
            QRgb pix = src.pixel(x, y);
            int grey = qGray(pix);
            setPixel(bits, x, y, grey < 127);
        }
    }
}

int findNearest(QList<QPoint> & region, uchar *bits, int x, int y, QPoint & nearest)
{
    int min = 0x7fffffff;
    for(int i = 0; i < region.count(); i++)
    {
        QPoint p = region.at(i);
        if(getPixel(bits, p.x(), p.y()))
        {
            continue;
        }
        int dx = x - p.x();
        int dy = y - p.y();
        int dst = dx * dx + dy * dy;
        if(dst < min && dst > 0)
        {
            nearest = p;
            min = dst;
        }
    }
    return min;
}

void move(MainWindow * win, QPoint & p, QPoint & np)
{
    int dx = np.x() - p.x();
    int dy = np.y() - p.y();
    if(dx > 0)
    {
        win->right(dx);
    }
    else if(dx < 0)
    {
        win->left(-dx);
    }
    if(dy > 0)
    {
        win->down(dy);
    }
    else if(dy < 0)
    {
        win->up(-dy);
    }
}

void niceDraw(QImage & src, uchar *bits, uchar *prnBits, MainWindow * win)
{
    fillImage(src, bits);
    redraw(win);

    // Find continuous regions in image
    QList< QList<QPoint> > regionList;
    for(;;)
    {
        qDebug() << "findRegion";
        if(!findRegion(bits, regionList))
        {
            break;
        }
        redraw(win);
    }

    // Find start point (nearest point to 0,0)
    QPoint startPoint;
    QList<QPoint> startRegion;
    int min = 0x7fffffff;
    for(int i = 0; i < regionList.count(); i++)
    {
        QList<QPoint> region = regionList.at(i);
        QPoint point;
        int dst = findNearest(region, prnBits, 0, 0, point);
        if(dst < min)
        {
            startRegion = region;
            startPoint = point;
            min = dst;
        }
    }

    QPoint p(0, 0);
    move(win, p, startPoint);             // move to start point
    win->down(startPoint.y());
    redraw(win);

    QList<QPoint> region = startRegion;
    p = startPoint;
    for(;;)
    {
        QPoint np;
        while(findNearest(region, bits, p.x(), p.y(), np) == 1)     // draw all nearby points
        {
            setPixel(bits, p.x(), p.y(), 1);
            setPixel(bits, np.x(), np.y(), 1);
            move(win, p, np);
            p = np;
            redraw(win);
        }
        int i = 0;
        for(;;)
        {
            if(i >= region.count())
            {
                i = 0;
            }
            if(region.at(i) == p)
            {
                break;
            }
            i++;
        }
        break;
    }
}

bool findNearest(int x, int y, QImage & img, uchar *bits, int *nx, int *ny)
{
    int imax = img.width() + img.height();

    for(int i = 1; i < imax; i++)
    {
        for(int j = 0; j <= i; j++)
        {
            //   0
            // 6   4
            //   2
            for(int r = 0; r < 8; r++)
            {
                int px = x;
                int py = y;
                switch(r)
                {
                    case 0: py -= i; px += j; break;
                    case 1: py -= i; px -= j; break;
                    case 2: py += i; px += j; break;
                    case 3: py += i; px -= j; break;
                    case 4: px += i; py += j; break;
                    case 5: px += i; py -= j; break;
                    case 6: px -= i; py += j; break;
                    case 7: px -= i; py -= j; break;
                }

                if(px < 0 || px >= img.width() ||
                   py < 0 || py >= img.height())
                {
                    continue;
                }
                QRgb pix = img.pixel(px, py);
                int grey = qGray(pix);
                if(grey > 127)
                {
                    continue;
                }
                if(getPixel(bits, px, py))
                {
                    continue;
                }
                setPixel(bits, px, py, 1);
                *nx = px;
                *ny = py;
                return true;
            }
        }
    }
    return false;
}

bool isBlack(QImage & img, int x, int y)
{
    if(x < 0 || x >= img.width() || y < 0 || y >= img.height())
    {
        return false;
    }
    QRgb pix = img.pixel(x, y);
    int grey = qGray(pix);
    return grey <= 127;
}

bool isWhite(QImage & img, int x, int y)
{
    return !isBlack(img, x, y);
}

// Find first pixel from top, if more return the one most left
bool findTopLeft(QImage & img, uchar *bits, int *x, int *y)
{
    for(int i = 0; i < img.height(); i++)
    {
        for(int j = 0; j < img.width(); j++)
        {
            if(isWhite(img, j, i))
            {
                continue;
            }
            if(getPixel(bits, j, i))
            {
                continue;
            }
            *x = j;
            *y = i;
            return true;
        }
    }
    return false;
}

void MainWindow::move(int axis, int pos, int target, int scale, bool queue)
{
    QString cmd = "a" + QString::number(axis) +
                  " p" + QString::number(pos * scale) +
                  " t" + QString::number(target * scale);

    if(queue)
    {
        cmd += " ";
    }
    else
    {
        cmd += " m" + QString::number(moveNo) + " ";
    }

    qDebug() << cmd;
    port.write(cmd.toAscii());

    if(queue)
    {
        return;
    }

    QString expect = "done " + QString::number(moveNo);
    QString reply;
    for(;;)
    {
        port.waitForReadyRead(10);
        int avail = port.bytesAvailable();
        if(avail <= 0)
        {
            continue;
        }
        QByteArray str = port.read(avail);
        ui->tbSerial->append(str);
        ui->tbSerial->update();
        reply += str;
        if(reply.indexOf(expect) >= 0)
        {
            break;
        }
        if(reply.indexOf("limit") >= 0)
        {
            QMessageBox::information(this, "Limit reached", reply);
        }
    }
    moveNo++;
}

void MainWindow::loadImg()
{
    //QMessageBox::information(this, "info", "loading " + imgFile);

    qDebug() << imgFile;

    img = QImage(imgFile);

    qDebug() << "img size=" << img.width() << "x" << img.height();
    setWindowTitle(imgFile + " " + QString::number(img.width()) + "x" + QString::number(img.height()));

    penX = penY = 0;

    MkPrnImg(prn, PRN_WIDTH, PRN_HEIGHT, &prnBits);
    update();
}

void MainWindow::nextImg()
{
    QStringList list = QDir::current().entryList();
    bool found = false;
    for(int i = 0; i < list.count(); i++)
    {
        QString filename = list.at(i);
        if(!filename.endsWith(".png"))
        {
            continue;
        }
        if(imgFile == QString::null)
        {
            found = true;
        }
        else if(imgFile == filename)
        {
            found = true;
            continue;
        }
        if(!found)
        {
            continue;
        }
        imgFile = filename;
        loadImg();
        return;
    }
    imgFile = QString::null;
    if(found)
    {
        nextImg();
    }
}

void MainWindow::on_bImg_clicked()
{
    nextImg();
}

void MainWindow::on_bPrintDotted_clicked()
{
    int doth = 2;

    openOutFile("dotted_" + imgFile);
    for(int y = 0;;)
    {
        for(int x = 0; x < img.width(); x++)
        {
            QRgb px = img.pixel(x, y);
            int grey = qGray(px);
            if(grey < 128)
            {
                up(doth);
                down(doth);
            }

            px = img.pixel(x, y + 1);
            grey = qGray(px);
            if(grey < 128)
            {
                down(doth);
                up(doth);
            }

            if(x < img.width() - 1)
            {
                oneRight();
            }
        }
        down(2);
        //left(img.width());

        y += 2;
        if(y + 1 >= img.height())
        {
            break;
        }

        for(int x = img.width() - 1; x >= 0; x--)
        {
            QRgb px = img.pixel(x, y);
            int grey = qGray(px);
            if(grey < 128)
            {
                up(doth);
                down(doth);
            }

            px = img.pixel(x, y + 1);
            grey = qGray(px);
            if(grey < 128)
            {
                down(doth);
                up(doth);
            }

            if(x > 0)
            {
                oneLeft();
            }
        }

        y += 2;
        if(y + 1 >= img.height())
        {
            break;
        }
        down(2);
    }
    closeOutFile();
}


void MainWindow::on_bPrintDraw_clicked()
{
    int x = 0;
    int y = 0;
    int nx, ny;
    int scale = 3;

    setPixel(prnBits, 0, 0, 1);
    findNearest(0, 0, img, prnBits, &x, &y);

    while(findNearest(x, y, img, prnBits, &nx, &ny))
    {
        update();
        QApplication::processEvents();

        move(0, x, nx, scale);
        move(1, y, ny, scale);

        x = nx;
        y = ny;
    }
}

void MainWindow::on_bNiceDraw_clicked()
{
    int x = 0;
    int y = 0;
    int x1, y1;
    int x2, y2;
    int scale = 2;

    setPixel(prnBits, 0, 0, 1);
    findNearest(0, 0, img, prnBits, &x, &y);

    for(;;)
    {
        if(!findNearest(x, y, img, prnBits, &x1, &y1))
        {
            return;
        }
        update();
        QApplication::processEvents();

        for(;;)
        {
            if(!findNearest(x1, y1, img, prnBits, &x2, &y2))
            {
                move(0, x, x1, scale);
                move(1, y, y1, scale);
                return;
            }
            update();
            QApplication::processEvents();

            if(x == x1 && x == x2)
            {
                continue;
            }
            if(y == y1 && y == y2)
            {
                continue;
            }
            move(0, x, x1, scale);
            move(1, y, y1, scale);
            move(0, x1, x2, scale);
            move(1, y1, y2, scale);
            x = x2;
            y = y2;
            break;
        }
    }
}


//    int scale = 3;
//    int x = 0;
//    int y = 0;
//    int tx, ty;
//    while(findTopLeft(img, prnBits, &tx, &ty))
//    {
//        move(0, x, tx, scale);
//        move(1, y, ty, scale);
//        x = tx;
//        y = ty;
//        //move(2, 0, 100);     // pen down
//        setPixel(prnBits, x, y, 1);

//        for(;;)
//        {
//            // Move right until first white pixel
//            int right = 0;
//            for(int i = 1; i < img.width(); i++)
//            {
//                if(isWhite(img, x + i, y))
//                {
//                    break;
//                }
//                right = i;
//                setPixel(prnBits, x + i, y, 1);
//            }
//            if(right > 0)
//            {
//                move(0, x, x + right, scale);
//                x += right;
//            }

//            // Check pixel below
//            if(isWhite(img, x, y + 1))
//            {
//                //move(2, 100, 0);     // pen up
//                break;
//            }

//            // Move to line below
//            move(1, y, y + 1, scale);
//            y++;
//            setPixel(prnBits, x, y, 1);

//            update();
//            QApplication::processEvents();

//            // Move left until first white pixel
//            int left = 0;
//            for(int i = 1; i < img.width(); i++)
//            {
//                if(isWhite(img, x - i, y))
//                {
//                    break;
//                }
//                left = i;
//                setPixel(prnBits, x - i, y, 1);
//            }
//            if(left > 0)
//            {
//                move(0, x, x - left, scale);
//                x -= left;
//            }

//            // Check pixel below
//            if(isWhite(img, x, y + 1))
//            {
//                //move(2, 100, 0);     // pen up
//                break;
//            }

//            // Move to line below
//            move(1, y, y + 1, scale);
//            y++;
//            setPixel(prnBits, x, y, 1);

//            update();
//            QApplication::processEvents();
//        }
//    }
//}

void MainWindow::readSerial()
{
    int avail = port.bytesAvailable();
    if(avail <= 0)
    {
        QTimer::singleShot(1000, this, SLOT(readSerial()));
        return;
    }
    QByteArray data = port.readAll();
    qDebug() << data;
    ui->tbSerial->append(data);
    QTimer::singleShot(200, this, SLOT(readSerial()));
}

void MainWindow::on_bSendSerial_clicked()
{
    port.write(ui->tbSendSerial->text().toAscii());
}

void MainWindow::on_bXMinus_clicked()
{
    port.write("a0 p200 t0 m ");
}

void MainWindow::on_bXPlus_clicked()
{
    qDebug() << "XPlus";
    port.write("a0 p0 t200 m ");
}

void MainWindow::on_bYMinus_clicked()
{
    qDebug() << "XMinus";
    port.write("a1 p200 t0 m ");
}

void MainWindow::on_bYPlus_clicked()
{
    port.write("a1 p0 t200 m ");
}

void MainWindow::on_bZMinus_clicked()
{
    port.write("a2 p200 t0 m ");
}

void MainWindow::on_bZPlus_clicked()
{
    port.write("a2 p0 t200 m ");
}

static bool findNext(QImage & img, uchar *bits, int *x, int *y, int oldX, int oldY, int nx, int ny)
{
    if(oldX == nx && oldY == ny)
    {
        return false;       // never return to the same point
    }
    if(isWhite(img, nx, ny))
    {
        return false;
    }
    *x = nx;
    *y = ny;
    setPixel(bits, nx, ny, 1);
    return true;
}

void MainWindow::on_bMill_clicked()
{
//    for(;;)
//    {
//        move(0, 0, 5000);
//        move(1, 0, 5000);
//        move(0, 5000, 0);
//        move(1, 5000, 0);
//        move(2, 0, 200);
//    }

    QFile f("pcb.svg");
    if(!f.open(QFile::ReadOnly))
    {
        ui->tbSerial->append(f.errorString());
        return;
    }

    static qint64 x1[65535];
    static qint64 y1[65535];
    static qint64 x2[65535];
    static qint64 y2[65535];
    int count = 0;

    QRegExp rxy("d=\"([mM]) (-?\\d+\\.?\\d*),(-?\\d+\\.?\\d*)");
    QRegExp rwh("(-?\\d+\\.?\\d*),(-?\\d+\\.?\\d*)");
    QRegExp rsci("[0123456789\\.-]+e[0123456789\\.-]+");     // scientific format, e.g. -8e-4

    qint64 minX = 0xfffffff;
    qint64 maxX = 0;

    // Read and parse svg file, results are in x1,x2,y1,y2 arrays
    for(;;)
    {
        QByteArray line = f.readLine();
        if(line.isEmpty())
        {
            break;
        }
        line = line.trimmed();

        // Convert scientific small numbers to 0
        int pos = rsci.indexIn(line);
        if(pos >= 0)
        {
            line.replace(rsci.cap(0), "0");
            qDebug() << "removed scientific, new line=" << line;
        }

        pos = rxy.indexIn(line);
        if(pos < 0)
        {
            continue;
        }
        qDebug() << "line[" << count << "]=" << line;

        QString capxy = rxy.cap(0);
        qint64 x = getCoord(rxy.cap(2));
        qint64 y = getCoord(rxy.cap(3));

        line.remove(0, pos + capxy.length());

        for(;;)
        {
            pos = rwh.indexIn(line);
            if(pos < 0)
            {
                break;
            }

            QString capwh = rwh.cap(0);
            qint64 w = getCoord(rwh.cap(1));
            qint64 h = getCoord(rwh.cap(2));

            qDebug() << "x=" << x << " y=" << y << " w=" << w << " h=" << h;

            x1[count] = x;
            y1[count] = y;
            x2[count] = x + w;
            y2[count] = y + h;
            count++;            

//            drawLine(prnBits,
//                     x / 1000,
//                     y / 1000 - 500,
//                     (x + w) / 1000,
//                     (y + h) / 1000 - 500);

            x += w;
            y += h;

            minX = (x < minX ? x : minX);
            maxX = (x > maxX ? x : maxX);

            line.remove(0, pos + capwh.length());
            qDebug() << "rem=" << line;
        }
    }

    qDebug() << "MIN X=" << minX << " MAX X=" << maxX;

    // Current and target positions on svg
    qint64 cx = x1[0];
    qint64 cy = y1[0];
    qint64 tx = x2[0];
    qint64 ty = y2[0];

    // Real current and target positions (acconting driller radius)
    qint64 cX, cY, tX, tY, dummy;

    int color = 1;

    for(;;)
    {
        drawLine(prnBits,
                 cx / 1000,
                 cy / 1000 - 500,
                 tx / 1000,
                 ty / 1000 - 500,
                 color);

        // We need to take into account driller radius and shift the line
        // accordingly
        drillingPath(cx, cy, tx, ty,
                     0, 0, 0, 0,
                     9525,                    // driller radius
                     cX, cY, tX, tY,
                     dummy, dummy, dummy, dummy);

        drawLine(prnBits,
                 cX / 1000,
                 cY / 1000 - 500,
                 tX / 1000,
                 tY / 1000 - 500,
                 color);


        // 5000 steps = 43.6 mm
        // x steps    = 95.3 mm
        // x = 10,928.8990826 steps
        // 1 step = 51.9126396641 svg points
        // steps = svg coord / 51.9126396641
        int stepsCX = (cX * 1000) / 51912;
        int stepsCY = (cY * 1000) / 51912;
        int stepsTX = (tX * 1000) / 51912;
        int stepsTY = (tY * 1000) / 51912;

        move(0, stepsCX, stepsTX, 1, true);
        move(1, stepsCY, stepsTY, 1, false);

        cx = tx;
        cy = ty;
        cX = tX;
        cY = tY;

        if(cx == x1[0] && cy == y1[0])
        {
            color = color ? 0 : 1;
            move(2, 0, 200);
        }

        update();
        QApplication::processEvents();
        //Sleeper::msleep(1000);

        // Find nearest start of line
        int ndist = 0x7fffffff;
        int nindex = -1;
        for(int i = 0; i < count; i++)
        {
            qint64 dist = abs(x1[i] - cx) + abs(y1[i] - cy);
            if(dist > ndist)
            {
                continue;
            }
            ndist = dist;
            nindex = i;
        }

        tx = x2[nindex];
        ty = y2[nindex];
    }



//    int scale = 1;
//    int x = 0;
//    int y = 0;
//    int top, left;

//    // Find position in case serial communication was interrupted
//    int findX = -1;
//    int findY = -1;
//    QString posStr = ui->tbPos->text();
//    bool findPos = posStr.length() > 0;
//    if(findPos)
//    {
//        QStringList list = posStr.split(',');
//        QString strX = list.at(0);
//        QString strY = list.at(1);
//        findX = strX.toInt();
//        findY = strY.toInt();
//    }

//    findTopLeft(img, prnBits, &left, &top);

//    move(0, 0, left, scale, findPos);
//    move(1, 0, top, scale, findPos);
//    setPixel(prnBits, left, top, 1);

//    x = left;
//    y = top;

//    //
//    //    0
//    // 3     1
//    //    2
//    int dir = -1;
//    int color = 1;
//    for(;;)
//    {
//        int score0 = 0;
//        for(int i = 1; i < 50 && dir != 2; i++)
//        {
//            if(isWhite(img, x, y - i))
//            {
//                break;
//            }
//            score0++;
//        }
//        int score1 = 0;
//        for(int i = 1; i < 50 && dir != 3; i++)
//        {
//            if(isWhite(img, x + i, y))
//            {
//                break;
//            }
//            score1++;
//        }
//        int score2 = 0;
//        for(int i = 1; i < 50 && dir != 0; i++)
//        {
//            if(isWhite(img, x, y + i))
//            {
//                break;
//            }
//            score2++;
//        }
//        int score3 = 0;
//        for(int i = 1; i < 50 && dir != 1; i++)
//        {
//            if(isWhite(img, x - i, y))
//            {
//                break;
//            }
//            score3++;
//        }

//        int newX = x;
//        int newY = y;

//        if(score0 >= score1 && score0 >= score2 && score0 >= score3)
//        {
//            newY -= score0;
//            dir = 0;
//            for(int i = 1; i <= score0; i++)
//            {
//                setPixel(prnBits, x, y - i, color);
//            }
//        }
//        else if(score1 >= score2 && score1 >= score3)
//        {
//            newX += score1;
//            dir = 1;
//            for(int i = 1; i <= score1; i++)
//            {
//                setPixel(prnBits, x + i, y, color);
//            }
//        }
//        else if(score2 >= score3)
//        {
//            newY += score2;
//            dir = 2;
//            for(int i = 1; i <= score2; i++)
//            {
//                setPixel(prnBits, x, y + i, color);
//            }
//        }
//        else
//        {
//            for(int i = 1; i <= score3; i++)
//            {
//                setPixel(prnBits, x - i, y, color);
//            }
//            newX -= score3;
//            dir = 3;
//        }

//        ui->tbPos->setText(QString::number(newX) + "," + QString::number(newY));

//        update();
//        QApplication::processEvents();

//        move(0, x, newX, scale, findPos);
//        move(1, y, newY, scale, findPos);

//        x = newX;
//        y = newY;

//        if(x == left && y == top)
//        {
//            color = color ? 0 : 1;
//            port.write("a2 p0 t30 m ");
//        }

//        if(x == findX && y == findY)
//        {
//            findPos = false;    // found pos, we can start real moving
//        }
//    }
}
