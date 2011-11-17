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
    //p.drawImage(img.width(), 200, prt);
    p.drawImage(img.width(), 200, prn);
}

uchar getSetPixel(QImage & img, int x, int y, bool get, uchar val)
{
    uchar *bits = img.bits();
    int index = y * (img.width() + 1) + x;
    if(get)
    {
        return bits[index];
    }
    bits[index] = val;
    return 0;
}

uchar getPixel(QImage & img, int x, int y)
{
    return getSetPixel(img, x, y, true, 0);
}

void setPixel(QImage & img, int x, int y, uchar val)
{
    getSetPixel(img, x, y, false, val);
}

bool findNearest(int x, int y, QImage & img, QImage & prt, int *nx, int *ny)
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
                if(getPixel(prt, px, py))
                {
                    continue;
                }
                setPixel(prt, px, py, 1);
                *nx = px;
                *ny = py;
                return true;
            }
        }
    }
    return false;
}

static void MkPrnImg(QImage &img, int width, int height)
{
    img = QImage(width, height, QImage::Format_Indexed8);

    img.setNumColors(2);
    img.setColor(0, qRgb(255, 255, 255));
    img.setColor(1, qRgb(0, 0, 0));

    uchar *bits = img.bits();
    memset(bits, 0, width * height);
    bits[0] = 1;
}

void MainWindow::loadImg()
{
    //QMessageBox::information(this, "info", "loading " + imgFile);

    qDebug() << imgFile;

    img = QImage(imgFile);

    qDebug() << "img size=" << img.width() << "x" << img.height();

    MkPrnImg(prt, 511, 511);
    MkPrnImg(prn, 511, 511);
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

    setPixel(prn, penX, penY, 1);
    penY--;
    setPixel(prn, penX, penY, 1);
}

void MainWindow::oneDown()
{
    outFile.write("5,");

    setPixel(prn, penX, penY, 1);
    penY++;
    setPixel(prn, penX, penY, 1);
}

void MainWindow::oneLeft()
{
    outFile.write("7,");

    setPixel(prn, penX, penY, 1);
    penX--;
    setPixel(prn, penX, penY, 1);
}

void MainWindow::oneRight()
{
    outFile.write("3,");

    setPixel(prn, penX, penY, 1);
    penX++;
    setPixel(prn, penX, penY, 1);
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

    while(findNearest(x, y, img, prt, &nx, &ny))
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


