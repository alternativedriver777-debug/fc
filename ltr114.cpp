#include "ltr114.h"

#include <cstring>

LTR114::LTR114()
    : m_is_open(false)
    , m_slot(-1)
{
    std::memset(&m_handle, 0, sizeof(m_handle));
}

LTR114::~LTR114()
{
    close();
}

bool LTR114::open(const QString& crate_sn, int slot)
{
    if (m_is_open)
        close();

    if (LTR114_Init(&m_handle) != LTR_OK)
        return false;

    QByteArray sn_bytes = crate_sn.toLatin1();
    INT result = LTR114_Open(&m_handle, SADDR_DEFAULT, SPORT_DEFAULT, sn_bytes.constData(), slot);

    if (result == LTR_WARNING_MODULE_IN_USE)
        result = LTR_OK;

    if (result != LTR_OK)
        return false;

    m_slot = slot;
    m_is_open = true;
    return true;
}

void LTR114::close()
{
    if (m_is_open) {
        LTR114_Close(&m_handle);
        m_is_open = false;
    }
}

bool LTR114::get_config()
{
    return m_is_open && (LTR114_GetConfig(&m_handle) == LTR_OK);
}

bool LTR114::apply_config()
{
    if (!m_is_open)
        return false;

    return LTR114_SetADC(&m_handle) == LTR_OK;
}

bool LTR114::start()
{
    return m_is_open && (LTR114_Start(&m_handle) == LTR_OK);
}

bool LTR114::stop()
{
    return m_is_open && (LTR114_Stop(&m_handle) == LTR_OK);
}

QVector<DWORD> LTR114::receive_data(DWORD timeout, int* error_code)
{
    QVector<DWORD> data;

    if (!m_is_open) {
        if (error_code)
            *error_code = -1;
        return data;
    }

    INT requested_size = static_cast<INT>(10 * m_handle.FrameLength);
    if (requested_size <= 0)
        requested_size = 10;

    data.resize(requested_size);

    INT recv_result = LTR114_Recv(&m_handle, data.data(), nullptr, requested_size, timeout);
    if (recv_result < 0) {
        if (error_code)
            *error_code = recv_result;
        data.clear();
        return data;
    }

    data.resize(recv_result);

    if (error_code)
        *error_code = 0;

    return data;
}

void LTR114::set_freq_divider(DWORD freq_divider)
{
    m_handle.FreqDivider = freq_divider;
}

void LTR114::set_logical_channels(int channel_count, const TLTR114_LCHANNEL* channel_table)
{
    if (channel_count < 0)
        channel_count = 0;

    if (channel_count > LTR114_CHANNEL_CNT_MAX)
        channel_count = LTR114_CHANNEL_CNT_MAX;

    m_handle.LChQnt = channel_count;

    for (int i = 0; i < channel_count; ++i)
        m_handle.LChTbl[i] = channel_table[i];
}

void LTR114::set_sync_mode(DWORD sync_mode)
{
    m_handle.SyncMode = sync_mode;
}

void LTR114::set_interval(DWORD interval)
{
    m_handle.Interval = interval;
}
