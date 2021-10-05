#include "keyboard.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
#include <array>
#include <QTimer>

Keyboard::Keyboard(int octaves, QWidget * parent) : QLabel(parent),
  octaves{octaves},
  confidence{0}
{
    this->setScaledContents(false);
    this->setMinimumSize(parent ? parent->size().width() : 900, 270);
    this->setSizePolicy({QSizePolicy::Expanding, QSizePolicy::Expanding});
    this->heightForWidth(true);

    QImage background(this->total_local_width(), TOTAL_HEIGHT, QImage::Format_RGB32);
    this->setAttribute(Qt::WA_Hover, true);

    QPainter painter(&background);
    QBrush stroke_brush(Qt::GlobalColor::black);
    QPen white_pen(stroke_brush, STROKE_WIDTH);
    painter.setPen(white_pen);

    // Draw the white keys
    QBrush white_brush(Qt::GlobalColor::white);
    painter.setBrush(white_brush);
    float x_offset = 0;
    for (int i = 0; i < 7; ++i) {
        QRect rect(x_offset, 0, WIDTH_WHITE_KEY, HEIGHT_WHITE_KEY);
        this->key_rects[WHITE_KEY_INDICES[i]] = rect;
        for (int octave = 0; octave < octaves; ++octave) {
            rect.moveLeft(x_offset + octave*OFFSET);
            painter.drawRect(rect);
        }
        x_offset += WIDTH_WHITE_KEY;
    }

    // Black keys
    QBrush black_brush(Qt::GlobalColor::black);
    painter.setBrush(black_brush);
    painter.setPen(Qt::PenStyle::NoPen);
    x_offset = WIDTH_WHITE_KEY - (WIDTH_BLACK_KEY / 2);
    for (int i = 0; i < 2; ++i) {
        QRect rect(x_offset, 0, WIDTH_BLACK_KEY, HEIGHT_BLACK_KEY);
        this->key_rects[BLACK_KEY_INDICES[i]] = rect;
        for (int octave = 0; octave < octaves; ++octave) {
            rect.moveLeft(x_offset + OFFSET*octave);
            painter.drawRect(rect);
        }
        x_offset += WIDTH_WHITE_KEY;
    }
    x_offset += WIDTH_WHITE_KEY;
    for (int i = 2; i < 5; ++i) {
        QRect rect(x_offset, 0, WIDTH_BLACK_KEY, HEIGHT_BLACK_KEY);
        this->key_rects[BLACK_KEY_INDICES[i]] = rect;
        for (int octave = 0; octave < octaves; ++octave) {
            rect.moveLeft(x_offset + OFFSET*octave);
            painter.drawRect(rect);
        }
        x_offset += WIDTH_WHITE_KEY;
    }

    QPixmap pix;
    pix.convertFromImage(background);
    this->layers[LayerId::BACKGROUND].pixels = pix;
    this->layers[LayerId::BACKGROUND].mask = {pix.rect()};
    this->layers[LayerId::BACKGROUND].composition_mode = QPainter::CompositionMode_Source;

    this->layers[LayerId::HOVER].mask = QRegion();
    this->layers[LayerId::HOVER].pixels = pix;
    this->layers[LayerId::HOVER].pixels.fill(QColor::fromRgb(0, 0, 255));
    this->layers[LayerId::HOVER].composition_mode = QPainter::CompositionMode_Source;

    this->layers[LayerId::CORRECT].pixels = pix;
    this->layers[LayerId::CORRECT].pixels.fill(QColor::fromRgb(0, 255, 0));
    this->layers[LayerId::CORRECT].mask = QRegion();
    this->layers[LayerId::CORRECT].composition_mode = QPainter::CompositionMode_Source;
    this->composite = pix;

    this->compose(this->size());


    this->flickerState = false;
    this->flickerTimer.setInterval(100);
    this->flickerTimer.setSingleShot(false);
    connect(&this->flickerTimer, &QTimer::timeout, this, [this]() {
        this->flickerState = !this->flickerState;

        if (this->disable_flicker.isActive() && this->flickerState) {
            int const gpci = std::get<0>(this->correct)*12 + std::get<1>(this->correct);
            this->layers[LayerId::CORRECT].mask = this->range_region(gpci, gpci);
        } else {
            this->layers[LayerId::CORRECT].mask = QRegion();
        }
        this->compose(this->size());

        if (!this->disable_flicker.isActive()) {
            this->flickerTimer.stop();
        }
    });
    this->flickerTimer.start();

    this->disable_flicker.setInterval(2000);
    this->disable_flicker.setSingleShot(true);
    connect(&this->disable_flicker, &QTimer::timeout, this, [this]() { });
}

void Keyboard::flicker_correct(int octave, Synth::PitchClass p) {
    this->correct = {octave, p};
    this->flickerState = true;
    this->disable_flicker.start();
    this->flickerTimer.start();
    this->compose(this->size());
}

void Keyboard::compose(QSize const size) {
    QPainter p(&this->composite);
    for (size_t i = 0; i < this->layers.size(); ++i) {
        auto const& layer = this->layers[i];
        p.setCompositionMode(layer.composition_mode);
        p.setClipRegion(layer.mask);
        p.fillRect(this->composite.rect(), layer.pixels);
    }
    p.end();

    QPixmap px = this->composite.scaled(size, Qt::KeepAspectRatio);
    this->scale = static_cast<qreal>(px.width()) / this->total_local_width();
    this->setPixmap(px);
}

std::tuple<int, size_t> Keyboard::key_from_pos(QPoint const& p) const {
    qreal const yoffset = (this->size().height() - this->pixmap(Qt::ReturnByValueConstant{}).size().height()) / 2;
    qreal const y = (p.y() - yoffset) / this->scale;

    qreal const xt = p.x() / this->scale;
    int const octave = xt / OFFSET;
    qreal const x = xt - octave*OFFSET;

    std::array<bool, 12> matches;
    matches.fill(false);

    for (size_t i = 0; i < this->key_rects.size(); ++i) {
        auto key_rect = this->key_rects[i];
        key_rect.setHeight(key_rect.height() + STROKE_WIDTH);
        if (key_rect.contains(x, y)) {
            matches[i] = true;
        }
    }

    for (auto const i : BLACK_KEY_INDICES) {
        if (matches[i]) {
            Synth::PitchClass note_class = static_cast<Synth::PitchClass>(i);
            return {octave, note_class};
        }
    }
    for (auto const i : WHITE_KEY_INDICES) {
        if (matches[i]) {
            Synth::PitchClass note_class = static_cast<Synth::PitchClass>(i);
            return {octave, note_class};
        }
    }
    return {-1, 0};
}

void Keyboard::mouseReleaseEvent(QMouseEvent *ev) {
    auto const& [octave, pitch_class] = this->key_from_pos(ev->pos());
    if (octave != -1) {
        emit pressed(octave, static_cast<Synth::PitchClass>(pitch_class));
    }
}

void Keyboard::hoverEnter(QHoverEvent * e) {
    this->last_hover_event = *e;
};

void Keyboard::hoverLeave(QHoverEvent * e) {
    this->layers[LayerId::HOVER].mask = QRegion();
    this->compose(this->size());
    this->last_hover_event = std::nullopt;
}


QRegion Keyboard::range_region(int low, int high) const {
    QRegion mask;
    for (int pc : WHITE_KEY_INDICES) {
        for (int local_octave = 0; local_octave < this->octaves; ++local_octave) {
            int const i = pc + local_octave * 12;
            if (i >= low && i <= high) {
                QRect key_rect = key_rects[pc];
                key_rect.moveLeft(key_rect.left() + local_octave*OFFSET + STROKE_WIDTH);
                key_rect.moveTop(key_rect.top() + STROKE_WIDTH);
                key_rect.setWidth(key_rect.width() - STROKE_WIDTH);
                key_rect.setHeight(key_rect.height() - STROKE_WIDTH);
                mask = mask.united(key_rect);
            }
        }
    }

    for (int pc : BLACK_KEY_INDICES) {
        for (int local_octave = 0; local_octave < this->octaves; ++local_octave) {
            QRect key_rect = key_rects[pc];
            key_rect.moveLeft(key_rect.left() + local_octave*OFFSET);
            mask = mask.subtracted(key_rect);
        }
    }

    for (int pc : BLACK_KEY_INDICES) {
        for (int local_octave = 0; local_octave < this->octaves; ++local_octave) {
            int const i = pc + local_octave * 12;
            if (i >= low && i <= high) {
                QRect key_rect = key_rects[pc];
                key_rect.moveLeft(key_rect.left() + local_octave*OFFSET + STROKE_WIDTH);
                key_rect.moveTop(key_rect.top() + STROKE_WIDTH);
                key_rect.setWidth(key_rect.width() - 2*STROKE_WIDTH);
                key_rect.setHeight(key_rect.height() - 2*STROKE_WIDTH);
                mask = mask.united(key_rect);
            }
        }
    }
    return mask;
}

void Keyboard::hoverMove(QHoverEvent * ev) {
    auto const& [octave, pitch_class] = this->key_from_pos(ev->pos());

    if (octave == -1) {
        this->hoverLeave(ev);
        return;
    }

    int const pci = static_cast<int>(pitch_class);
    int const gpci = octave * 12 + pci;

    this->layers[LayerId::HOVER].mask = this->range_region(gpci - this->confidence, gpci + this->confidence);
    this->compose(this->size());
    this->last_hover_event = *ev;
}

bool Keyboard::event(QEvent * e) {
    switch(e->type())
    {
    case QEvent::HoverEnter:
        hoverEnter(static_cast<QHoverEvent*>(e));
        return true;
        break;
    case QEvent::HoverLeave:
        hoverLeave(static_cast<QHoverEvent*>(e));
        return true;
        break;
    case QEvent::HoverMove:
        hoverMove(static_cast<QHoverEvent*>(e));
        return true;
        break;
    default:
        break;
    }
    return QWidget::event(e);
}

void Keyboard::change_confidence(int cf) {
    this->confidence = cf;
    if (this->last_hover_event) {
        this->hoverMove(&this->last_hover_event.value());
    }
}

void Keyboard::set_correct_duration(double s) {
    int const msec = (s > 2) ? s*1000 : 2000;
    this->disable_flicker.setInterval(msec);
}

void Keyboard::resizeEvent(QResizeEvent * const e) {
    QLabel::resizeEvent(e);
    this->compose(e->size());
}

QSize Keyboard::sizeHint() const {
    return {this->width(), this->heightForWidth(this->width())};
}
int Keyboard::heightForWidth(int w) const {
    int const h =  qRound(w * TOTAL_HEIGHT / total_local_width());
    return h;
}

int Keyboard::total_local_width() const {
    return this->octaves * Keyboard::OFFSET + STROKE_WIDTH;
}

int Keyboard::aspect_ratio() const {
    return TOTAL_HEIGHT / total_local_width();
}
