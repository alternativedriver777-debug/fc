#ifndef MODULE_H
#define MODULE_H

#include <QString>
#include <QVector>
#include <Windows.h>

class Module
{
public:
    virtual ~Module() = default;
    virtual bool open(const QString& crateSn, int slot) = 0;
    virtual void close() = 0;
    virtual bool get_config() = 0;
    virtual bool apply_config() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual QVector<DWORD> receive_data(DWORD timeout, int* errorCode = nullptr) = 0;
};

#endif // MODULE_H
