#include "ltr_workers.h"

#include <QThread>
#include <QMutexLocker>
#include <utility>

#include "ltr114.h"
#include "ltr212.h"

namespace {
constexpr DWORD kRecvTimeoutMs = 50;
}

Ltr114Worker::Ltr114Worker(LTR114* module, SyncState* syncState, QObject* parent)
    : QObject(parent)
    , m_module(module)
    , m_syncState(syncState)
{
}

void Ltr114Worker::run()
{
    if (!m_module) {
        emit acquisitionError("LTR114 worker: модуль не инициализирован");
        emit finished();
        return;
    }

    m_running.store(true);
    m_wrapInitialized = false;
    m_secondEpoch = m_startEpoch = 0;

    while (m_running.load()) {
        int error = 0;
        auto [rawData, tmarks] = m_module->receive_data_with_marks(kRecvTimeoutMs, &error);

        if (error != 0) {
            if (!m_running.load()) {
                break;
            }
            emit acquisitionError(QString("LTR114: ошибка приёма %1").arg(error));
            break;
        }

        if (rawData.isEmpty()) {
            QThread::msleep(1);
            continue;
        }

        const QVector<TimePoint> timeline = buildTimeline(tmarks);
        if (timeline.isEmpty()) {
            QThread::msleep(1);
            continue;
        }

        QVector<DWORD> dataToProcess = rawData;
        QVector<double> procData(dataToProcess.size());
        QVector<double> thermData(dataToProcess.size());
        INT procSize = dataToProcess.size();
        INT thermCount = 0;

        INT result = LTR114_ProcessDataTherm(m_module->handle(),
                                             const_cast<DWORD*>(dataToProcess.constData()),
                                             procData.data(),
                                             thermData.data(),
                                             &procSize,
                                             &thermCount,
                                             LTR114_CORRECTION_MODE_INIT,
                                             LTR114_PROCF_VALUE);

        if (result != LTR_OK) {
            if (!m_running.load()) {
                break;
            }
            emit acquisitionError(QString("LTR114: ошибка обработки %1").arg(result));
            break;
        }

        procData.resize(procSize);
        const QVector<TimedSample> timed = attachTimeline(procData, timeline);
        if (!timed.isEmpty()) {
            emit newVoltageSamples(timed);
        }

        QThread::msleep(1);
    }

    m_running.store(false);
    emit finished();
}

void Ltr114Worker::stopAcquisition()
{
    m_running.store(false);
}

quint32 Ltr114Worker::extractStartMark(DWORD tmarkWord)
{
    return (tmarkWord >> 16) & 0xFFFF;
}

quint32 Ltr114Worker::extractSecondMark(DWORD tmarkWord)
{
    return tmarkWord & 0xFFFF;
}

quint32 Ltr114Worker::unwrapCounter(quint16 rawValue, quint16& lastRaw, quint32& epoch) const
{
    if (!m_wrapInitialized) {
        lastRaw = rawValue;
        return static_cast<quint32>(rawValue);
    }
    if (rawValue < lastRaw && static_cast<quint16>(lastRaw - rawValue) > 32768U) {
        epoch += 65536U;
    }
    lastRaw = rawValue;
    return epoch + static_cast<quint32>(rawValue);
}

void Ltr114Worker::initializeReferenceIfNeeded(quint32 startMark, quint32 secondMark)
{
    if (!m_syncState || !m_syncState->needSynchronization)
        return;

    QMutexLocker locker(&m_syncState->mutex);
    if (!m_syncState->refInitialized) {
        m_syncState->seen114 = true;
        m_syncState->start114 = startMark;
        m_syncState->second114 = secondMark;

        if (m_syncState->seen114 && m_syncState->seen212) {
            m_syncState->refStartMark = qMax(m_syncState->start114, m_syncState->start212);
            m_syncState->refSecondMark = qMax(m_syncState->second114, m_syncState->second212);
            m_syncState->refInitialized = true;
        }
    }
}

bool Ltr114Worker::shouldAcceptSample(quint32 startMark, quint32 secondMark) const
{
    if (!m_syncState || !m_syncState->needSynchronization)
        return true;

    QMutexLocker locker(&m_syncState->mutex);
    if (!m_syncState->refInitialized)
        return false;

    if (startMark < m_syncState->refStartMark)
        return false;
    if (startMark == m_syncState->refStartMark && secondMark < m_syncState->refSecondMark)
        return false;
    return true;
}

QVector<Ltr114Worker::TimePoint> Ltr114Worker::buildTimeline(const QVector<DWORD>& tmarks)
{
    QVector<TimePoint> timeline;
    if (tmarks.isEmpty())
        return timeline;

    timeline.reserve(tmarks.size());
    for (DWORD tmarkWord : tmarks) {
        const quint32 startMark = unwrapCounter(static_cast<quint16>(extractStartMark(tmarkWord)), m_lastStartRaw, m_startEpoch);
        const quint32 secondMark = unwrapCounter(static_cast<quint16>(extractSecondMark(tmarkWord)), m_lastSecondRaw, m_secondEpoch);
        m_wrapInitialized = true;

        initializeReferenceIfNeeded(startMark, secondMark);
        if (!shouldAcceptSample(startMark, secondMark))
            continue;

        if (timeline.isEmpty()) {
            m_currentSecond = secondMark;
            m_samplesInSecond = 0;
        } else if (secondMark != m_currentSecond) {
            if (m_samplesInSecond > 0) {
                m_estSamplesPerSecond = 0.8 * m_estSamplesPerSecond + 0.2 * static_cast<double>(m_samplesInSecond);
            }
            m_currentSecond = secondMark;
            m_samplesInSecond = 0;
        }

        TimePoint point;
        point.startMark = startMark;
        point.secondMark = secondMark;
        point.sampleInSecond = m_samplesInSecond;

        const double phase = (m_estSamplesPerSecond > 1.0)
                                 ? (static_cast<double>(m_samplesInSecond) / m_estSamplesPerSecond)
                                 : 0.0;
        point.globalTick = static_cast<quint64>(
            (static_cast<double>(secondMark) + phase) * static_cast<double>(m_syncState ? m_syncState->timeBaseTicks : 1000000ULL));
        ++m_samplesInSecond;

        timeline.append(point);
    }
    return timeline;
}

QVector<TimedSample> Ltr114Worker::attachTimeline(const QVector<double>& values, const QVector<TimePoint>& timeline) const
{
    QVector<TimedSample> result;
    const int n = qMin(values.size(), timeline.size());
    result.reserve(n);
    for (int i = 0; i < n; ++i) {
        TimedSample sample;
        sample.globalTick = timeline[i].globalTick;
        sample.startMark = timeline[i].startMark;
        sample.secondMark = timeline[i].secondMark;
        sample.sampleInSecond = timeline[i].sampleInSecond;
        sample.value = values[i];
        result.append(sample);
    }
    return result;
}

Ltr212Worker::Ltr212Worker(LTR212* module, SyncState* syncState, QObject* parent)
    : QObject(parent)
    , m_module(module)
    , m_syncState(syncState)
{
}

void Ltr212Worker::run()
{
    if (!m_module) {
        emit acquisitionError("LTR212 worker: модуль не инициализирован");
        emit finished();
        return;
    }

    m_running.store(true);
    m_wrapInitialized = false;
    m_secondEpoch = m_startEpoch = 0;

    while (m_running.load()) {
        int error = 0;
        auto [rawData, tmarks] = m_module->receive_data_with_marks(kRecvTimeoutMs, &error);

        if (error != 0) {
            if (!m_running.load()) {
                break;
            }
            emit acquisitionError(QString("LTR212: ошибка приёма %1").arg(error));
            break;
        }

        if (rawData.isEmpty()) {
            QThread::msleep(1);
            continue;
        }

        const QVector<TimePoint> timeline = buildTimeline(tmarks);
        if (timeline.isEmpty()) {
            QThread::msleep(1);
            continue;
        }

        const QVector<DWORD> dataToProcess = rawData;
        QVector<double> voltageSamples = m_module->process_data(dataToProcess, true);
        const QVector<TimedSample> timed = remapAndAttachTimeline(voltageSamples, timeline);
        if (!timed.isEmpty()) {
            emit newVoltageSamples(timed);
        }

        QThread::msleep(1);
    }

    m_running.store(false);
    emit finished();
}

void Ltr212Worker::stopAcquisition()
{
    m_running.store(false);
}

quint32 Ltr212Worker::extractStartMark(DWORD tmarkWord)
{
    return (tmarkWord >> 16) & 0xFFFF;
}

quint32 Ltr212Worker::extractSecondMark(DWORD tmarkWord)
{
    return tmarkWord & 0xFFFF;
}

quint32 Ltr212Worker::unwrapCounter(quint16 rawValue, quint16& lastRaw, quint32& epoch) const
{
    if (!m_wrapInitialized) {
        lastRaw = rawValue;
        return static_cast<quint32>(rawValue);
    }
    if (rawValue < lastRaw && static_cast<quint16>(lastRaw - rawValue) > 32768U) {
        epoch += 65536U;
    }
    lastRaw = rawValue;
    return epoch + static_cast<quint32>(rawValue);
}

void Ltr212Worker::initializeReferenceIfNeeded(quint32 startMark, quint32 secondMark)
{
    if (!m_syncState || !m_syncState->needSynchronization)
        return;

    QMutexLocker locker(&m_syncState->mutex);
    if (!m_syncState->refInitialized) {
        m_syncState->seen212 = true;
        m_syncState->start212 = startMark;
        m_syncState->second212 = secondMark;

        if (m_syncState->seen114 && m_syncState->seen212) {
            m_syncState->refStartMark = qMax(m_syncState->start114, m_syncState->start212);
            m_syncState->refSecondMark = qMax(m_syncState->second114, m_syncState->second212);
            m_syncState->refInitialized = true;
        }
    }
}

bool Ltr212Worker::shouldAcceptSample(quint32 startMark, quint32 secondMark) const
{
    if (!m_syncState || !m_syncState->needSynchronization)
        return true;

    QMutexLocker locker(&m_syncState->mutex);
    if (!m_syncState->refInitialized)
        return false;

    if (startMark < m_syncState->refStartMark)
        return false;
    if (startMark == m_syncState->refStartMark && secondMark < m_syncState->refSecondMark)
        return false;
    return true;
}

QVector<Ltr212Worker::TimePoint> Ltr212Worker::buildTimeline(const QVector<DWORD>& tmarks)
{
    QVector<TimePoint> timeline;
    if (tmarks.isEmpty())
        return timeline;

    timeline.reserve(tmarks.size());
    for (DWORD tmarkWord : tmarks) {
        const quint32 startMark = unwrapCounter(static_cast<quint16>(extractStartMark(tmarkWord)), m_lastStartRaw, m_startEpoch);
        const quint32 secondMark = unwrapCounter(static_cast<quint16>(extractSecondMark(tmarkWord)), m_lastSecondRaw, m_secondEpoch);
        m_wrapInitialized = true;

        initializeReferenceIfNeeded(startMark, secondMark);
        if (!shouldAcceptSample(startMark, secondMark))
            continue;

        if (timeline.isEmpty()) {
            m_currentSecond = secondMark;
            m_samplesInSecond = 0;
        } else if (secondMark != m_currentSecond) {
            if (m_samplesInSecond > 0) {
                m_estSamplesPerSecond = 0.8 * m_estSamplesPerSecond + 0.2 * static_cast<double>(m_samplesInSecond);
            }
            m_currentSecond = secondMark;
            m_samplesInSecond = 0;
        }

        TimePoint point;
        point.startMark = startMark;
        point.secondMark = secondMark;
        point.sampleInSecond = m_samplesInSecond;
        const double phase = (m_estSamplesPerSecond > 1.0)
                                 ? (static_cast<double>(m_samplesInSecond) / m_estSamplesPerSecond)
                                 : 0.0;
        point.globalTick = static_cast<quint64>(
            (static_cast<double>(secondMark) + phase) * static_cast<double>(m_syncState ? m_syncState->timeBaseTicks : 1000000ULL));
        ++m_samplesInSecond;
        timeline.append(point);
    }
    return timeline;
}

QVector<TimedSample> Ltr212Worker::remapAndAttachTimeline(const QVector<double>& values, const QVector<TimePoint>& timeline) const
{
    QVector<TimedSample> result;
    if (values.isEmpty() || timeline.isEmpty())
        return result;

    result.reserve(values.size());
    const int timelineSize = timeline.size();
    for (int i = 0; i < values.size(); ++i) {
        const int idx = qMin(i * 2, timelineSize - 1);
        TimedSample sample;
        sample.globalTick = timeline[idx].globalTick;
        sample.startMark = timeline[idx].startMark;
        sample.secondMark = timeline[idx].secondMark;
        sample.sampleInSecond = timeline[idx].sampleInSecond;
        sample.value = values[i];
        result.append(sample);
    }
    return result;
}
