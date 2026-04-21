#ifndef LTR11_H
#define LTR11_H

#include "module.h"
#include "LTR/ltr11api.h"

class LTR11 : public Module
{
public:
    LTR11();
    ~LTR11() override;

    bool open(const QString& crateSn, int slot) override;
    void close() override;
    bool get_config() override;
    bool apply_config() override;
    bool start() override;
    bool stop() override;
    QVector<DWORD> receive_data(DWORD timeout, int* errorCode = nullptr) override;

    void set_start_mode(BYTE mode) { m_handle.StartADCMode = mode; }
    void set_input_mode(BYTE mode) { m_handle.InpMode = mode; }
    void set_ADC_rate(BYTE prescaler, BYTE divider);
    void set_logical_channels(int count, const BYTE* channelTable);

    // геттеры для получения информации о модуле
    QString module_name() const { return QString::fromLatin1(m_handle.ModuleInfo.Name); }
    QString module_serial() const { return QString::fromLatin1(m_handle.ModuleInfo.Serial); }
    int firmware_version() const { return m_handle.ModuleInfo.Ver; }
    double channel_rate() const { return m_handle.ChRate; }

    TLTR11* handle() { return &m_handle; }

private:
    TLTR11 m_handle;
    bool m_is_open;
    QString m_crateSn;
    int m_slot;
};

#endif // LTR11_H
