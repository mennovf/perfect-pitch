#ifndef SYNTH_H
#define SYNTH_H

#include <QObject>
#include <QLibrary>
#include <QAudioOutput>
#include <QSharedPointer>
#include <stdint.h>
#include <QMutex>
#include <tuple>

class Synth : public QObject {
    Q_OBJECT
public:
    explicit Synth(QObject *parent = nullptr);

    enum PitchClass {
        C,
        Csharp,
        Dflat = Csharp,
        D,
        Dsharp,
        Eflat = Dsharp,
        E,
        Esharp,
        Fflat = E,
        F = Esharp,
        Fsharp,
        Gflat = Fsharp,
        G,
        Gsharp,
        Aflat = Gsharp,
        A,
        Asharp,
        Bflat = Asharp,
        B,
        Bsharp = C,
        Cflat = B,
        NOTECLASS_AMOUNT
    };

    struct Note {
        int octave;
        PitchClass note_class;

        Note(): Note{4, PitchClass::A} {}
        Note(int o, int c): octave{o}, note_class{static_cast<PitchClass>(c)} {}
        bool operator==(Note other) const { return std::make_tuple(octave, note_class) == std::make_tuple(other.octave, other.note_class); }

        operator QString() const {
            return QString("%1%2").arg(Synth::NOTECLASS_NAMES[this->note_class]).arg(this->octave);
        }
    };

    class Parameter {
        using Float = double;

    public:
        Parameter(Float current, Float rate);
        Float get() const;
        void set(Float);
        void advance(int ms);
    private:
        Float current;
        Float target;
        Float rate;
    };


    class ADSREnvelope {
    public:
        ADSREnvelope(double a, double d, double sv, double sr, double r);
        void advance(double dt);
        double value();
        void on();
        void off();
    protected:
        double t;
        double cvalue;
        enum class State {
            IDLE,
            ATTACK,
            DECAY,
            SUSTAIN,
            RELEASE,
            OFF
        };

        double attack, decay, sustain_value, sustain_rate, release;
        State state;
    };

    static constexpr char const * const NOTECLASS_NAMES[NOTECLASS_AMOUNT] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

public slots:
    void start();
    void stop();
    void playNote(Note const note);
    void stopNote();
    void playFrequency(double freq);
    void changeVolume(double v);

protected slots:
    void writeSamples();
signals:

protected:
    ADSREnvelope envelope;
    QAudioFormat format;
    Parameter volume;
    double frequency;
    double phase_offsets[7];
    QMutex sample_buffer_mutex;
    QVector<char> sample_buffer;
    QLibrary synthVST;
    QSharedPointer<QAudioOutput> outputDevice;
    QSharedPointer<QTimer> generationTimer;
    QSharedPointer<QTimer> volumeTimer;
    QIODevice * rawOutputDevice;
};

#endif // SYNTH_H
