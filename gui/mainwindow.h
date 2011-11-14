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

private:
    Ui::MainWindow *ui;
    int lpFlags;
    QImage img;
    QImage prt;
    QString imgFile;
    void handleArrowClick(Direction);
    void loadImg();
    void nextImg();

private slots:
    void on_bPrintDraw_clicked();
    void on_bPrintDotted_clicked();
    void on_bImg_clicked();
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
