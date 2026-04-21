#ifndef LTR_WORKERS_H
#define LTR_WORKERS_H

#include <QObject>
#include <QMutex>
#include <QVector>
#include <QString>
#include <atomic>

#include "LTR/ltrapi.h"
#include "LTR/ltr114api.h"

class LTR114;
class LTR212;

struct SyncState
{
    QMutex mutex;
    bool needSynchronization = false;
    bool refInitialized = false;
    quint32 refStartMark = 0;
    quint32 refSecondMark = 0;
    bool seen114 = false;
    bool seen212 = false;
    quint32 start114 = 0;
    quint32 start212 = 0;
    quint32 second114 = 0;
    quint32 second212 = 0;
    quint64 timeBaseTicks = 1000000ULL;
};

struct TimedSample
{
    quint64 globalTick = 0;      // Общая шкала: 1_000_000 тиков = 1 секунда
    quint32 startMark = 0;       // Верхние 16 бит tmark
    quint32 secondMark = 0;      // Нижние 16 бит tmark
    quint32 sampleInSecond = 0;  // Позиция в текущей секунде
    double value = 0.0;
};

Q_DECLARE_METATYPE(TimedSample)
Q_DECLARE_METATYPE(QVector<TimedSample>)

class Ltr114Worker : public QObject
{
    Q_OBJECT
public:
    explicit Ltr114Worker(LTR114* module, SyncState* syncState, QObject* parent = nullptr);

public slots:
    void run();
    void stopAcquisition();

signals:
    void newVoltageSamples(const QVector<TimedSample>& voltageSamples);
    void acquisitionError(const QString& message);
    void finished();

private:
    struct TimePoint
    {
        quint64 globalTick = 0;
        quint32 startMark = 0;
        quint32 secondMark = 0;
        quint32 sampleInSecond = 0;
    };

    QVector<TimePoint> buildTimeline(const QVector<DWORD>& tmarks);
    QVector<TimedSample> attachTimeline(const QVector<double>& values, const QVector<TimePoint>& timeline) const;
    static quint32 extractStartMark(DWORD tmarkWord);
    static quint32 extractSecondMark(DWORD tmarkWord);
    quint32 unwrapCounter(quint16 rawValue, quint16& lastRaw, quint32& epoch) const;
    bool shouldAcceptSample(quint32 startMark, quint32 secondMark) const;
    void initializeReferenceIfNeeded(quint32 startMark, quint32 secondMark);

    LTR114* m_module;
    SyncState* m_syncState;
    std::atomic_bool m_running{false};
    quint32 m_currentSecond = 0;
    quint32 m_samplesInSecond = 0;
    double m_estSamplesPerSecond = 2000.0;
    mutable quint16 m_lastSecondRaw = 0;
    mutable quint16 m_lastStartRaw = 0;
    mutable quint32 m_secondEpoch = 0;
    mutable quint32 m_startEpoch = 0;
    mutable bool m_wrapInitialized = false;
};

class Ltr212Worker : public QObject
{
    Q_OBJECT
public:
    explicit Ltr212Worker(LTR212* module, SyncState* syncState, QObject* parent = nullptr);

public slots:
    void run();
    void stopAcquisition();

signals:
    void newVoltageSamples(const QVector<TimedSample>& voltageSamples);
    void acquisitionError(const QString& message);
    void finished();

private:
    struct TimePoint
    {
        quint64 globalTick = 0;
        quint32 startMark = 0;
        quint32 secondMark = 0;
        quint32 sampleInSecond = 0;
    };

    QVector<TimePoint> buildTimeline(const QVector<DWORD>& tmarks);
    QVector<TimedSample> remapAndAttachTimeline(const QVector<double>& values, const QVector<TimePoint>& timeline) const;
    static quint32 extractStartMark(DWORD tmarkWord);
    static quint32 extractSecondMark(DWORD tmarkWord);
    quint32 unwrapCounter(quint16 rawValue, quint16& lastRaw, quint32& epoch) const;
    bool shouldAcceptSample(quint32 startMark, quint32 secondMark) const;
    void initializeReferenceIfNeeded(quint32 startMark, quint32 secondMark);

    LTR212* m_module;
    SyncState* m_syncState;
    std::atomic_bool m_running{false};
    quint32 m_currentSecond = 0;
    quint32 m_samplesInSecond = 0;
    double m_estSamplesPerSecond = 2000.0;
    mutable quint16 m_lastSecondRaw = 0;
    mutable quint16 m_lastStartRaw = 0;
    mutable quint32 m_secondEpoch = 0;
    mutable quint32 m_startEpoch = 0;
    mutable bool m_wrapInitialized = false;
};


#endif // LTR_WORKERS_H
