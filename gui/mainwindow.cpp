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

#define PRN_WIDTH 511
#define PRN_HEIGHT 511

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), port("/dev/ttyACM0", 9600), cmdNo(0)
{
    ui->setupUi(this);
    imgFile = QString::null;
    nextImg();
    if(!port.open(QFile::ReadWrite))
    {
        ui->tbSerial->setText(port.errorString());
    }
    readSerial();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if(img.isNull())
    {
        return;
    }
    p.drawImage(0, 200, img);
    p.drawImage(img.width(), 200, prn);
}

void redraw(MainWindow *win)
{
    win->update();
    QApplication::processEvents();
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

void MainWindow::move(int axis, int pos, int target, int scale)
{
    QString cmd = "a" + QString::number(axis) +
                  " p" + QString::number(pos * scale) +
                  " t" + QString::number(target * scale) +
                  " m" + QString::number(cmdNo) + " ";

    qDebug() << cmd;
    port.write(cmd.toAscii());

    QString expect = "done " + QString::number(cmdNo);
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
    cmdNo++;
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

void MainWindow::loadImg()
{
    //QMessageBox::information(this, "info", "loading " + imgFile);

    qDebug() << imgFile;

    img = QImage(imgFile);

    qDebug() << "img size=" << img.width() << "x" << img.height();

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
    int scale = 5;
    int x = 0;
    int y = 0;
    int tx, ty;
    while(findTopLeft(img, prnBits, &tx, &ty))
    {
        move(0, x, tx, scale);
        move(1, y, ty, scale);
        x = tx;
        y = ty;
        move(2, 0, 100);     // pen down
        setPixel(prnBits, x, y, 1);

        for(;;)
        {
            // Move right until first white pixel
            int right = 0;
            for(int i = 1; i < img.width(); i++)
            {
                if(isWhite(img, x + i, y))
                {
                    break;
                }
                right = i;
                setPixel(prnBits, x + i, y, 1);
            }
            if(right > 0)
            {
                move(0, x, x + right, scale);
                x += right;
            }

            // Check pixel below
            if(isWhite(img, x, y + 1))
            {
                move(2, 100, 0);     // pen up
                break;
            }

            // Move to line below
            move(1, y, y + 1, scale);
            y++;
            setPixel(prnBits, x, y, 1);

            update();
            QApplication::processEvents();

            // Move left until first white pixel
            int left = 0;
            for(int i = 1; i < img.width(); i++)
            {
                if(isWhite(img, x - i, y))
                {
                    break;
                }
                left = i;
                setPixel(prnBits, x - i, y, 1);
            }
            if(left > 0)
            {
                move(0, x, x - left, scale);
                x -= left;
            }

            // Check pixel below
            if(isWhite(img, x, y + 1))
            {
                move(2, 100, 0);     // pen up
                break;
            }

            // Move to line below
            move(1, y, y + 1, scale);
            y++;
            setPixel(prnBits, x, y, 1);

            update();
            QApplication::processEvents();
        }
    }
}

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
    port.write("a0 p100 t0 m ");
}

void MainWindow::on_bXPlus_clicked()
{
    port.write("a0 p0 t100 m ");
}

void MainWindow::on_bYMinus_clicked()
{
    port.write("a1 p100 t0 m ");
}

void MainWindow::on_bYPlus_clicked()
{
    port.write("a1 p0 t100 m ");
}

void MainWindow::on_bZMinus_clicked()
{
    port.write("a2 p30 t0 m ");
}

void MainWindow::on_bZPlus_clicked()
{
    port.write("a2 p0 t30 m ");
}
