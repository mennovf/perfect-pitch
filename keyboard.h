#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <QLabel>
#include <QPainter>
#include <QTimer>
#include <array>
#include "synth.h"

class Keyboard : public QLabel
{
    Q_OBJECT
public:
    Keyboard(int octaves = 1, QWidget * parent = nullptr);

signals:
    void pressed(int octave, Synth::PitchClass);
public slots:
    void change_confidence(int);
    void flicker_correct(int octave, Synth::PitchClass);
    void set_correct_duration(double s);
private:
    static constexpr qreal WIDTH_WHITE_KEY = 10;
    static constexpr qreal HEIGHT_WHITE_KEY = 50;
    static constexpr qreal WIDTH_BLACK_KEY = 6;
    static constexpr qreal HEIGHT_BLACK_KEY = 30;
    static constexpr qreal STROKE_WIDTH = 1;
    static constexpr qreal OFFSET = WIDTH_WHITE_KEY*7;
    static constexpr qreal WIDTH = OFFSET + STROKE_WIDTH;
    static constexpr std::array<int, 7> WHITE_KEY_INDICES = {0, 2, 4, 5, 7, 9, 11};
    static constexpr std::array<int, 5> BLACK_KEY_INDICES = {1, 3, 6, 8, 10};
    qreal scale;
    int octaves;

    std::array<QRect, 12> key_rects;

    struct Layer {
        QPixmap pixels;
        QRegion mask;
        QPainter::CompositionMode composition_mode;
    };

    enum LayerId {
        BACKGROUND,
        HOVER,
        CORRECT
    };

    std::array<Layer, 3> layers;
    QPixmap composite;

    int confidence;
    void compose();
    std::tuple<int, size_t> key_from_pos(QPoint const&) const;
    void mouseReleaseEvent(QMouseEvent *ev) override;

    void hoverEnter(QHoverEvent * event);
    void hoverLeave(QHoverEvent * event);
    void hoverMove(QHoverEvent * event);
    bool event(QEvent * e) override;
    QRegion range_region(int low, int high) const;

    bool flickerState;
    QTimer flickerTimer;

    std::tuple<int, Synth::PitchClass> correct;
    QTimer disable_flicker;
};

#endif // KEYBOARD_H
