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

    while (m_running.load()) {
        int error = 0;
        auto [rawData, tmarks] = m_module->receive_data_with_marks(kRecvTimeoutMs, &error);

        if (error != 0) {
            emit acquisitionError(QString("LTR114: ошибка приёма %1").arg(error));
            break;
        }

        if (rawData.isEmpty()) {
            QThread::msleep(1);
            continue;
        }

        const QVector<DWORD> dataToProcess = trimByStartMark(rawData, tmarks);
        if (dataToProcess.isEmpty()) {
            QThread::msleep(1);
            continue;
        }

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
            emit acquisitionError(QString("LTR114: ошибка обработки %1").arg(result));
            break;
        }

        procData.resize(procSize);
        if (!procData.isEmpty()) {
            emit newVoltageSamples(procData);
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

QVector<DWORD> Ltr114Worker::trimByStartMark(const QVector<DWORD>& raw, const QVector<DWORD>& tmarks)
{
    if (!m_syncState || !m_syncState->needSynchronization) {
        return raw;
    }

    if (tmarks.isEmpty() || raw.size() != tmarks.size()) {
        return raw;
    }

    quint32 refStart = 0;
    {
        QMutexLocker locker(&m_syncState->mutex);
        const quint32 currentStart = extractStartMark(tmarks.first());

        if (!m_syncState->refInitialized) {
            m_syncState->seen114 = true;
            m_syncState->start114 = currentStart;

            if (m_syncState->seen114 && m_syncState->seen212) {
                m_syncState->refStartMark = qMax(m_syncState->start114, m_syncState->start212);
                m_syncState->refInitialized = true;
            }
        }

        if (!m_syncState->refInitialized) {
            return {};
        }

        refStart = m_syncState->refStartMark;
    }

    for (int i = 0; i < tmarks.size(); ++i) {
        if (extractStartMark(tmarks[i]) >= refStart) {
            return raw.mid(i);
        }
    }

    return {};
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

    while (m_running.load()) {
        int error = 0;
        auto [rawData, tmarks] = m_module->receive_data_with_marks(kRecvTimeoutMs, &error);

        if (error != 0) {
            emit acquisitionError(QString("LTR212: ошибка приёма %1").arg(error));
            break;
        }

        if (rawData.isEmpty()) {
            QThread::msleep(1);
            continue;
        }

        const QVector<DWORD> dataToProcess = trimByStartMark(rawData, tmarks);
        if (dataToProcess.isEmpty()) {
            QThread::msleep(1);
            continue;
        }

        QVector<double> voltageSamples = m_module->process_data(dataToProcess, true);
        if (!voltageSamples.isEmpty()) {
            emit newVoltageSamples(voltageSamples);
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

QVector<DWORD> Ltr212Worker::trimByStartMark(const QVector<DWORD>& raw, const QVector<DWORD>& tmarks)
{
    if (!m_syncState || !m_syncState->needSynchronization) {
        return raw;
    }

    if (tmarks.isEmpty() || raw.size() != tmarks.size()) {
        return raw;
    }

    quint32 refStart = 0;
    {
        QMutexLocker locker(&m_syncState->mutex);
        const quint32 currentStart = extractStartMark(tmarks.first());

        if (!m_syncState->refInitialized) {
            m_syncState->seen212 = true;
            m_syncState->start212 = currentStart;

            if (m_syncState->seen114 && m_syncState->seen212) {
                m_syncState->refStartMark = qMax(m_syncState->start114, m_syncState->start212);
                m_syncState->refInitialized = true;
            }
        }

        if (!m_syncState->refInitialized) {
            return {};
        }

        refStart = m_syncState->refStartMark;
    }

    for (int i = 0; i < tmarks.size(); ++i) {
        if (extractStartMark(tmarks[i]) >= refStart) {
            return raw.mid(i);
        }
    }

    return {};
}


