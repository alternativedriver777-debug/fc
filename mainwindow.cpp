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
#include <QFormLayout>
#include <QFile>
#include <QGroupBox>
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
    , chartView114(nullptr)
    , chartView212(nullptr)
    , chart114(nullptr)
    , chart212(nullptr)
    , lineSeries114(nullptr)
    , lineSeries212(nullptr)
    , axisX114(nullptr)
    , axisY114(nullptr)
    , axisX212(nullptr)
    , axisY212(nullptr)
    , m_ltr114Slot(-1)
    , m_ltr212Slot(-1)
    , m_captureRunning(false)
    , m_captureFile(nullptr)
    , m_captureStream(nullptr)
    , m_simulationMode(false)
    , m_simulatedSampleAccumulator(0.0)
    , m_simulatedSampleAccumulator212(0.0)
{
    ui->setupUi(this);
    qRegisterMetaType<QVector<TimedSample>>("QVector<TimedSample>");

    init_ui_replace();
    setup_plot();
    appendInfo("Приложение запущено.");

    init_ltr();
}

MainWindow::~MainWindow()
{
    stop_worker_threads();
    close_capture_files();
    close_ltr114_capture();
    close_ltr212_capture();
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

    m_ltr212SettingsGroup = new QGroupBox("Настройки LTR212", this);
    QFormLayout* ltr212Form = new QFormLayout(m_ltr212SettingsGroup);

    m_acqModeCombo = new QComboBox(this);
    m_acqModeCombo->addItem("4 канала, средняя точность", 0);
    m_acqModeCombo->addItem("4 канала, высокая точность", 1);
    m_acqModeCombo->addItem("8 каналов, высокая точность", 2);
    ltr212Form->addRow("Режим АЦП:", m_acqModeCombo);

    m_useClb212Check = new QCheckBox(this);
    m_useClb212Check->setChecked(false);
    ltr212Form->addRow("UseClb:", m_useClb212Check);

    m_useFabricClb212Check = new QCheckBox(this);
    m_useFabricClb212Check->setChecked(true);
    ltr212Form->addRow("UseFabricClb:", m_useFabricClb212Check);

    m_refVoltageCombo = new QComboBox(this);
    m_refVoltageCombo->addItem("2.5 В", 0);
    m_refVoltageCombo->addItem("5 В", 1);
    ltr212Form->addRow("Опорное напряжение:", m_refVoltageCombo);

    m_acModeCombo = new QComboBox(this);
    m_acModeCombo->addItem("DC (постоянное)", 0);
    m_acModeCombo->addItem("AC (переменное)", 1);
    ltr212Form->addRow("Режим:", m_acModeCombo);

    m_ltr212ChCountSpin = new QSpinBox(this);
    m_ltr212ChCountSpin->setRange(1, 8);
    m_ltr212ChCountSpin->setValue(1);
    ltr212Form->addRow("Кол-во лог. каналов:", m_ltr212ChCountSpin);

    m_ltr212RangeCombo = new QComboBox(this);
    m_ltr212RangeCombo->addItem("±10 мВ", 0);
    m_ltr212RangeCombo->addItem("±20 мВ", 1);
    m_ltr212RangeCombo->addItem("±40 мВ", 2);
    m_ltr212RangeCombo->addItem("±80 мВ", 3);
    m_ltr212RangeCombo->addItem("0..+10 мВ", 4);
    m_ltr212RangeCombo->addItem("0..+20 мВ", 5);
    m_ltr212RangeCombo->addItem("0..+40 мВ", 6);
    m_ltr212RangeCombo->addItem("0..+80 мВ", 7);
    ltr212Form->addRow("Диапазон канала 1:", m_ltr212RangeCombo);

    rightLay->addWidget(m_ltr212SettingsGroup);
    m_ltr212SettingsGroup->setVisible(false);

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
        if (axisY114) {
            axisY114->setTitleText(QString("Напряжение, %1").arg(current_unit_name()));
            axisY114->setLabelFormat(current_unit_name() == "V" ? "%.6f" : "%.3f");
        }
        if (axisY212) {
            axisY212->setTitleText(QString("Напряжение, %1").arg(current_unit_name()));
            axisY212->setLabelFormat(current_unit_name() == "V" ? "%.6f" : "%.3f");
        }
        refresh_plot();
    });

    connect(m_acqModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (!m_acqModeCombo || !m_ltr212ChCountSpin)
            return;
        const int acqMode = m_acqModeCombo->currentData().toInt();
        const int maxChannels = (acqMode == 2) ? 8 : 4;
        m_ltr212ChCountSpin->setMaximum(maxChannels);
        if (m_ltr212ChCountSpin->value() > maxChannels) {
            m_ltr212ChCountSpin->setValue(maxChannels);
        }
    });

    appendInfo("Информационный виджет создан.");
}

void MainWindow::setup_plot()
{
    lineSeries114 = new QLineSeries(this);
    lineSeries114->setName("LTR114");
    lineSeries114->setColor(Qt::blue);
    chart114 = new QChart();
    chart114->addSeries(lineSeries114);
    chart114->legend()->setVisible(false);
    chart114->setTitle("LTR114");

    axisX114 = new QValueAxis();
    axisX114->setTitleText("Время / тики");
    axisX114->setLabelFormat("%i");
    axisX114->setRange(0, 100);
    axisY114 = new QValueAxis();
    axisY114->setTitleText("Напряжение, mV");
    axisY114->setLabelFormat("%.3f");
    axisY114->setRange(-100.0, 100.0);

    chart114->addAxis(axisX114, Qt::AlignBottom);
    chart114->addAxis(axisY114, Qt::AlignLeft);
    lineSeries114->attachAxis(axisX114);
    lineSeries114->attachAxis(axisY114);

    chartView114 = new QChartView(chart114, this);
    chartView114->setMinimumHeight(220);
    chartView114->setRenderHint(QPainter::Antialiasing);

    lineSeries212 = new QLineSeries(this);
    lineSeries212->setName("LTR212");
    lineSeries212->setColor(Qt::red);
    chart212 = new QChart();
    chart212->addSeries(lineSeries212);
    chart212->legend()->setVisible(false);
    chart212->setTitle("LTR212");

    axisX212 = new QValueAxis();
    axisX212->setTitleText("Время / тики");
    axisX212->setLabelFormat("%i");
    axisX212->setRange(0, 100);
    axisY212 = new QValueAxis();
    axisY212->setTitleText("Напряжение, mV");
    axisY212->setLabelFormat("%.3f");
    axisY212->setRange(-100.0, 100.0);

    chart212->addAxis(axisX212, Qt::AlignBottom);
    chart212->addAxis(axisY212, Qt::AlignLeft);
    lineSeries212->attachAxis(axisX212);
    lineSeries212->attachAxis(axisY212);

    chartView212 = new QChartView(chart212, this);
    chartView212->setMinimumHeight(220);
    chartView212->setRenderHint(QPainter::Antialiasing);

    auto* rightLay = qobject_cast<QVBoxLayout*>(qobject_cast<QWidget*>(infoText->parentWidget())->layout());
    if (rightLay) {
        rightLay->insertWidget(1, chartView114, 1);
        rightLay->insertWidget(2, chartView212, 1);
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
        m_simulatedSignalTick = 0;
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

    if (!m_ltr212->open(m_crateSerial, m_ltr212Slot, "ltr212.bio")) {  // ← используй полную версию open
        appendInfo("Не удалось открыть LTR212", true);
        m_ltr212.reset();
        return false;
    }

    m_ltr212->set_size();
    const INT acqMode = m_acqModeCombo ? m_acqModeCombo->currentData().toInt() : 1;
    const INT useClb = (m_useClb212Check && m_useClb212Check->isChecked()) ? 1 : 0;
    const INT useFabricClb = (m_useFabricClb212Check && m_useFabricClb212Check->isChecked()) ? 1 : 0;
    const INT refVolt = m_refVoltageCombo ? m_refVoltageCombo->currentData().toInt() : 1;
    const INT acMode = m_acModeCombo ? m_acModeCombo->currentData().toInt() : 0;
    const int maxChannels = (acqMode == 2) ? 8 : 4;
    const int ch_count = m_ltr212ChCountSpin ? qBound(1, m_ltr212ChCountSpin->value(), maxChannels) : 1;
    const INT scale = m_ltr212RangeCombo ? m_ltr212RangeCombo->currentData().toInt() : 3;

    m_ltr212->set_acq_mode(acqMode);
    m_ltr212->set_use_clb(useClb);
    m_ltr212->set_use_fabric_clb(useFabricClb);
    m_ltr212->set_ref_voltage(refVolt);
    m_ltr212->set_ac_mode(acMode);

    INT ch_table[8] = {};
    for (int i = 0; i < ch_count; ++i) {
        ch_table[i] = LTR212_CreateLChannel(i + 1, scale);
    }

    m_ltr212->set_logical_channels(ch_count, ch_table);

    // Применяем настройки
    if (!m_ltr212->apply_config()) {   // внутри: LTR212_SetADC()
        appendInfo("Не удалось применить конфигурацию LTR212", true);
        m_ltr212.reset();
        return false;
    }

    //  калибровка (рекомендуется
    if (LTR212_Calibrate(m_ltr212->handle(), nullptr, LTR212_CALIBR_MODE_INT_FULL, 1) != LTR_OK) {
        appendInfo("Предупреждение: LTR212_Calibrate не прошла (можно продолжить)", false);
    }

    // Запуск сбора
    if (!m_ltr212->start()) {
        appendInfo("Не удалось запустить сбор LTR212", true);
        m_ltr212.reset();
        return false;
    }

    appendInfo(QString("LTR212 успешно запущен: %1 канал(ов), AcqMode=%2, REF=%3, AC/DC=%4")
                   .arg(ch_count)
                   .arg(acqMode)
                   .arg(refVolt)
                   .arg(acMode));
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

void MainWindow::run_ltr212_module(const QString& crate_sn, int ltr212_slot)
{
    Q_UNUSED(crate_sn)
    m_ltr212Slot = ltr212_slot;
    // m_crateSerial уже установлен LTR114 или можно установить отдельно
    appendInfo(QString("LTR212 найден в слоте %1. Готов к работе.").arg(ltr212_slot));
}

void MainWindow::refresh_plot()
{
    const qreal factor = current_unit_factor();
    QVector<QPointF> scaledPoints114;
    scaledPoints114.reserve(m_plotPoints.size());
    for (const QPointF& p : m_plotPoints)
        scaledPoints114.append(QPointF(p.x(), p.y() * factor));
    lineSeries114->replace(scaledPoints114);

    QVector<QPointF> scaledPoints212;
    scaledPoints212.reserve(m_plotPoints212.size());
    for (const QPointF& p : m_plotPoints212)
        scaledPoints212.append(QPointF(p.x(), p.y() * factor));
    lineSeries212->replace(scaledPoints212);

    if (chartView114)
        chartView114->setVisible(m_simulationMode || m_usingLtr114 || !m_plotPoints.isEmpty());
    if (chartView212)
        chartView212->setVisible(m_simulationMode ? m_simulateTwoModules : (m_usingLtr212 || !m_plotPoints212.isEmpty()));

    auto updateAxes = [this](const QVector<QPointF>& points, QValueAxis* xAxis, QValueAxis* yAxis) {
        if (!xAxis || !yAxis || points.isEmpty())
            return;
        const qreal firstX = points.first().x();
        const qreal maxX = points.last().x();
        const qreal minWindowX = qMax<qreal>(1.0, maxX - static_cast<qreal>(PLOT_WINDOW_TICKS) + 1.0);
        const qreal minX = qMax(firstX, minWindowX);
        xAxis->setRange(minX, qMax(minX + 10.0, maxX));

        qreal maxAbsV = (current_unit_name() == "V") ? 0.001 : 1.0;
        for (const QPointF& p : points)
            maxAbsV = qMax(maxAbsV, std::abs(p.y()));
        const qreal margin = maxAbsV * 0.1;
        yAxis->setRange(-(maxAbsV + margin), maxAbsV + margin);
    };

    updateAxes(scaledPoints114, axisX114, axisY114);
    updateAxes(scaledPoints212, axisX212, axisY212);
}

double MainWindow::current_unit_factor() const
{
    return (unitCombo && unitCombo->currentData().toString() == "V") ? 1.0 : 1000.0;
}

QString MainWindow::current_unit_name() const
{
    return (unitCombo && unitCombo->currentData().toString() == "V") ? "V" : "mV";
}

bool MainWindow::open_capture_file(int moduleId)
{
    if (!saveToFileCheck || !saveToFileCheck->isChecked())
        return false;

    QFile*& targetFile = (moduleId == 1) ? m_captureFile212 : m_captureFile;
    QTextStream*& targetStream = (moduleId == 1) ? m_captureStream212 : m_captureStream;
    QString& targetPath = (moduleId == 1) ? m_captureFilePath212 : m_captureFilePath;
    const QString moduleName = (moduleId == 1) ? "LTR212" : "LTR114";

    if (targetFile)
        return true;

    targetPath = QString("ltr_capture_%1_%2.txt")
                     .arg(moduleName.toLower())
                     .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    targetFile = new QFile(targetPath);
    if (!targetFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        appendInfo(QString("Не удалось открыть файл %1 для записи").arg(targetPath), true);
        delete targetFile;
        targetFile = nullptr;
        return false;
    }

    targetStream = new QTextStream(targetFile);
    (*targetStream) << "# rate_hz=" << sampleRateSpin->value()
                    << " unit=" << current_unit_name()
                    << " module=" << moduleName
                    << " columns=global_tick second_mark sample_in_second value\n";
    targetStream->flush();
    return true;
}

bool MainWindow::append_samples_to_file(const QVector<TimedSample>& samples, int moduleId)
{
    if (samples.isEmpty())
        return true;

    QFile*& targetFile = (moduleId == 1) ? m_captureFile212 : m_captureFile;
    QTextStream*& targetStream = (moduleId == 1) ? m_captureStream212 : m_captureStream;

    if (!open_capture_file(moduleId) || !targetStream || !targetFile)
        return false;

    const double factor = current_unit_factor();
    const int precision = current_unit_name() == "V" ? 9 : 3;

    for (const TimedSample& sample : samples) {
        (*targetStream) << sample.globalTick << "    "
                        << sample.secondMark << "    "
                        << sample.sampleInSecond << "    "
                        << QString::number(sample.value * factor, 'f', precision) << "\n";
    }

    targetStream->flush();
    return (targetStream->status() == QTextStream::Ok);
}

void MainWindow::close_capture_file(int moduleId)
{
    QTextStream*& targetStream = (moduleId == 1) ? m_captureStream212 : m_captureStream;
    QFile*& targetFile = (moduleId == 1) ? m_captureFile212 : m_captureFile;
    QString& targetPath = (moduleId == 1) ? m_captureFilePath212 : m_captureFilePath;

    if (targetStream) {
        targetStream->flush();
        delete targetStream;
        targetStream = nullptr;
    }

    if (targetFile) {
        targetFile->flush();
        targetFile->close();
        delete targetFile;
        targetFile = nullptr;
        if (!targetPath.isEmpty()) {
            appendInfo(QString("Файл сохранён: %1").arg(targetPath));
            targetPath.clear();
        }
    }
}

void MainWindow::close_capture_files()
{
    close_capture_file(0);
    close_capture_file(1);
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
    m_plotPoints212.clear();
    m_pendingFileSamples114.clear();
    m_pendingFileSamples212.clear();
    refresh_plot();

    if (m_simulationMode) {
        if (saveToFileCheck->isChecked()) {
            if (!open_capture_file(0))
                return;
            if (m_simulateTwoModules && !open_capture_file(1)) {
                close_capture_file(0);
                return;
            }
        }

        m_captureRunning = true;
        m_simulatedSampleRate = qMax(1, sampleRateSpin->value());
        m_simulatedSampleAccumulator = 0.0;
        m_simulatedSampleAccumulator212 = 0.0;
        m_simulatedSignalTick = 0;
        m_simulatedSignalTick212 = 0;

        startButton->setEnabled(false);
        stopButton->setEnabled(true);
        sampleRateSpin->setEnabled(false);
        unitCombo->setEnabled(false);
        if (m_ltr212SettingsGroup)
            m_ltr212SettingsGroup->setEnabled(false);

        if (!m_simulationTimer) {
            m_simulationTimer = new QTimer(this);
            connect(m_simulationTimer, &QTimer::timeout, this, [this]() {
                if (m_captureRunning && m_simulationMode) {
                    process_voltage_samples(generate_simulated_samples(0), 0);
                    if (m_simulateTwoModules) {
                        const auto samples212 = generate_simulated_samples(1);
                        process_voltage_samples(samples212, 1);
                    }
                }
            });
        }

        m_usingLtr114 = false;
        m_usingLtr212 = false;
        m_simulationTimer->start(30);
        appendInfo("Симуляция запущена через QTimer.");
        return;
    }

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
        close_capture_files();
        return;
    }

    if (!m_simulationMode && m_usingLtr114 && m_usingLtr212) {
        setup_crate_sync();
    }

    if (saveToFileCheck->isChecked()) {
        if (m_usingLtr114 && !open_capture_file(0)) {
            close_ltr114_capture();
            close_ltr212_capture();
            return;
        }
        if (m_usingLtr212 && !open_capture_file(1)) {
            close_ltr114_capture();
            close_ltr212_capture();
            close_capture_file(0);
            return;
        }
    }

    m_syncState.needSynchronization = (m_usingLtr114 && m_usingLtr212);
    m_syncState.refInitialized = !m_syncState.needSynchronization;
    m_syncState.refStartMark = 0;
    m_syncState.refSecondMark = 0;
    m_syncState.seen114 = false;
    m_syncState.seen212 = false;
    m_syncState.start114 = 0;
    m_syncState.start212 = 0;
    m_syncState.second114 = 0;
    m_syncState.second212 = 0;
    m_syncState.timeBaseTicks = 1000000ULL;

    if (m_syncState.needSynchronization) {
        appendInfo("Запущены оба модуля → включаем синхронизацию по tmark");
    } else {
        appendInfo("Работаем с одним модулем — синхронизация не требуется");
    }

    m_captureRunning = true;
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    sampleRateSpin->setEnabled(false);
    unitCombo->setEnabled(false);
    if (m_ltr212SettingsGroup)
        m_ltr212SettingsGroup->setEnabled(false);

    if (m_usingLtr114 && m_ltr114) {
        m_ltr114Thread = new QThread(this);
        m_ltr114Worker = new Ltr114Worker(m_ltr114.get(), &m_syncState);
        m_ltr114Worker->moveToThread(m_ltr114Thread);

        connect(m_ltr114Thread, &QThread::started, m_ltr114Worker, &Ltr114Worker::run);
        connect(m_ltr114Worker, &Ltr114Worker::newVoltageSamples, this, [this](const QVector<TimedSample>& samples) {
            process_voltage_samples(samples, 0);
        }, Qt::QueuedConnection);
        connect(m_ltr114Worker, &Ltr114Worker::acquisitionError, this, [this](const QString& error) {
            appendInfo(error, true);
            QMetaObject::invokeMethod(this, "on_stop_capture_clicked", Qt::QueuedConnection);
        }, Qt::QueuedConnection);
        connect(m_ltr114Worker, &Ltr114Worker::finished, m_ltr114Thread, &QThread::quit);
        connect(m_ltr114Thread, &QThread::finished, m_ltr114Worker, &QObject::deleteLater);

        m_ltr114Thread->start();
    }

    if (m_usingLtr212 && m_ltr212) {
        m_ltr212Thread = new QThread(this);
        m_ltr212Worker = new Ltr212Worker(m_ltr212.get(), &m_syncState);
        m_ltr212Worker->moveToThread(m_ltr212Thread);

        connect(m_ltr212Thread, &QThread::started, m_ltr212Worker, &Ltr212Worker::run);
        connect(m_ltr212Worker, &Ltr212Worker::newVoltageSamples, this, [this](const QVector<TimedSample>& samples) {
            process_voltage_samples(samples, 1);
        }, Qt::QueuedConnection);
        connect(m_ltr212Worker, &Ltr212Worker::acquisitionError, this, [this](const QString& error) {
            appendInfo(error, true);
            QMetaObject::invokeMethod(this, "on_stop_capture_clicked", Qt::QueuedConnection);
        }, Qt::QueuedConnection);
        connect(m_ltr212Worker, &Ltr212Worker::finished, m_ltr212Thread, &QThread::quit);
        connect(m_ltr212Thread, &QThread::finished, m_ltr212Worker, &QObject::deleteLater);

        m_ltr212Thread->start();
    }
}

void MainWindow::on_stop_capture_clicked()
{
    if (!m_captureRunning) return;

    m_captureRunning = false;
    if (m_simulationTimer) {
        m_simulationTimer->stop();
    }

    stop_worker_threads();
    close_ltr114_capture();
    close_ltr212_capture();

    if (saveToFileCheck->isChecked()) {
        if (!append_samples_to_file(m_pendingFileSamples114, 0)) {
            appendInfo("Ошибка дозаписи данных в файл.", true);
        }
        if (!append_samples_to_file(m_pendingFileSamples212, 1)) {
            appendInfo("Ошибка дозаписи данных в файл LTR212.", true);
        }
        m_pendingFileSamples114.clear();
        m_pendingFileSamples212.clear();
        close_capture_files();
    } else {
        appendInfo("Сохранение файла отключено пользователем.");
    }

    if (m_crate && m_crate->is_open()) {
        m_crate->stop_sync_marks();
        appendInfo("Синхрометки остановлены");
    }

    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    sampleRateSpin->setEnabled(true);
    unitCombo->setEnabled(true);
    if (m_ltr212SettingsGroup)
        m_ltr212SettingsGroup->setEnabled(m_ltr212Slot != -1);

    appendInfo("Сбор остановлен пользователем.");
}

void MainWindow::process_voltage_samples(const QVector<TimedSample>& voltageSamples, int moduleId)
{
    if (voltageSamples.isEmpty())
        return;

    const int everyN = qMax(1, plotEverySpin->value());
    QVector<TimedSample>& modulePendingFileSamples = (moduleId == 1) ? m_pendingFileSamples212 : m_pendingFileSamples114;

    for (const TimedSample& sample : voltageSamples) {
        m_allSamples.append(sample);
        modulePendingFileSamples.append(sample);

        if ((sample.globalTick % static_cast<quint64>(everyN)) == 0) {
            if (moduleId == 0) {
                m_plotPoints.append(QPointF(static_cast<qreal>(sample.globalTick), sample.value));
            } else if (moduleId == 1) {
                m_plotPoints212.append(QPointF(static_cast<qreal>(sample.globalTick), sample.value));
            }
        }
    }

    if (saveToFileCheck->isChecked() && !modulePendingFileSamples.isEmpty()) {
        const int flushEverySamples = qMax(1, sampleRateSpin->value());
        if (modulePendingFileSamples.size() >= flushEverySamples) {
            if (!append_samples_to_file(modulePendingFileSamples, moduleId)) {
                appendInfo("Ошибка записи в файл во время сбора.", true);
            }
            modulePendingFileSamples.clear();
        }
    }

    const int maxPlotPoints = static_cast<int>(qMax<quint64>(1, PLOT_WINDOW_TICKS / static_cast<quint64>(everyN) + 2));
    if (m_plotPoints.size() > maxPlotPoints)
        m_plotPoints.remove(0, m_plotPoints.size() - maxPlotPoints);
    if (m_plotPoints212.size() > maxPlotPoints)
        m_plotPoints212.remove(0, m_plotPoints212.size() - maxPlotPoints);

    if (moduleId == 0 || moduleId == 1) {
        refresh_plot();
    }
}

QVector<TimedSample> MainWindow::generate_simulated_samples(int moduleId)
{
    QVector<TimedSample> samples;
    const double sampleRate = static_cast<double>(qMax(1, m_simulatedSampleRate));
    const double timerPeriodSec = 0.03;
    double& accumulator = (moduleId == 1) ? m_simulatedSampleAccumulator212 : m_simulatedSampleAccumulator;
    quint64& signalTick = (moduleId == 1) ? m_simulatedSignalTick212 : m_simulatedSignalTick;
    accumulator += sampleRate * timerPeriodSec;

    int samplesToGenerate = static_cast<int>(accumulator);
    accumulator -= samplesToGenerate;

    if (samplesToGenerate <= 0)
        return samples;

    samples.reserve(samplesToGenerate);
    const double dt = 1.0 / sampleRate;
    const double freq1 = (moduleId == 1) ? qMin(7.0, sampleRate / 17.0) : qMin(5.0, sampleRate / 20.0);
    const double freq2 = (moduleId == 1) ? qMin(11.0, sampleRate / 10.0) : qMin(13.0, sampleRate / 12.0);
    const double amp1 = (moduleId == 1) ? 0.00095 : 0.0012;
    const double amp2 = (moduleId == 1) ? 0.00060 : 0.00045;
    const double amp3 = (moduleId == 1) ? 0.00022 : 0.00015;
    const double phase2 = (moduleId == 1) ? 1.4 : 0.8;
    const double slowFreq = (moduleId == 1) ? 0.55 : 0.35;
    constexpr double pi = 3.14159265358979323846;

    for (int i = 0; i < samplesToGenerate; ++i) {
        const double t = (static_cast<double>(signalTick) + static_cast<double>(i) + 1.0) * dt;
        const double signal = amp1 * std::sin(2.0 * pi * freq1 * t)
                            + amp2 * std::sin(2.0 * pi * freq2 * t + phase2)
                            + amp3 * std::sin(2.0 * pi * slowFreq * t);
        TimedSample sample;
        sample.globalTick = (signalTick + static_cast<quint64>(i) + 1ULL) * 1000000ULL / static_cast<quint64>(sampleRate);
        sample.secondMark = static_cast<quint32>((signalTick + static_cast<quint64>(i)) / static_cast<quint64>(sampleRate));
        sample.sampleInSecond = static_cast<quint32>((signalTick + static_cast<quint64>(i)) % static_cast<quint64>(sampleRate));
        sample.value = signal;
        samples.append(sample);
    }

    signalTick += static_cast<quint64>(samplesToGenerate);
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
        if (m_ltr212SettingsGroup)
            m_ltr212SettingsGroup->setVisible(true);
        if (m_ltr212SettingsGroup)
            m_ltr212SettingsGroup->setEnabled(true);
    } else {
        if (m_ltr212SettingsGroup)
            m_ltr212SettingsGroup->setEnabled(false);
    }

    appendInfo("Поиск модулей завершён. Соединение с крейтом оставлено открытым для синхрометок.");
}

void MainWindow::stop_worker_threads()
{
    if (m_ltr114Worker) {
        QMetaObject::invokeMethod(m_ltr114Worker, "stopAcquisition", Qt::DirectConnection);
    }
    if (m_ltr212Worker) {
        QMetaObject::invokeMethod(m_ltr212Worker, "stopAcquisition", Qt::DirectConnection);
    }

    // Прерываем потенциально блокирующий Recv в драйвере.
    if (m_ltr114) {
        m_ltr114->stop();
    }
    if (m_ltr212) {
        m_ltr212->stop();
    }

    if (m_ltr114Thread) {
        m_ltr114Thread->quit();
        if (!m_ltr114Thread->wait(5000)) {
            appendInfo("LTR114 worker не завершился за 5000 мс", true);
        }
        m_ltr114Thread->deleteLater();
        m_ltr114Thread = nullptr;
        m_ltr114Worker = nullptr;
    }

    if (m_ltr212Thread) {
        m_ltr212Thread->quit();
        if (!m_ltr212Thread->wait(5000)) {
            appendInfo("LTR212 worker не завершился за 5000 мс", true);
        }
        m_ltr212Thread->deleteLater();
        m_ltr212Thread = nullptr;
        m_ltr212Worker = nullptr;
    }
}
