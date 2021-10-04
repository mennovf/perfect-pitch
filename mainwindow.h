#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <synth.h>
#include <QThread>
#include <QKeyEvent>
#include "keyboard.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class KeyCapturer : public QObject {
    Q_OBJECT

public:
    enum Direction {
        PRESS,
        RELEASE
    };
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void notePressed(Synth::Note const);
    void onKey(int key, int direction);
    void volumeChanged(int v);
private:
    Synth::Note playing;
    void playRandomNote();
    void changeNote();
    Ui::MainWindow *ui;

    QThread synthThread;
    Synth synth;
    Keyboard * kb;

    QTimer delay_timer;
    QTimer noise_timer;
    int noise_count;

    void keyPressEvent(QKeyEvent * const e) override;
    void keyReleaseEvent(QKeyEvent * const e) override;
    bool MainWindow::eventFilter(QObject *obj, QEvent *event) override;
};
#endif // MAINWINDOW_H
