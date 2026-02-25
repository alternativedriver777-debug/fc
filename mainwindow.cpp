#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QFrame>
#include <QDebug>
#include <numeric>

#include "ltr11.h"
#include "ltr114.h"

QString MainWindow::module_name(WORD mid)
{
    switch (mid)
    {
    case LTR_MID_EMPTY: return "EMPTY";
    case LTR_MID_IDENTIFYING: return "IDENTIFYING";
    case LTR_MID_INVALID: return "INVALID";
    case LTR_MID_LTR01: return "LTR01";
    case LTR_MID_LTR11: return "LTR11";
    case LTR_MID_LTR22: return "LTR22";
    case LTR_MID_LTR24: return "LTR24";
    case LTR_MID_LTR25: return "LTR25";
    case LTR_MID_LTR27: return "LTR27";
    case LTR_MID_LTR34: return "LTR34";
    case LTR_MID_LTR35: return "LTR35";
    case LTR_MID_LTR41: return "LTR41";
    case LTR_MID_LTR42: return "LTR42";
    case LTR_MID_LTR43: return "LTR43";
    case LTR_MID_LTR51: return "LTR51";
    case LTR_MID_LTR114: return "LTR114";
    case LTR_MID_LTR210: return "LTR210";
    case LTR_MID_LTR212: return "LTR212";
    default: return QString("Unknown (%1, 0x%2)").arg(mid).arg(QString::number(mid, 16).toUpper());
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    init_ui_replace();
    appendInfo("Приложение запущено.");

    init_ltr();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::init_ui_replace()
{
    QWidget* central = ui->centralwidget ? ui->centralwidget : new QWidget(this);
    setCentralWidget(central);

    QHBoxLayout* main_lay = new QHBoxLayout;
    central->setLayout(main_lay);

    modulesList = new QListWidget(this);
    modulesList->setMinimumWidth(320);
    modulesList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    modulesList->setSelectionMode(QAbstractItemView::NoSelection);
    modulesList->setFocusPolicy(Qt::NoFocus);

    infoText = new QTextEdit(this);
    infoText->setReadOnly(true);
    infoText->setAcceptRichText(false);
    QFont f = infoText->font();
    f.setFamily("Monospace");
    f.setStyleHint(QFont::Monospace);
    infoText->setFont(f);

    main_lay->addWidget(modulesList, 0);
    main_lay->addWidget(infoText, 1);

    appendInfo("Информационный виджет создан.");
}

void MainWindow::appendInfo(const QString &msg, bool isError)
{
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString line = QString("[%1] %2").arg(time, msg);
    if (isError) {
        infoText->setTextColor(Qt::red);
        infoText->append(line);
        infoText->setTextColor(Qt::black);
    } else {
        infoText->append(line);
    }

    qDebug() << line;
}

QWidget* MainWindow::createModuleItemWidget(int slot, const QString &name, bool ok)
{
    QWidget* w = new QWidget;
    QHBoxLayout* lay = new QHBoxLayout(w);
    lay->setContentsMargins(6, 3, 6, 3);

    QLabel* slot_label = new QLabel(QString("Слот %1").arg(slot));
    slot_label->setMinimumWidth(70);
    QLabel* name_label = new QLabel(name);
    name_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QLabel* indicator = new QLabel;
    indicator->setFixedSize(14, 14);
    indicator->setObjectName(QString("indicator_%1").arg(slot));
    indicator->setStyleSheet(QString("border-radius:7px; background-color: %1;")
                                 .arg(ok ? "green" : "red"));

    lay->addWidget(slot_label);
    lay->addWidget(name_label, 1);
    lay->addWidget(indicator);
    return w;
}

void MainWindow::setModuleStatus(int slot, bool ok)
{
    QWidget* w = moduleWidgets.value(slot, nullptr);
    if (!w) return;
    QLabel* ind = w->findChild<QLabel*>(QString("indicator_%1").arg(slot));
    if (ind) {
        ind->setStyleSheet(QString("border-radius:7px; background-color: %1;")
                               .arg(ok ? "green" : "red"));
    }
}

void MainWindow::run_ltr11_module(const QString& crate_sn, int ltr11_slot)
{
    m_ltr11 = std::make_unique<LTR11>();
    appendInfo(QString("Открываем LTR11 в слоте %1...").arg(ltr11_slot));
    if (!m_ltr11->open(crate_sn, ltr11_slot)) {
        appendInfo(QString("Не удалось открыть LTR11 в слоте %1").arg(ltr11_slot), true);
        m_ltr11.reset();
        setModuleStatus(ltr11_slot, false);
        return;
    }

    appendInfo("LTR11 успешно открыт.");
    setModuleStatus(ltr11_slot, true);

    if (!m_ltr11->get_config()) {
        appendInfo("Не удалось получить конфигурацию LTR11", true);
        return;
    }
    appendInfo(QString("LTR11: %1  SN: %2").arg(m_ltr11->module_name(), m_ltr11->module_serial()));

    m_ltr11->set_start_mode(LTR11_STARTADCMODE_INT);
    m_ltr11->set_input_mode(LTR11_INPMODE_INT);
    BYTE ch_tbl[] = { static_cast<BYTE>((0 << 6) | (0 << 4) | (0 << 0)) };
    m_ltr11->set_logical_channels(1, ch_tbl);
    m_ltr11->set_ADC_rate(1, 149);

    if (!m_ltr11->apply_config()) {
        appendInfo("Не удалось применить настройки АЦП LTR11", true);
        return;
    }
    appendInfo(QString("LTR11: параметры АЦП установлены, частота канала: %1 кГц").arg(m_ltr11->channel_rate()));

    if (!m_ltr11->start()) {
        appendInfo("LTR11: не удалось запустить сбор данных", true);
        return;
    }
    appendInfo("LTR11: сбор данных запущен.");

    int error = 0;
    QVector<DWORD> data = m_ltr11->receive_data(3000, &error);
    if (error != 0) {
        appendInfo(QString("LTR11: ошибка приёма данных: %1").arg(error), true);
        setModuleStatus(ltr11_slot, false);
    } else {
        appendInfo(QString("LTR11: принято слов: %1").arg(data.size()));
        TLTR11* hmodule = m_ltr11->handle();

        QVector<double> voltages(data.size());
        int data_size = data.size();

        if (data.isEmpty()) {
            appendInfo("LTR11: нет данных для обработки", true);
        } else {
            INT proc_result = LTR11_ProcessData(
                hmodule,
                data.data(),
                voltages.data(),
                &data_size,
                TRUE,
                TRUE
                );

            if (proc_result != LTR_OK) {
                appendInfo(QString("LTR11: ошибка обработки данных: %1").arg(proc_result), true);
            } else {
                appendInfo(QString("LTR11: обработано данных: %1 значений").arg(data_size));
            }
        }
    }

    if (m_ltr11) {
        m_ltr11->stop();
        m_ltr11->close();
        appendInfo("LTR11: сбор остановлен, модуль закрыт.");
    }
    m_ltr11.reset();
}

void MainWindow::run_ltr114_module(const QString& crate_sn, int ltr114_slot)
{
    m_ltr114 = std::make_unique<LTR114>();
    appendInfo(QString("Открываем LTR114 в слоте %1...").arg(ltr114_slot));

    if (!m_ltr114->open(crate_sn, ltr114_slot)) {
        appendInfo(QString("Не удалось открыть LTR114 в слоте %1").arg(ltr114_slot), true);
        m_ltr114.reset();
        setModuleStatus(ltr114_slot, false);
        return;
    }

    appendInfo("LTR114 успешно открыт.");
    setModuleStatus(ltr114_slot, true);

    if (!m_ltr114->get_config()) {
        appendInfo("Не удалось получить конфигурацию LTR114", true);
        return;
    }

    appendInfo(QString("LTR114: %1  SN: %2").arg(m_ltr114->module_name(), m_ltr114->module_serial()));

    DWORD lch_tbl[1];
    lch_tbl[0] = LTR114_CreateLChannel(LTR114_MEASMODE_U, 0, LTR114_URANGE_04);

    m_ltr114->set_freq_divider(4);
    m_ltr114->set_logical_channels(1, lch_tbl);
    m_ltr114->set_sync_mode(LTR114_SYNCMODE_INTERNAL);
    m_ltr114->set_interval(0);

    if (!m_ltr114->apply_config()) {
        appendInfo("Не удалось применить настройки АЦП LTR114", true);
        return;
    }

    if (LTR114_Calibrate(m_ltr114->handle()) != LTR_OK) {
        appendInfo("LTR114: ошибка начальной автокалибровки", true);
        return;
    }
    appendInfo("LTR114: начальная автокалибровка выполнена.");

    if (!m_ltr114->start()) {
        appendInfo("LTR114: не удалось запустить сбор данных", true);
        return;
    }
    appendInfo("LTR114: сбор данных запущен.");

    int error = 0;
    QVector<DWORD> data = m_ltr114->receive_data(3000, &error);
    if (error != 0) {
        appendInfo(QString("LTR114: ошибка приёма данных: %1").arg(error), true);
        setModuleStatus(ltr114_slot, false);
    } else if (data.isEmpty()) {
        appendInfo("LTR114: не приняты данные", true);
    } else {
        appendInfo(QString("LTR114: принято слов: %1").arg(data.size()));

        TLTR114* hmodule = m_ltr114->handle();
        QVector<double> proc_data(data.size());
        QVector<double> therm_data(data.size());
        INT proc_size = data.size();
        INT therm_count = 0;

        INT proc_result = LTR114_ProcessDataTherm(
            hmodule,
            data.data(),
            proc_data.data(),
            therm_data.data(),
            &proc_size,
            &therm_count,
            LTR114_CORRECTION_MODE_INIT,
            LTR114_PROCF_VALUE
            );

        if (proc_result != LTR_OK) {
            appendInfo(QString("LTR114: ошибка обработки данных: %1").arg(proc_result), true);
        } else {
            appendInfo(QString("LTR114: обработано %1 значений АЦП").arg(proc_size));
            appendInfo(QString("LTR114: получено %1 значений термопары").arg(therm_count));

            if (therm_count > 0) {
                QString therm_values;
                int show_count = qMin(therm_count, 20);
                for (int i = 0; i < show_count; ++i) {
                    therm_values += QString("%1 В").arg(QString::number(therm_data[i], 'f', 6));
                    if (i + 1 < show_count)
                        therm_values += ", ";
                }

                appendInfo("LTR114: первые значения термопары (напряжение):");
                appendInfo(therm_values);

                double min_v = *std::min_element(therm_data.begin(), therm_data.begin() + therm_count);
                double max_v = *std::max_element(therm_data.begin(), therm_data.begin() + therm_count);
                double sum_v = std::accumulate(therm_data.begin(), therm_data.begin() + therm_count, 0.0);
                double avg_v = sum_v / therm_count;

                appendInfo(QString("LTR114 термопара: min=%1 В, max=%2 В, avg=%3 В, размах=%4 В")
                               .arg(QString::number(min_v, 'f', 6))
                               .arg(QString::number(max_v, 'f', 6))
                               .arg(QString::number(avg_v, 'f', 6))
                               .arg(QString::number(max_v - min_v, 'f', 6)));
            }
        }
    }

    if (m_ltr114) {
        m_ltr114->stop();
        m_ltr114->close();
        appendInfo("LTR114: сбор остановлен, модуль закрыт.");
    }
    m_ltr114.reset();
}

void MainWindow::init_ltr()
{
    appendInfo("Начало поиска крейтов...");

    auto crates = Crate::enumerate_crates();
    if (crates.isEmpty()) {
        appendInfo("Нет подключенных крейтов", true);
        return;
    }

    appendInfo(QString("Найдено %1 крейт(ов):").arg(crates.size()));
    for (const auto& s : crates) appendInfo(QString("  %1").arg(s));

    QString crate_sn = crates.first();
    appendInfo(QString("Открываем крейт SN: %1").arg(crate_sn));

    m_crate = std::make_unique<Crate>(crate_sn);
    if (!m_crate->is_open()) {
        appendInfo("Не удалось открыть крейт", true);
        m_crate.reset();
        return;
    }
    appendInfo("Крейт успешно открыт.");

    auto modules = m_crate->get_modules();
    if (modules.isEmpty()) {
        appendInfo("В крейте нет модулей", true);
        return;
    }

    WORD slot_count = m_crate->get_slot_count();
    appendInfo(QString("Вместимость крейта: %1 слотов").arg(slot_count));

    for (int s = 1; s <= slot_count; ++s) {
        QWidget* w = createModuleItemWidget(s, "EMPTY", false);
        QListWidgetItem* it = new QListWidgetItem(modulesList);
        it->setSizeHint(w->sizeHint());
        modulesList->addItem(it);
        modulesList->setItemWidget(it, w);
        moduleWidgets.insert(s, w);
    }

    appendInfo("Список модулей в крейте:");
    for (const auto& mod : modules) {
        int slot = mod.first;
        WORD mid = mod.second;
        QString name = module_name(mid);
        appendInfo(QString("  слот %1: %2").arg(slot).arg(name));

        QWidget* w = moduleWidgets.value(slot, nullptr);
        if (w) {
            QWidget* new_w = createModuleItemWidget(slot, name, true);
            QListWidgetItem* item = modulesList->item(slot - 1);
            if (item) {
                modulesList->setItemWidget(item, new_w);
                moduleWidgets[slot] = new_w;
            }
        }
    }

    int ltr11_slot = -1;
    int ltr114_slot = -1;

    for (const auto& mod : modules) {
        if (mod.second == LTR_MID_LTR11 && ltr11_slot == -1)
            ltr11_slot = mod.first;
        if (mod.second == LTR_MID_LTR114 && ltr114_slot == -1)
            ltr114_slot = mod.first;
    }

    if (ltr11_slot != -1) {
        appendInfo(QString("Модуль LTR11 найден в слоте %1").arg(ltr11_slot));
        run_ltr11_module(crate_sn, ltr11_slot);
    } else {
        appendInfo("Модуль LTR11 не найден", true);
    }

    if (ltr114_slot != -1) {
        appendInfo(QString("Модуль LTR114 найден в слоте %1").arg(ltr114_slot));
        run_ltr114_module(crate_sn, ltr114_slot);
    } else {
        appendInfo("Модуль LTR114 не найден", true);
    }

    m_crate.reset();
    appendInfo("Соединения с крейтом закрыты. Работа завершена.");
}
