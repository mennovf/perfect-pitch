#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPushButton>
#include <QRandomGenerator>
#include <QDebug>
#include "keyboard.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      playing{0, 0}
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    synth.moveToThread(&this->synthThread);
    this->synthThread.start();

    this->synth.start();

    this->kb = new Keyboard(3);
    connect(this->kb, &Keyboard::pressed, this, [this](int octave, Synth::PitchClass note_class) {
        this->notePressed({octave + 3, note_class});
    });
    this->ui->keyboards->addWidget(kb);

    QObject::connect(this->ui->confidence, QOverload<int>::of(&QSpinBox::valueChanged), kb, &Keyboard::change_confidence);

    this->installEventFilter(this);
    this->playRandomNote();

    connect(this->ui->volumeSlider, &QSlider::valueChanged, this, &MainWindow::volumeChanged);
}

void MainWindow::volumeChanged(int v) {
    double const volume = QAudio::convertVolume(v / qreal(100), QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
    this->synth.changeVolume(volume);
}

MainWindow::~MainWindow()
{
    this->synth.stop();
    this->synthThread.exit();
    this->synthThread.wait();
    delete ui;
}

void MainWindow::playRandomNote() {
    QRandomGenerator * const random = QRandomGenerator::global();
    this->playing = Synth::Note{random->bounded(3, 6), random->bounded(0, Synth::NOTECLASS_AMOUNT)};
    this->synth.playNote(this->playing);
    qDebug() << "Playing Note=" << this->playing;
}

void MainWindow::notePressed(Synth::Note const chosen) {
    int const confidence = this->ui->confidence->value();
    int const difference = (chosen.octave - this->playing.octave)*12 + (chosen.note_class - this->playing.note_class);

    if (std::abs(difference) <= confidence) {
        this->ui->statusbar->showMessage(QString("Correct: %1. Guess: %2. Off by %3").arg(this->playing).arg(chosen).arg(difference));
        this->kb->flicker_correct(this->playing.octave - 3, this->playing.note_class);
        playRandomNote();
    } else {
        this->ui->statusbar->showMessage("Wrong");
    }
}

void MainWindow::keyPressEvent(QKeyEvent * const e) {
    if (e ->isAutoRepeat()) return;
    this->onKey(e->key(), KeyCapturer::PRESS);
}
void MainWindow::keyReleaseEvent(QKeyEvent * const e) {
    if (e ->isAutoRepeat()) return;
    this->onKey(e->key(), KeyCapturer::RELEASE);
}
void MainWindow::onKey(int key, int direction) {
    bool noted = false;
    Synth::PitchClass note;
    switch (key) {
        case (Qt::Key_A): note = Synth::PitchClass::A; noted = true; break;
        case (Qt::Key_B): note = Synth::PitchClass::B; noted = true; break;
        case (Qt::Key_C): note = Synth::PitchClass::C; noted = true; break;
        case (Qt::Key_D): note = Synth::PitchClass::D; noted = true; break;
        case (Qt::Key_E): note = Synth::PitchClass::E; noted = true; break;
        case (Qt::Key_F): note = Synth::PitchClass::F; noted = true; break;
        case (Qt::Key_G): note = Synth::PitchClass::G; noted = true; break;
    }

    if (noted) {
        Synth::Note const fullNote = {4, note};
        bool const alreadyPlaying = this->playing == fullNote;
        if (direction == KeyCapturer::PRESS && !alreadyPlaying) {
            this->playing = fullNote;
            this->synth.playNote(fullNote);
        } else if (alreadyPlaying) {
            this->playing = {0, 0};
            this->synth.stopNote();
        }
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    QKeyEvent *keyEvent = NULL;
    bool result = false;


    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
         keyEvent = dynamic_cast<QKeyEvent*>(event);
         this->keyPressEvent(keyEvent);
         result = true;
    }  else if (event->type() == QEvent::KeyRelease) {
        keyEvent = dynamic_cast<QKeyEvent*>(event);
        this->keyReleaseEvent(keyEvent);
        result = true;
    }

    //### Standard event processing ###
    else
        result = QObject::eventFilter(obj, event);

    return result;
}
