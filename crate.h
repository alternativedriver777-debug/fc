#ifndef CRATE_H
#define CRATE_H

#include <QString>
#include <QList>
#include <QPair>
#include <memory>
#include "LTR/ltrapi.h"   // для TLTR, WORD и констант

class Module;

class Crate
{
public:
    // стат метод – получить список серийных номеров доступных крейтов
    static QList<QString> enumerate_crates();

    explicit Crate(const QString& serial_number);
    ~Crate();

    // запрет копирования
    Crate(const Crate&) = delete;
    Crate& operator=(const Crate&) = delete;

    bool is_open() const { return m_hcrate != nullptr; }

    QString serial_number() const { return m_serial_number; }

    QList<QPair<int, WORD>> get_modules() const;

    // создать объект модуля для указанного слота (по идентификатору)
    // ретурн nullptr, если тип модуля неизвестен или не удалось открыть
    std::unique_ptr<Module> create_module(int slot) const;

    WORD get_slot_count() const;

private:
    QString m_serial_number;
    TLTR* m_hcrate;   // дескриптор управляющего соединения с крейтом
};

#endif // CRATE_H
