#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/QMainWindow>
#include <QWidget>
#include <QMouseEvent>
#include <QTimer>
#include <QImage>
#include <QThread>
#include <QPainter>
#include <QRgb>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QDebug>
#include <QRegExp>
#include <math.h>

#include "qserialport.h"

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    enum Direction
    {
        DirectionNone,
        DirectionUp,
        DirectionDown,
        DirectionLeft,
        DirectionRight,
    };

protected:
    void paintEvent(QPaintEvent *);

public:
    Ui::MainWindow *ui;
    int lpFlags;
    QImage img;
    QImage prn;
    uchar *prnBits;
    QString imgFile;
    int penX;
    int penY;
    QSerialPort port;
    QString serialLog;
    int moveNo;
    QStringList cmdQueue;
    bool milling;

    void handleArrowClick(Direction);
    void loadImg();
    void nextImg();
    void oneUp();
    void oneDown();
    void oneLeft();
    void oneRight();
    void up(int times);
    void down(int times);
    void left(int times);
    void right(int times);
    void sendCmd(QString cmd, bool flush = true);
    void flushQueue();
    void move(int axis, int pos, int target, bool justSetPos = false, bool flush = true);
    void moveBySvgCoord(int axis, int pos, int target, int driftX, bool justSetPos);
    void millShape(qint64 * x1, qint64 *y1, qint64 * x2, qint64 *y2,
                   int *colors, int count, int color, int driftX,
                   QStringList & lines,
                   qint64 & lastX, qint64 & lastY);

    void moveZ(int zDirection, int & driftX);

private slots:
    void on_bMillPath_clicked();
    void on_bMill_clicked();
    void on_bZPlus_clicked();
    void on_bZMinus_clicked();
    void on_bYPlus_clicked();
    void on_bYMinus_clicked();
    void on_bXPlus_clicked();
    void on_bXMinus_clicked();
    void on_bSendSerial_clicked();
    void on_bPrintDraw_clicked();
    void on_bPrintDotted_clicked();
    void on_bImg_clicked();
    void on_bNiceDraw_clicked();
    void readSerial();
};

class Sleeper : public QThread
{
public:
    static void msleep(unsigned long msecs)
    {
        QThread::msleep(msecs);
    }
};

#endif // MAINWINDOW_H
