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

#define MILL_LOG_LEN 90000

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
    QImage img;
    QImage prn;
    uchar *prnBits;
    QString imgFile;
    QSerialPort port;
    QString serialLog;
    int moveNo;
    QStringList cmdQueue;
    bool milling;
    int moves[MILL_LOG_LEN];
    int movesCount;
    int curZ;

    void sendCmd(QString cmd, bool flush = true);
    void flushQueue();
    void move(int axis, int pos, int target, bool justSetPos = false, bool flush = true);
    void moveBySvgCoord(int axis, qint64 pos, qint64 target, int driftX, bool justSetPos);
    void millShape(qint64 * x1, qint64 *y1, qint64 * x2, qint64 *y2,
                   int *colors, int count, int color, int driftX,
                   QStringList & lines,
                   qint64 & lastX, qint64 & lastY, bool firstPoint = true);

    void moveZ(int z, int & driftX);

private slots:
    void on_bMill_clicked();
    void on_bZPlus_clicked();
    void on_bZMinus_clicked();
    void on_bYPlus_clicked();
    void on_bYMinus_clicked();
    void on_bXPlus_clicked();
    void on_bXMinus_clicked();
    void on_bSendSerial_clicked();
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
