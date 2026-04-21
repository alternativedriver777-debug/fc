#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QFrame>
#include <QDebug>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QFile>
#include <QTextStream>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <numeric>
#include <algorithm>
#include <cmath>

#include "ltr11.h"
#include "ltr114.h"

QString MainWindow::module_name(WORD mid)
{
    switch (mid)
    {
    case LTR_MID_EMPTY: return "EMPTY";
    case LTR_MID_IDENTIFYING: return "IDENTIFYING";
    // case LTR_MID_INVALID: return "INVALID";
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
    , startButton(nullptr)
    , stopButton(nullptr)
    , sampleRateSpin(nullptr)
    , plotEverySpin(nullptr)
    , chunkSizeSpin(nullptr)
    , saveToFileCheck(nullptr)
    , unitCombo(nullptr)
    , chartView(nullptr)
    , chart(nullptr)
    , lineSeries(nullptr)
    , axisX(nullptr)
    , axisY(nullptr)
    , m_ltr114Slot(-1)
    , m_ltr212Slot(-1)
    , m_captureRunning(false)
    , m_tickCounter(0)
    , m_captureFile(nullptr)
    , m_captureStream(nullptr)
    , m_simulationMode(false)
    , m_simulatedSampleAccumulator(0.0)
{
    ui->setupUi(this);

    init_ui_replace();
    setup_plot();
    appendInfo("Приложение запущено.");

    connect(&acquisitionTimer, &QTimer::timeout, this, &MainWindow::poll_capture_data);
    init_ltr();
}

MainWindow::~MainWindow()
{
    close_capture_file();
    close_ltr114_capture();
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

    QWidget* rightPanel = new QWidget(this);
    QVBoxLayout* rightLay = new QVBoxLayout(rightPanel);

    QHBoxLayout* controlLay = new QHBoxLayout;
    sampleRateSpin = new QSpinBox(this);
    sampleRateSpin->setRange(1, 4000);
    sampleRateSpin->setValue(2000);
    sampleRateSpin->setSuffix(" Гц");
    sampleRateSpin->setPrefix("Частота: ");

    plotEverySpin = new QSpinBox(this);
    plotEverySpin->setRange(1, 10000);
    plotEverySpin->setValue(50);
    plotEverySpin->setPrefix("График каждый N тик: ");

    chunkSizeSpin = new QSpinBox(this);
    chunkSizeSpin->setRange(16, 20000);
    chunkSizeSpin->setValue(3000);
    chunkSizeSpin->setPrefix("Chunk слов: ");

    startButton = new QPushButton("Старт", this);
    stopButton = new QPushButton("Остановить", this);
    stopButton->setEnabled(false);

    controlLay->addWidget(sampleRateSpin);
    controlLay->addWidget(plotEverySpin);
    controlLay->addWidget(chunkSizeSpin);
    controlLay->addWidget(startButton);
    controlLay->addWidget(stopButton);

    saveToFileCheck = new QCheckBox("Сохранять файл", this);
    saveToFileCheck->setChecked(true);
    controlLay->addWidget(saveToFileCheck);

    unitCombo = new QComboBox(this);
    unitCombo->addItem("mV", "mV");
    unitCombo->addItem("V", "V");
    controlLay->addWidget(new QLabel("Единицы:", this));
    controlLay->addWidget(unitCombo);

    rightLay->addLayout(controlLay);

    infoText = new QTextEdit(this);
    infoText->setReadOnly(true);
    infoText->setAcceptRichText(false);
    QFont f = infoText->font();
    f.setFamily("Monospace");
    f.setStyleHint(QFont::Monospace);
    infoText->setFont(f);

    rightLay->addWidget(infoText, 1);

    main_lay->addWidget(modulesList, 0);
    main_lay->addWidget(rightPanel, 1);

    connect(startButton, &QPushButton::clicked, this, &MainWindow::on_start_capture_clicked);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::on_stop_capture_clicked);
    connect(unitCombo, &QComboBox::currentTextChanged, this, [this](const QString&) {
        axisY->setTitleText(QString("Напряжение, %1").arg(current_unit_name()));
        axisY->setLabelFormat(current_unit_name() == "V" ? "%.6f" : "%.3f");
        refresh_plot();
    });

    appendInfo("Информационный виджет создан.");
}

void MainWindow::setup_plot()
{
    lineSeries = new QLineSeries(this);

    chart = new QChart();
    chart->addSeries(lineSeries);
    chart->legend()->hide();
    chart->setTitle("LTR: Тики / Напряжение");

    axisX = new QValueAxis();
    axisX->setTitleText("Тики");
    axisX->setLabelFormat("%i");
    axisX->setRange(0, 100);

    axisY = new QValueAxis();
    axisY->setTitleText("Напряжение, mV");
    axisY->setLabelFormat("%.3f");
    axisY->setRange(-100.0, 100.0);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    lineSeries->attachAxis(axisX);
    lineSeries->attachAxis(axisY);

    chartView = new QChartView(chart, this);
    chartView->setMinimumHeight(280);
    chartView->setRenderHint(QPainter::Antialiasing);

    auto* rightLay = qobject_cast<QVBoxLayout*>(qobject_cast<QWidget*>(infoText->parentWidget())->layout());
    if (rightLay) {
        rightLay->insertWidget(1, chartView, 2);
    }
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
    Q_UNUSED(crate_sn)
    Q_UNUSED(ltr11_slot)
}

void MainWindow::run_ltr114_module(const QString& crate_sn, int ltr114_slot)
{
    m_crateSerial = crate_sn;
    m_ltr114Slot = ltr114_slot;
    appendInfo(QString("LTR114 найден в слоте %1. Готов к непрерывному сбору.").arg(ltr114_slot));
}

bool MainWindow::open_ltr114_for_capture()
{
    if (m_simulationMode) {
        m_simulatedSampleAccumulator = 0.0;
        appendInfo(QString("Сбор запущен в режиме симуляции. Частота=%1 Гц").arg(sampleRateSpin->value()));
        return true;
    }

    if (m_crateSerial.isEmpty() || m_ltr114Slot < 0) {
        appendInfo("Не найден LTR114: невозможно запустить сбор.", true);
        return false;
    }

    m_ltr114 = std::make_unique<LTR114>();

    if (!m_ltr114->open(m_crateSerial, m_ltr114Slot)) {
        appendInfo("Не удалось открыть LTR114 для сбора.", true);
        m_ltr114.reset();
        setModuleStatus(m_ltr114Slot, false);
        return false;
    }

    if (!m_ltr114->get_config()) {
        appendInfo("Не удалось получить конфигурацию LTR114.", true);
        m_ltr114.reset();
        return false;
    }

    TLTR114_LCHANNEL lch_tbl[1];

    // указываем номер канала, режим и диапазон измерений
    lch_tbl[0] = LTR114_CreateLChannel(LTR114_MEASMODE_U, 0, LTR114_URANGE_04);

    const int requestedSampleRate = sampleRateSpin->value();
    const DWORD divider = static_cast<DWORD>(qBound(2, 8000 / qMax(1, requestedSampleRate), 8000));
    // был тип string

    m_ltr114->set_freq_divider(divider);
    m_ltr114->set_logical_channels(1, lch_tbl);
    m_ltr114->set_sync_mode(LTR114_SYNCMODE_INTERNAL);
    m_ltr114->set_interval(0);

    if (!m_ltr114->apply_config()) {
        appendInfo("Не удалось применить настройки АЦП LTR114.", true);
        m_ltr114.reset();
        return false;
    }

    if (LTR114_Calibrate(m_ltr114->handle()) != LTR_OK) {
        appendInfo("LTR114: ошибка автокалибровки.", true);
        m_ltr114.reset();
        return false;
    }

    if (!m_ltr114->start()) {
        appendInfo("LTR114: не удалось запустить сбор данных.", true);
        m_ltr114.reset();
        return false;
    }

    double sampleRateHz = static_cast<double>(LTR114_FREQ(*m_ltr114->handle())) * 1000.0;
    appendInfo(QString("Сбор запущен. Частота=%1 Гц (FreqDivider=%2, фактическая=%3 Гц)")
                   .arg(requestedSampleRate)
                   .arg(divider)
                   .arg(QString::number(sampleRateHz, 'f', 2)));
    setModuleStatus(m_ltr114Slot, true);
    return true;
}

void MainWindow::close_ltr114_capture()
{
    if (m_simulationMode)
        return;

    if (!m_ltr114)
        return;

    m_ltr114->stop();
    m_ltr114->close();
    m_ltr114.reset();
}


bool MainWindow::open_ltr212_for_capture()
{
    if (m_simulationMode) {
        appendInfo("Симуляция LTR212 не поддерживается пока");
        return false;
    }

    if (m_crateSerial.isEmpty() || m_ltr212Slot < 0) {
        appendInfo("LTR212 не найден в крейте", true);
        return false;
    }

    m_ltr212 = std::make_unique<LTR212>();

    if (!m_ltr212->open(m_crateSerial, m_ltr212Slot)) {
        appendInfo("Не удалось открыть LTR212", true);
        m_ltr212.reset();
        return false;
    }

    // === КОНФИГУРАЦИЯ (подберите под свои нужды) ===
    m_ltr212->set_acq_mode(1);           // 1 = высокоточный 4-канальный режим (см. мануал)
    m_ltr212->set_use_clb(0);
    m_ltr212->set_use_fabric_clb(1);
    m_ltr212->set_ref_voltage(1);        // 1 = опорное 5 В
    m_ltr212->set_ac_mode(0);            // 0 = постоянное напряжение (DC)

    // Логические каналы — САМЫЙ ВАЖНЫЙ МОМЕНТ!
    // Посмотрите в ltr212api.h функцию LTR212_CreateLChannel (или как там называется)
    const int ch_count = 1;              // ← измените на нужное количество (1–8)
    INT ch_table[8] = {};                // обнуляем

    // Пример для 1 канала (замените на реальную функцию из вашего API)
    // ch_table[0] = LTR212_CreateLChannel(режим, канал, диапазон);
    ch_table[0] = 0;                     // минимальный вариант — просто номер канала

    m_ltr212->set_logical_channels(ch_count, ch_table);

    if (!m_ltr212->apply_config()) {
        appendInfo("Не удалось применить конфигурацию LTR212", true);
        m_ltr212.reset();
        return false;
    }

    // Калибровка (если есть функция в API)
    // if (LTR212_Calibrate(m_ltr212->handle()) != LTR_OK) { ... }

    if (!m_ltr212->start()) {
        appendInfo("Не удалось запустить сбор LTR212", true);
        m_ltr212.reset();
        return false;
    }

    appendInfo(QString("LTR212 запущен (каналов: %1)").arg(ch_count));
    return true;
}

void MainWindow::close_ltr212_capture()
{
    if (!m_ltr212)
        return;

    m_ltr212->stop();
    m_ltr212->close();
    m_ltr212.reset();
}

void MainWindow::poll_ltr212_data()
{
    if (!m_captureRunning || !m_ltr212)
        return;

    int error = 0;
    // Запрашиваем сырые данные (можно увеличить размер буфера)
    QVector<DWORD> raw = m_ltr212->receive_data(
        static_cast<DWORD>(chunkSizeSpin->value() * 2), // *2 — запас под формат модуля
        &error);

    if (error != 0) {
        appendInfo(QString("LTR212: ошибка приёма %1").arg(error), true);
        on_stop_capture_clicked();
        return;
    }

    if (raw.isEmpty())
        return;

    QVector<double> voltageSamples = m_ltr212->process_data(raw, true); // true = в вольты

    if (!voltageSamples.isEmpty()) {
        process_voltage_samples(voltageSamples);   // уже существующий метод — график + файл
    }
}

void MainWindow::run_ltr212_module(const QString& crate_sn, int ltr212_slot)
{
    m_ltr212Slot = ltr212_slot;
    // m_crateSerial уже установлен LTR114 или можно установить отдельно
    appendInfo(QString("LTR212 найден в слоте %1. Готов к работе.").arg(ltr212_slot));
}

void MainWindow::refresh_plot()
{
    QVector<QPointF> scaledPoints;
    scaledPoints.reserve(m_plotPoints.size());
    const qreal factor = current_unit_factor();
    for (const QPointF& p : m_plotPoints) {
        scaledPoints.append(QPointF(p.x(), p.y() * factor));
    }

    lineSeries->replace(scaledPoints);

    if (!scaledPoints.isEmpty()) {
        const qreal maxX = scaledPoints.last().x();
        const qreal minWindowX = qMax<qreal>(1.0, maxX - static_cast<qreal>(PLOT_WINDOW_TICKS) + 1.0);
        const qreal minX = qMax<qreal>(scaledPoints.first().x(), minWindowX);
        axisX->setRange(minX, qMax(minX + 10.0, maxX));

        qreal maxAbsV = (current_unit_name() == "V") ? 0.001 : 1.0;
        for (const QPointF& p : scaledPoints)
            maxAbsV = qMax(maxAbsV, std::abs(p.y()));

        const qreal margin = maxAbsV * 0.1;
        axisY->setRange(-(maxAbsV + margin), maxAbsV + margin);
    }
}

double MainWindow::current_unit_factor() const
{
    return (unitCombo && unitCombo->currentData().toString() == "V") ? 1.0 : 1000.0;
}

QString MainWindow::current_unit_name() const
{
    return (unitCombo && unitCombo->currentData().toString() == "V") ? "V" : "mV";
}

bool MainWindow::open_capture_file()
{
    if (m_captureFile)
        return true;

    m_captureFilePath = QString("ltr_capture_%1.txt")
                            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    m_captureFile = new QFile(m_captureFilePath);
    if (!m_captureFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        appendInfo(QString("Не удалось открыть файл %1 для записи").arg(m_captureFilePath), true);
        delete m_captureFile;
        m_captureFile = nullptr;
        return false;
    }

    m_captureStream = new QTextStream(m_captureFile);
    (*m_captureStream) << sampleRateSpin->value() << "    " << current_unit_name() << "    LTR114\n";
    m_captureStream->flush();
    return true;
}

bool MainWindow::append_samples_to_file(const QVector<QPair<quint64, double>>& samples)
{
    if (samples.isEmpty())
        return true;

    if (!open_capture_file() || !m_captureStream || !m_captureFile)
        return false;

    const double factor = current_unit_factor();
    const int precision = current_unit_name() == "V" ? 9 : 3;

    for (const auto& sample : samples) {
        (*m_captureStream) << sample.first << "    " << QString::number(sample.second * factor, 'f', precision) << "\n";
    }

    m_captureStream->flush();
    return (m_captureStream->status() == QTextStream::Ok);
}

void MainWindow::close_capture_file()
{
    if (m_captureStream) {
        m_captureStream->flush();
        delete m_captureStream;
        m_captureStream = nullptr;
    }

    if (m_captureFile) {
        m_captureFile->close();
        delete m_captureFile;
        m_captureFile = nullptr;
        if (!m_captureFilePath.isEmpty()) {
            appendInfo(QString("Файл сохранён: %1").arg(m_captureFilePath));
            m_captureFilePath.clear();
        }
    }
}

void MainWindow::setup_crate_sync()
{
    if (!m_crate || !m_crate->is_open()) {
        appendInfo("Крейт не открыт — синхрометки не настроены", true);
        return;
    }

    if (m_crate->setup_sync_marks()) {
        appendInfo("Синхрометки настроены (START + SECOND 1 Гц)");
    } else {
        appendInfo("Ошибка настройки синхрометок на крейте", true);
    }
}

void MainWindow::on_start_capture_clicked()
{
    if (m_captureRunning) return;

    m_allSamples.clear();
    m_plotPoints.clear();
    m_pendingFileSamples.clear();
    m_tickCounter = 0;
    refresh_plot();

    if (saveToFileCheck->isChecked() && !open_capture_file())
        return;

    // === НАСТРАИВАЕМ СИНХРОМЕТКИ ===
    setup_crate_sync();

    m_usingLtr114 = false;
    m_usingLtr212 = false;

    bool success = false;

    if (m_ltr114Slot != -1) {
        if (open_ltr114_for_capture()) {
            m_usingLtr114 = true;
            success = true;
        }
    }
    if (m_ltr212Slot != -1) {
        if (open_ltr212_for_capture()) {
            m_usingLtr212 = true;
            success = true;
        }
    }

    if (!success) {
        appendInfo("Ни LTR114, ни LTR212 не удалось запустить!", true);
        close_capture_file();
        return;
    }

    m_needSynchronization = (m_usingLtr114 && m_usingLtr212);
    m_syncInitialized = !m_needSynchronization;   // если один модуль — сразу готовы

    if (m_needSynchronization) {
        appendInfo("Запущены оба модуля → включаем синхронизацию по tmark");
    } else {
        appendInfo("Работаем с одним модулем — синхронизация не требуется");
    }

    m_captureRunning = true;
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    sampleRateSpin->setEnabled(false);
    unitCombo->setEnabled(false);

    acquisitionTimer.start(30);
}

void MainWindow::on_stop_capture_clicked()
{
    if (!m_captureRunning) return;

    acquisitionTimer.stop();
    m_captureRunning = false;

    close_ltr114_capture();
    close_ltr212_capture();

    if (m_crate && m_crate->is_open()) {
        m_crate->stop_sync_marks();
        appendInfo("Синхрометки остановлены");
    }

    if (saveToFileCheck->isChecked()) {
        if (!append_samples_to_file(m_pendingFileSamples)) {
            appendInfo("Ошибка дозаписи данных в файл.", true);
        }
        m_pendingFileSamples.clear();
        close_capture_file();
    } else {
        appendInfo("Сохранение файла отключено пользователем.");
    }

    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    sampleRateSpin->setEnabled(true);
    unitCombo->setEnabled(true);

    appendInfo("Сбор остановлен пользователем.");
}

void MainWindow::poll_ltr114_data()
{
    if (!m_captureRunning || !m_ltr114 || !m_usingLtr114) return;

    if (m_simulationMode) {
        process_voltage_samples(generate_simulated_samples());
        return;
    }

    int error = 0;
    auto [raw_data, tmarks] = m_ltr114->receive_data_with_marks(
        static_cast<DWORD>(chunkSizeSpin->value()), &error);

    if (error != 0) {
        appendInfo(QString("LTR114: ошибка приёма: %1").arg(error), true);
        on_stop_capture_clicked();
        return;
    }
    if (raw_data.isEmpty()) return;

    // Здесь можно добавить обрезку по tmarks (позже)

    QVector<double> proc_data(raw_data.size());
    QVector<double> therm_data(raw_data.size());
    INT proc_size = raw_data.size();
    INT therm_count = 0;

    INT proc_result = LTR114_ProcessDataTherm(
        m_ltr114->handle(), raw_data.data(), proc_data.data(), therm_data.data(),
        &proc_size, &therm_count, LTR114_CORRECTION_MODE_INIT, LTR114_PROCF_VALUE);

    if (proc_result != LTR_OK) {
        appendInfo(QString("LTR114: ошибка обработки: %1").arg(proc_result), true);
        return;
    }

    QVector<double> voltageSamples;
    voltageSamples.reserve(proc_size);
    for (int i = 0; i < proc_size; ++i)
        voltageSamples.append(proc_data[i]);

    if (!voltageSamples.isEmpty())
        process_voltage_samples(voltageSamples);
}

void MainWindow::poll_capture_data()
{
    if (!m_captureRunning)
        return;

    if (m_simulationMode) {
        process_voltage_samples(generate_simulated_samples());
        return;
    }

    QVector<double> voltageSamples;

    // ====================== LTR114 ======================
    if (m_usingLtr114 && m_ltr114) {
        int error = 0;
        auto [raw, tmarks] = m_ltr114->receive_data_with_marks(
            static_cast<DWORD>(chunkSizeSpin->value()), &error);

        if (error != 0) {
            appendInfo(QString("LTR114: ошибка приёма %1").arg(error), true);
            on_stop_capture_clicked();
            return;
        }

        if (!raw.isEmpty()) {
            QVector<DWORD> dataToProcess = raw;

            if (m_needSynchronization && m_syncInitialized) {
                dataToProcess = trim_by_start_mark(raw, tmarks, m_refStartMark);
            }

            if (!dataToProcess.isEmpty()) {
                QVector<double> proc_data(dataToProcess.size());
                QVector<double> therm_data(dataToProcess.size());
                INT proc_size = dataToProcess.size();
                INT therm_count = 0;

                if (LTR114_ProcessDataTherm(m_ltr114->handle(), dataToProcess.data(),
                                            proc_data.data(), therm_data.data(), &proc_size, &therm_count,
                                            LTR114_CORRECTION_MODE_INIT, LTR114_PROCF_VALUE) == LTR_OK) {

                    for (int i = 0; i < proc_size; ++i)
                        voltageSamples.append(proc_data[i]);
                }
            }
        }
    }

    // ====================== LTR212 ======================
    if (m_usingLtr212 && m_ltr212) {
        int error = 0;
        auto [raw, tmarks] = m_ltr212->receive_data_with_marks(
            static_cast<DWORD>(chunkSizeSpin->value() * 2), &error);

        if (error != 0) {
            appendInfo(QString("LTR212: ошибка приёма %1").arg(error), true);
            on_stop_capture_clicked();
            return;
        }

        if (!raw.isEmpty()) {
            QVector<DWORD> dataToProcess = raw;

            if (m_needSynchronization && m_syncInitialized) {
                dataToProcess = trim_by_start_mark(raw, tmarks, m_refStartMark);
            }

            if (!dataToProcess.isEmpty()) {
                QVector<double> proc = m_ltr212->process_data(dataToProcess, true);
                voltageSamples.append(proc);
            }
        }
    }

    if (!voltageSamples.isEmpty())
        process_voltage_samples(voltageSamples);
}


void MainWindow::process_voltage_samples(const QVector<double>& voltageSamples)
{
    if (voltageSamples.isEmpty())
        return;

    const int everyN = qMax(1, plotEverySpin->value());
    for (double voltageV : voltageSamples) {
        ++m_tickCounter;
        m_allSamples.append(qMakePair(m_tickCounter, voltageV));
        m_pendingFileSamples.append(qMakePair(m_tickCounter, voltageV));

        if ((m_tickCounter % static_cast<quint64>(everyN)) == 0) {
            m_plotPoints.append(QPointF(static_cast<qreal>(m_tickCounter), voltageV));
        }
    }

    if (saveToFileCheck->isChecked() && !m_pendingFileSamples.isEmpty()) {
        const int flushEverySamples = qMax(1, sampleRateSpin->value());
        if (m_pendingFileSamples.size() >= flushEverySamples) {
            if (!append_samples_to_file(m_pendingFileSamples)) {
                appendInfo("Ошибка записи в файл во время сбора.", true);
            }
            m_pendingFileSamples.clear();
        }
    }

    const int maxPlotPoints = static_cast<int>(qMax<quint64>(1, PLOT_WINDOW_TICKS / static_cast<quint64>(everyN) + 2));
    if (m_plotPoints.size() > maxPlotPoints) {
        m_plotPoints.remove(0, m_plotPoints.size() - maxPlotPoints);
    }

    refresh_plot();
}

QVector<double> MainWindow::generate_simulated_samples()
{
    QVector<double> samples;
    const double sampleRate = static_cast<double>(qMax(1, sampleRateSpin->value()));
    const double timerPeriodSec = static_cast<double>(acquisitionTimer.interval()) / 1000.0;
    m_simulatedSampleAccumulator += sampleRate * timerPeriodSec;

    int samplesToGenerate = static_cast<int>(m_simulatedSampleAccumulator);
    m_simulatedSampleAccumulator -= samplesToGenerate;

    if (samplesToGenerate <= 0)
        return samples;

    samples.reserve(samplesToGenerate);
    const double dt = 1.0 / sampleRate;
    const double freq1 = qMin(5.0, sampleRate / 20.0);
    const double freq2 = qMin(13.0, sampleRate / 12.0);
    constexpr double pi = 3.14159265358979323846;

    for (int i = 0; i < samplesToGenerate; ++i) {
        const double t = (static_cast<double>(m_tickCounter) + static_cast<double>(i) + 1.0) * dt;
        const double signal = 0.0012 * std::sin(2.0 * pi * freq1 * t)
                            + 0.00045 * std::sin(2.0 * pi * freq2 * t + 0.8)
                            + 0.00015 * std::sin(2.0 * pi * 0.35 * t);
        samples.append(signal);
    }

    return samples;
}

void MainWindow::init_ltr()
{
    appendInfo("Начало поиска крейтов...");

    auto crates = Crate::enumerate_crates();
    if (crates.isEmpty()) {
        m_simulationMode = true;
        m_crateSerial = "SIMULATED_CRATE";
        m_ltr114Slot = 1;

        appendInfo("Нет подключенных крейтов. Включен режим симуляции.", true);

        QWidget* w = createModuleItemWidget(1, "LTR114 (SIM)", true);
        QListWidgetItem* it = new QListWidgetItem(modulesList);
        it->setSizeHint(w->sizeHint());
        modulesList->addItem(it);
        modulesList->setItemWidget(it, w);
        moduleWidgets.insert(1, w);

        appendInfo("Симулированный крейт создан, модуль LTR114 виртуально доступен в слоте 1.");
        run_ltr114_module(m_crateSerial, m_ltr114Slot);
        return;
    }

    appendInfo(QString("Найдено %1 крейт(ов):").arg(crates.size()));
    for (const auto& s : crates) appendInfo(QString("  %1").arg(s));

    QString crate_sn = crates.first();
    appendInfo(QString("Открываем крейт SN: %1").arg(crate_sn));

    m_crate = std::make_unique<Crate>(crate_sn);
    if (!m_crate->is_open()) {
        appendInfo("Соединение с крейтом оставлено открытым для синхрометок", true);
        // m_crate.reset();
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

    int ltr114_slot = -1;
    for (const auto& mod : modules) {
        if (mod.second == LTR_MID_LTR114 && ltr114_slot == -1)
            ltr114_slot = mod.first;
    }

    if (ltr114_slot != -1) {
        appendInfo(QString("LTR114 IN SLOT %1").arg(ltr114_slot));
        run_ltr114_module(crate_sn, ltr114_slot);
    } else {
        appendInfo("LTR114 error", true);
        startButton->setEnabled(false);
    }

    int ltr212_slot = -1;
    for (const auto& mod : modules) {
        if (mod.second == LTR_MID_LTR212 && ltr212_slot == -1)
            ltr212_slot = mod.first;
    }

    if (ltr212_slot != -1) {
        appendInfo(QString("LTR212 найден в слоте %1").arg(ltr212_slot));
        run_ltr212_module(crate_sn, ltr212_slot);
    }

    m_crate.reset();
    appendInfo("Поиск модулей завершён. Соединение с крейтом закрыто до старта.");
}

quint32 MainWindow::extractStartMark(DWORD tmarkWord) const
{
    return (tmarkWord >> 16) & 0xFFFF;   // старшие 16 бит — счётчик START
}

QVector<DWORD> MainWindow::trim_by_start_mark(const QVector<DWORD>& raw,
                                              const QVector<DWORD>& tmarks,
                                              quint32 refStart)
{
    if (raw.isEmpty() || tmarks.isEmpty() || raw.size() != tmarks.size())
        return raw;

    for (int i = 0; i < tmarks.size(); ++i) {
        if (extractStartMark(tmarks[i]) >= refStart) {
            return raw.mid(i);               // обрезаем всё, что было раньше
        }
    }
    return {}; // весь пакет раньше референса — отбрасываем
}

void MainWindow::initializeSync(quint32 start114, quint32 start212)
{
    m_refStartMark = qMax(start114, start212);
    m_syncInitialized = true;

    appendInfo(QString("Синхронизация по tmark готова. Референс START = %1 (114=%2, 212=%3)")
                   .arg(m_refStartMark).arg(start114).arg(start212));
}
