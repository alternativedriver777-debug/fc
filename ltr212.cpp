#include "ltr212.h"
#include <cstring>
#include <QDebug>

LTR212::LTR212()
    : m_is_open(false)
    , m_slot(-1)
{
    std::memset(&m_handle, 0, sizeof(m_handle));
}

LTR212::~LTR212()
{
    close();
}

bool LTR212::open(const QString& crate_sn, int slot)
{
    return open(crate_sn, slot, "ltr212.bio");   // вызов полной версии
}

bool LTR212::open(const QString& crate_sn, int slot, const QString& bios)
{
    if (m_is_open) close();

    if (LTR212_Init(&m_handle) != LTR_OK)
        return false;

    QByteArray sn_bytes = crate_sn.toLatin1();
    INT result = LTR212_Open(&m_handle, SADDR_DEFAULT, SPORT_DEFAULT,
                             sn_bytes.constData(), slot, bios.toLatin1().constData());

    if (result == LTR_WARNING_MODULE_IN_USE)
        result = LTR_OK;

    if (result != LTR_OK)
        return false;

    m_slot = slot;
    m_is_open = true;

    set_size();
    return true;
}

void LTR212::close()
{
    if (m_is_open) {
        LTR212_Close(&m_handle);
        m_is_open = false;
    }
}

bool LTR212::apply_config()
{
    if (!m_is_open)
        return false;

    return LTR212_SetADC(&m_handle) == LTR_OK;   // или LTR212_SetConfig — проверь в ltr212api.h
}

bool LTR212::start()
{
    return m_is_open && (LTR212_Start(&m_handle) == LTR_OK);
}

bool LTR212::stop()
{
    return m_is_open && (LTR212_Stop(&m_handle) == LTR_OK);
}

QVector<DWORD> LTR212::receive_data(DWORD timeout, int* error_code)
{
    QVector<DWORD> data;

    if (!m_is_open) {
        if (error_code)
            *error_code = -1;
        return data;
    }

    // Для LTR212 размер буфера можно позже сделать настраиваемым
    INT requested_size = 4096;
    data.resize(requested_size);

    INT recv_result = LTR212_Recv(&m_handle, data.data(), nullptr, requested_size, timeout);
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

void LTR212::set_acq_mode(INT mode)
{
    m_handle.AcqMode = mode;
}

void LTR212::set_use_clb(INT use)
{
    m_handle.UseClb = use;
}

void LTR212::set_use_fabric_clb(INT use)
{
    m_handle.UseFabricClb = use;
}

void LTR212::set_ref_voltage(INT ref)
{
    m_handle.REF = ref;
}

void LTR212::set_ac_mode(INT ac)
{
    m_handle.AC = ac;
}

void LTR212::set_logical_channels(int count, const INT* channel_table)
{
    if (count < 0) count = 0;
    if (count > 8) count = 8; // максимум 8 для LTR212

    m_handle.LChQnt = count;
    for (int i = 0; i < count; ++i)
        m_handle.LChTbl[i] = channel_table[i];
}

void LTR212::set_size()
{
    m_handle.size = sizeof(TLTR212);
}

QVector<double> LTR212::process_data(const QVector<DWORD>& src, bool to_volts)
{
    if (!m_is_open || src.isEmpty()) {
        return {};
    }

    // Резервируем место (по описанию из мануала размер примерно делится пополам)
    QVector<double> dest(src.size() / 2 + 64);
    DWORD proc_size = static_cast<DWORD>(src.size());

    INT err = LTR212_ProcessData(&m_handle,
                                 const_cast<DWORD*>(src.data()),
                                 dest.data(),
                                 &proc_size,
                                 to_volts ? 1 : 0);

    if (err != LTR_OK) {
        qDebug() << "LTR212_ProcessData error:" << err;
        // Если есть LTR212_GetErrorString(err) — можно вывести строку ошибки
    }

    dest.resize(static_cast<int>(proc_size));
    return dest;
}

QPair<QVector<DWORD>, QVector<DWORD>> LTR212::receive_data_with_marks(DWORD timeout, int* error_code)
{
    QVector<DWORD> data;
    QVector<DWORD> tmark;

    if (!m_is_open) {
        if (error_code) *error_code = -1;
        return {data, tmark};
    }

    INT requested_size = 4096;
    data.resize(requested_size);
    tmark.resize(requested_size);

    INT recv_result = LTR212_Recv(&m_handle, data.data(), tmark.data(), requested_size, timeout);

    if (recv_result < 0) {
        if (error_code) *error_code = recv_result;
        return {{}, {}};
    }

    data.resize(recv_result);
    tmark.resize(recv_result);

    if (error_code) *error_code = 0;
    return {data, tmark};
}

bool LTR212::get_config()
{
    return m_is_open;   // get_config нет для ltr212
}
