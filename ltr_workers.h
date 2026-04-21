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
    bool seen114 = false;
    bool seen212 = false;
    quint32 start114 = 0;
    quint32 start212 = 0;
};

class Ltr114Worker : public QObject
{
    Q_OBJECT
public:
    explicit Ltr114Worker(LTR114* module, SyncState* syncState, QObject* parent = nullptr);

public slots:
    void run();
    void stopAcquisition();

signals:
    void newVoltageSamples(const QVector<double>& voltageSamples);
    void acquisitionError(const QString& message);
    void finished();

private:
    QVector<DWORD> trimByStartMark(const QVector<DWORD>& raw, const QVector<DWORD>& tmarks);
    static quint32 extractStartMark(DWORD tmarkWord);

    LTR114* m_module;
    SyncState* m_syncState;
    std::atomic_bool m_running{false};
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
    void newVoltageSamples(const QVector<double>& voltageSamples);
    void acquisitionError(const QString& message);
    void finished();

private:
    QVector<DWORD> trimByStartMark(const QVector<DWORD>& raw, const QVector<DWORD>& tmarks);
    static quint32 extractStartMark(DWORD tmarkWord);

    LTR212* m_module;
    SyncState* m_syncState;
    std::atomic_bool m_running{false};
};


#endif // LTR_WORKERS_H
