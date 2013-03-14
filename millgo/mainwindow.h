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

protected:
    void paintEvent(QPaintEvent *);

public:
    Ui::MainWindow *ui;
    QImage img;
    QImage prn;
    QString imgFile;
    QSerialPort port;
    QString serialLog;
    int moveNo;
    QStringList cmdQueue;
    bool milling;
    bool preview;
    int curX;           // cursor x, y, z
    int curY;
    int curZ;

    void sendCmd(QString cmd, bool flush = true);
    void writeCmdQueue();
    void waitCmdDone();
    void move(int x, int y, int z);
    void moveBySvgCoord(int axis, qint64 pos, qint64 target, int driftX, bool justSetPos);
    void millShape(qint64 * x1, qint64 *y1, qint64 * x2, qint64 *y2,
                   int *colors, int count, int color, int driftX,
                   QStringList & lines,
                   qint64 & lastX, qint64 & lastY, bool firstPoint = true);

    void moveZ(int z, int & driftX);
    void mill();

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
    void on_bPreview_clicked();
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
