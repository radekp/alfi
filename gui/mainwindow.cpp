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

int shift = 0;

QFile outFile("../driver/data.txt");

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    imgFile = QString::null;

    penX = penY = 0;

    QFile::remove(outFile.fileName());
    outFile.open(QFile::WriteOnly);
    outFile.write("char prn[] = {3,");       // scale 3

    nextImg();
}

MainWindow::~MainWindow()
{
    delete ui;
    outFile.write("0};");
    outFile.close();
}

void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if(img.isNull())
    {
        return;
    }
    p.drawImage(0, 200, img);
    p.drawImage(img.width(), 200, prt);
    p.drawImage(img.width() + PRN_WIDTH, 200, prn);
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

int populateRegion(uchar *bits, int x, int y, QList<QPoint> & region)
{
    if(x < 0 || x >= PRN_WIDTH || y < 0 || y >= PRN_HEIGHT || getPixel(bits, x, y) == 0)
    {
        return 0;
    }
    int around =
            populateRegion(bits, x - 1, y - 1, region) +
            populateRegion(bits, x, y - 1, region) +
            populateRegion(bits, x + 1, y - 1, region) +
            populateRegion(bits, x + 1, y, region) +
            populateRegion(bits, x + 1, y + 1, region) +
            populateRegion(bits, x, y + 1, region) +
            populateRegion(bits, x - 1, y + 1, region) +
            populateRegion(bits, x - 1, y, region);

    QPoint p(x, y);
    if(around == 8)
    {
        region.append(p);           // inner points last
    }
    else
    {
        region.insert(0, p);        // border points first
    }
    setPixel(bits, x, y, 0);
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
    for(int x = 0; x < src.width(); x++)
    {
        for(int y = 0; y < src.height(); y++)
        {
            QRgb pix = src.pixel(x, y);
            int grey = qGray(pix);
            setPixel(bits, x, y, grey > 127);
        }
    }
}

void niceDraw(QImage & src, uchar *bits, MainWindow * win)
{
    fillImage(src, bits);

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

    MkPrnImg(prt, PRN_WIDTH, PRN_HEIGHT, &prtBits);
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

void MainWindow::oneUp()
{
    outFile.write("1,");

    setPixel(prnBits, penX, penY, 1);
    penY--;
    setPixel(prnBits, penX, penY, 1);
}

void MainWindow::oneDown()
{
    outFile.write("5,");

    setPixel(prnBits, penX, penY, 1);
    penY++;
    setPixel(prnBits, penX, penY, 1);
}

void MainWindow::oneLeft()
{
    outFile.write("7,");

    setPixel(prnBits, penX, penY, 1);
    penX--;
    setPixel(prnBits, penX, penY, 1);
}

void MainWindow::oneRight()
{
    outFile.write("3,");

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

void MainWindow::on_bPrintDotted_clicked()
{
    int doth = 2;

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
}

void MainWindow::on_bPrintDraw_clicked()
{
    int x = 0;
    int y = 0;
    int nx, ny;

    setPixel(prtBits, 0, 0, 1);

    while(findNearest(x, y, img, prtBits, &nx, &ny))
    {
        int dx = nx - x;
        int dy = ny - y;

        //update(x - abs(dx), y - abs(dy), abs(dx) * 2, abs(dy) * 2);
        update();
        QApplication::processEvents();

        while(dx > 0)
        {
            dx--;
            oneRight();
        }
        while(dx < 0)
        {
            dx++;
            oneLeft();
        }
        while(dy > 0)
        {
            dy--;
            oneDown();
        }
        while(dy < 0)
        {
            dy++;
            oneUp();
        }
        x = nx;
        y = ny;
    }
}



void MainWindow::on_bNiceDraw_clicked()
{
    niceDraw(img, prtBits, this);
}
