#ifndef LTR114_H
#define LTR114_H

#include "module.h"
#include "LTR/ltr114api.h"

class LTR114 : public Module
{
public:
    LTR114();
    ~LTR114() override;

    bool open(const QString& crate_sn, int slot) override;
    void close() override;
    bool get_config() override;
    bool apply_config() override;
    bool start() override;
    bool stop() override;
    QVector<DWORD> receive_data(DWORD timeout, int* error_code = nullptr) override;

    void set_freq_divider(DWORD freq_divider);
    void set_logical_channels(int channel_count, const TLTR114_LCHANNEL* channel_table);
    void set_sync_mode(DWORD sync_mode);
    void set_interval(DWORD interval);

    // === НОВОЕ: приём с синхрометками ===
    QPair<QVector<DWORD>, QVector<DWORD>> receive_data_with_marks(
        DWORD timeout, int* error_code = nullptr);

    QString module_name() const { return QString::fromLatin1(m_handle.ModuleInfo.Name); }
    QString module_serial() const { return QString::fromLatin1(m_handle.ModuleInfo.Serial); }

    TLTR114* handle() { return &m_handle; }

private:
    TLTR114 m_handle;
    bool m_is_open;
    int m_slot;
};

#endif // LTR114_H
