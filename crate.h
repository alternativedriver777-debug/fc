#ifndef CRATE_H
#define CRATE_H

#include <QString>
#include <QList>
#include <QPair>
#include <memory>
#include "LTR/ltrapi.h"

class Module;

class Crate
{
public:
    static QList<QString> enumerate_crates();

    explicit Crate(const QString& serial_number);
    ~Crate();

    // запрет копирования
    Crate(const Crate&) = delete;
    Crate& operator=(const Crate&) = delete;

    bool is_open() const { return m_hcrate != nullptr; }

    QString serial_number() const { return m_serial_number; }

    QList<QPair<int, WORD>> get_modules() const;

    std::unique_ptr<Module> create_module(int slot) const;

    WORD get_slot_count() const;

    bool setup_sync_marks();
    void stop_sync_marks();

private:
    QString m_serial_number;
    TLTR* m_hcrate;   // дескриптор управляющего соединения с крейтом
};

#endif // CRATE_H
