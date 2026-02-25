#include "ltr11.h"
#include "LTR/ltrapi.h"   // для LTR_Recv
#include <QDebug>
#include <cstring>

LTR11::LTR11()
    : m_is_open(false)
    , m_slot(-1)
{
    std::memset(&m_handle, 0, sizeof(m_handle));
}

LTR11::~LTR11()
{
    close();
}

bool LTR11::open(const QString& crateSn, int slot)
{
    if (m_is_open) close();

    if (LTR11_Init(&m_handle) != LTR_OK)
        return false;

    QByteArray snBytes = crateSn.toLatin1();
    if (LTR11_Open(&m_handle, SADDR_DEFAULT, SPORT_DEFAULT, snBytes.constData(), slot) != LTR_OK)
        return false;

    m_crateSn = crateSn;
    m_slot = slot;
    m_is_open = true;
    return true;
}

void LTR11::close()
{
    if (m_is_open) {
        LTR11_Close(&m_handle);
        m_is_open = false;
    }
}

bool LTR11::get_config()
{
    return m_is_open && (LTR11_GetConfig(&m_handle) == LTR_OK);
}

bool LTR11::apply_config()
{
    if (!m_is_open) return false;
    // Здесь можно применить различные настройки, но для простоты используем LTR11_SetADC
    int rc = LTR11_SetADC(&m_handle);
    if (rc != LTR_OK) {
        qDebug() << "LTR11_SetADC failed:" << rc;
        return false;
    }
    return true;
}



bool LTR11::start()
{
    return m_is_open && (LTR11_Start(&m_handle) == LTR_OK);
}

bool LTR11::stop()
{
    return m_is_open && (LTR11_Stop(&m_handle) == LTR_OK);
}

QVector<DWORD> LTR11::receive_data(DWORD timeout, int* errorCode)
{
    QVector<DWORD> data;
    if (!m_is_open) {
        if (errorCode) *errorCode = -1;
        return data;
    }

    const DWORD data_size = 1024;
    DWORD buffer[data_size];
    INT received = LTR_Recv(&m_handle.Channel, buffer, nullptr, data_size, timeout);
    if (received < 0) {
        if (errorCode) *errorCode = received;
        return data;
    }

    data.resize(received);
    std::memcpy(data.data(), buffer, received * sizeof(DWORD));
    if (errorCode) *errorCode = 0;
    return data;
}

void LTR11::set_ADC_rate(BYTE prescaler, BYTE divider)
{
    m_handle.ADCRate.prescaler = prescaler;
    m_handle.ADCRate.divider = divider;
}

void LTR11::set_logical_channels(int count, const BYTE* channelTable)
{
    if (count > LTR11_MAX_CHANNEL) count = LTR11_MAX_CHANNEL;
    m_handle.LChQnt = count;
    for (int i = 0; i < count; ++i)
        m_handle.LChTbl[i] = channelTable[i];
}
