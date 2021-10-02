#include "synth.h"
#include <QDebug>
#include <cmath>
#include <limits>
#include <QTimer>
#include <stdlib.h>

Synth::Synth(QObject *parent) : QObject(parent),
    envelope{15, 13, 0.7, 0., 2},
    frequency{0},
    synthVST{},
    outputDevice{nullptr},
    rawOutputDevice{nullptr}
{
    this->sample_buffer.resize(20000);
}

void Synth::start() {
    this->format.setSampleRate(22050);
    this->format.setChannelCount(1);
    this->format.setSampleSize(16);
    this->format.setCodec("audio/pcm");
    this->format.setByteOrder(QAudioFormat::LittleEndian);
    this->format.setSampleType(QAudioFormat::SignedInt);

    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported(format)) {
        qWarning()<<"raw audio format not supported by backend, cannot play audio.";
        return;
    }

    this->outputDevice =  decltype(this->outputDevice)::create(format, nullptr);
    this->connect(this->outputDevice.get(), &QAudioOutput::notify, this, &Synth::writeSamples);

    this->generationTimer = decltype(this->generationTimer)::create();
    this->generationTimer->setSingleShot(false);
    this->generationTimer->setInterval(1);
    this->generationTimer->start();

    int constexpr notify_interval_ms = 10;
    this->outputDevice->setNotifyInterval(notify_interval_ms);
    if (this->outputDevice->notifyInterval() != notify_interval_ms) {
        qWarning() << "Notify interval doesn't match: " << this->outputDevice->notifyInterval();
    }
    this->rawOutputDevice = this->outputDevice->start();
}

void Synth::stop() {
    if (this->outputDevice) {
        this->outputDevice->stop();
    }
}

void Synth::stopNote() {
    this->envelope.off();
}

void Synth::playNote(Note const note) {
    // Calculate the frequency, store it, generate it on the callback
    static constexpr double reference_freq = 440;
    static constexpr int reference_octave = 4;

    int const octave_diff = note.octave - reference_octave;
    int const note_diff = note.note_class - PitchClass::A;

    this->playFrequency(reference_freq * std::pow(2, octave_diff + note_diff / 12.));
}

void Synth::playFrequency(double freq) {
    this->frequency = freq;
    std::fill(std::begin(this->phase_offsets), std::end(this->phase_offsets), 0);
    this->envelope.on();
}

void Synth::writeSamples() {
    double constexpr harmonics_db[] = {-31, -46, -54, -52, -68, -55, -55};
    int const to_generate = this->outputDevice->bytesFree() / 2;
    double const sample_period = 1. / this->format.sampleRate();
    double const normalization = 440. / this->frequency;

    for (int i = 0, j = 0; i < to_generate; ++i, j += 2) {
        double samplef = 0;
        for (int h = 0; h < 7; h++) {
            double const harmonic_power = std::pow(10, (harmonics_db[h] - harmonics_db[0]) / 10.);
            samplef += harmonic_power * std::sin(this->phase_offsets[h]);
            this->phase_offsets[h] += this->frequency * (h+1) * 2 * 3.1415926 * sample_period;
        }
        samplef *= normalization * this->envelope.value() * std::numeric_limits<int16_t>::max() / 7;
        uint16_t const sample = samplef;
        char bytes[sizeof(sample)];
        memcpy(bytes, &sample, sizeof(sample));
        sample_buffer[j] = (bytes[0]);
        sample_buffer[j + 1] = (bytes[1]);
        this->envelope.advance(sample_period);
    }
    this->rawOutputDevice->write(this->sample_buffer.data(), to_generate * 2);

    // Renormalize phase_offsets
    for (int h = 0; h < 7; h++) {
        this->phase_offsets[h] = fmod(this->phase_offsets[h], 2*3.14159265);
    }
}

Synth::ADSREnvelope::ADSREnvelope(double a, double d, double sv, double sr, double r):
    t{0},
    attack{a},
    decay{d},
    sustain_value{sv},
    sustain_rate{sr},
    release{r},
    state{Synth::ADSREnvelope::State::IDLE} {
}

void Synth::ADSREnvelope::advance(double dt) {
    if (this->state == State::IDLE) return;
    this->t += dt;
    switch (this->state) {
        case (State::IDLE): break;
        case (State::ATTACK): {
            this->cvalue += dt * this->attack;
            if (this->cvalue >= 1.0) {
                this->state = State::DECAY;
                this->t = 0;
            }
        } break;
        case (State::DECAY): {
            this->cvalue -= dt * this->decay;
            if (this->cvalue <= this->sustain_value) {
                this->state = State::SUSTAIN;
                this->t = 0;
            }
        } break;
        case (State::SUSTAIN): {
            this->cvalue -= dt * this->sustain_rate;
            if (this->cvalue <= 0) {
                this->state = State::IDLE;
                this->t = 0;
            }
        } break;
        case (State::RELEASE): {
            this->cvalue -= dt * this->release;
            if (this->cvalue <= 0) {
                this->state = State::IDLE;
                this->t = 0;
            }
        } break;
    }
}

double Synth::ADSREnvelope::value() {
    return this->cvalue;
}

void Synth::ADSREnvelope::on() {
    this->state = State::ATTACK;
    this->t = 0;
    this->cvalue = 0;
}

void Synth::ADSREnvelope::off() {
    this->state = State::RELEASE;
}

std::ostream& operator<<(std::ostream& out, Synth::PitchClass c) {
    return out << Synth::NOTECLASS_NAMES[c];
}

void Synth::changeVolume(double v) {
    this->outputDevice->setVolume(v);
}
