#ifndef MODULE_H
#define MODULE_H

#include <QString>
#include <QVector>
#include <Windows.h>    // для DWORD, WORD и т.д. (зависит от вашего окружения)

class Module
{
public:
    virtual ~Module() = default;

    // Открыть модуль в указанном крейте
    virtual bool open(const QString& crateSn, int slot) = 0;

    // Закрыть модуль
    virtual void close() = 0;

    // Получить текущую конфигурацию модуля (обновить внутренние данные)
    virtual bool get_config() = 0;

    // Применить настройки (например, записать регистры АЦП)
    virtual bool apply_config() = 0;

    // Запустить сбор данных
    virtual bool start() = 0;

    // Остановить сбор данных
    virtual bool stop() = 0;

    // Принять данные от модуля (общий для всех модулей метод)
    // timeout – мс, errorCode – опциональный код ошибки
    virtual QVector<DWORD> receive_data(DWORD timeout, int* errorCode = nullptr) = 0;
};

#endif // MODULE_H
