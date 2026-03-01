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
#include <QFile>
#include <QTextStream>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <numeric>
#include <algorithm>

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
    , startButton(nullptr)
    , stopButton(nullptr)
    , sampleRateSpin(nullptr)
    , plotEverySpin(nullptr)
    , chunkSizeSpin(nullptr)
    , saveToFileCheck(nullptr)
    , chartView(nullptr)
    , chart(nullptr)
    , lineSeries(nullptr)
    , axisX(nullptr)
    , axisY(nullptr)
    , m_ltr114Slot(-1)
    , m_captureRunning(false)
    , m_tickCounter(0)
{
    ui->setupUi(this);

    init_ui_replace();
    setup_plot();
    appendInfo("Приложение запущено.");

    connect(&acquisitionTimer, &QTimer::timeout, this, &MainWindow::poll_ltr114_data);
    init_ltr();
}

MainWindow::~MainWindow()
{
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

    appendInfo("Информационный виджет создан.");
}

void MainWindow::setup_plot()
{
    lineSeries = new QLineSeries(this);

    chart = new QChart();
    chart->addSeries(lineSeries);
    chart->legend()->hide();
    chart->setTitle("LTR114: Тики / Напряжение");

    axisX = new QValueAxis();
    axisX->setTitleText("Тики");
    axisX->setLabelFormat("%i");
    axisX->setRange(0, 100);

    axisY = new QValueAxis();
    axisY->setTitleText("Напряжение, В");
    axisY->setLabelFormat("%.6f");
    axisY->setRange(-0.1, 0.1);

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
    lch_tbl[0] = LTR114_CreateLChannel(LTR114_MEASMODE_U, 0, LTR114_URANGE_04);

    const int requestedSampleRate = sampleRateSpin->value();
    const DWORD divider = static_cast<DWORD>(qBound(2, 8000 / qMax(1, requestedSampleRate), 8000));
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

    double sampleRateHz = static_cast<double>(LTR114_FREQ(m_ltr114->handle())) * 1000.0;
    appendInfo(QString("Сбор запущен. Частота=%1 Гц (FreqDivider=%2, фактическая=%3 Гц)")
                   .arg(requestedSampleRate)
                   .arg(divider)
                   .arg(QString::number(sampleRateHz, 'f', 2)));
    setModuleStatus(m_ltr114Slot, true);
    return true;
}

void MainWindow::close_ltr114_capture()
{
    if (!m_ltr114)
        return;

    m_ltr114->stop();
    m_ltr114->close();
    m_ltr114.reset();
}

void MainWindow::refresh_plot()
{
    lineSeries->replace(m_plotPoints);

    if (!m_plotPoints.isEmpty()) {
        const qreal minX = m_plotPoints.first().x();
        const qreal maxX = m_plotPoints.last().x();
        axisX->setRange(minX, qMax(minX + 10.0, maxX));

        qreal maxAbsV = 0.001;
        for (const QPointF& p : m_plotPoints)
            maxAbsV = qMax(maxAbsV, std::abs(p.y()));

        const qreal margin = maxAbsV * 0.1;
        axisY->setRange(-(maxAbsV + margin), maxAbsV + margin);
    }
}

bool MainWindow::save_capture_to_file(const QString& file_path)
{
    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendInfo(QString("Не удалось открыть файл %1 для записи").arg(file_path), true);
        return false;
    }

    QTextStream out(&file);
    for (const auto& sample : m_allSamples) {
        out << sample.first << ";" << QString::number(sample.second, 'f', 9) << "\n";
    }

    file.close();
    appendInfo(QString("Файл сохранён: %1 (%2 строк)").arg(file_path).arg(m_allSamples.size()));
    return true;
}

void MainWindow::on_start_capture_clicked()
{
    if (m_captureRunning)
        return;

    m_allSamples.clear();
    m_plotPoints.clear();
    m_tickCounter = 0;
    refresh_plot();

    if (!open_ltr114_for_capture())
        return;

    m_captureRunning = true;
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    sampleRateSpin->setEnabled(false);

    acquisitionTimer.start(30);
}

void MainWindow::on_stop_capture_clicked()
{
    if (!m_captureRunning)
        return;

    acquisitionTimer.stop();
    m_captureRunning = false;

    close_ltr114_capture();

    if (saveToFileCheck->isChecked()) {
        const QString fileName = QString("ltr114_capture_%1.txt")
                                     .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
        save_capture_to_file(fileName);
    } else {
        appendInfo("Сохранение файла отключено пользователем.");
    }

    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    sampleRateSpin->setEnabled(true);

    appendInfo("Сбор остановлен пользователем.");
}

void MainWindow::poll_ltr114_data()
{
    if (!m_captureRunning || !m_ltr114)
        return;

    int error = 0;
    QVector<DWORD> data = m_ltr114->receive_data(static_cast<DWORD>(chunkSizeSpin->value()), &error);

    if (error != 0) {
        appendInfo(QString("LTR114: ошибка приёма данных: %1").arg(error), true);
        on_stop_capture_clicked();
        return;
    }

    if (data.isEmpty())
        return;

    QVector<double> proc_data(data.size());
    QVector<double> therm_data(data.size());
    INT proc_size = data.size();
    INT therm_count = 0;

    INT proc_result = LTR114_ProcessDataTherm(
        m_ltr114->handle(),
        data.data(),
        proc_data.data(),
        therm_data.data(),
        &proc_size,
        &therm_count,
        LTR114_CORRECTION_MODE_INIT,
        LTR114_PROCF_VALUE);

    if (proc_result != LTR_OK) {
        appendInfo(QString("LTR114: ошибка обработки данных: %1").arg(proc_result), true);
        return;
    }

    const int everyN = qMax(1, plotEverySpin->value());
    for (int i = 0; i < therm_count; ++i) {
        const double voltage = therm_data[i];
        ++m_tickCounter;
        m_allSamples.append(qMakePair(m_tickCounter, voltage));

        if ((m_tickCounter % static_cast<quint64>(everyN)) == 0) {
            m_plotPoints.append(QPointF(static_cast<qreal>(m_tickCounter), voltage));
        }
    }

    const int maxPlotPoints = 5000;
    if (m_plotPoints.size() > maxPlotPoints) {
        m_plotPoints.remove(0, m_plotPoints.size() - maxPlotPoints);
    }

    refresh_plot();
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

    int ltr114_slot = -1;
    for (const auto& mod : modules) {
        if (mod.second == LTR_MID_LTR114 && ltr114_slot == -1)
            ltr114_slot = mod.first;
    }

    if (ltr114_slot != -1) {
        appendInfo(QString("Модуль LTR114 найден в слоте %1").arg(ltr114_slot));
        run_ltr114_module(crate_sn, ltr114_slot);
    } else {
        appendInfo("Модуль LTR114 не найден", true);
        startButton->setEnabled(false);
    }

    m_crate.reset();
    appendInfo("Поиск модулей завершён. Соединение с крейтом закрыто до старта.");
}
