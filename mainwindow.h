#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QListWidget>
#include <QTextEdit>
#include <QTimer>
#include <QThread>
#include <QVector>
#include <QPair>
#include <QPointF>

#include <QMainWindow>
#include <QString>
#include <QMap>

#include "LTR/ltrapi.h"
#include "LTR/ltr11api.h"
#include "LTR/ltr114api.h"
#include "LTR/ltr212api.h"

#include "crate.h"
#include "ltr11.h"
#include "ltr114.h"
#include "ltr212.h"
#include "ltr_workers.h"

#include "crate.h"
#include <memory>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE

    namespace QtCharts {
    class QValueAxis;
    class QLineSeries;
    class QChart;
    class QChartView;
}


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Crate;
class LTR11;
class LTR114;
class LTR212;
class QPushButton;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QFile;
class QTextStream;

// QT_BEGIN_NAMESPACE
// class QValueAxis;
// class QLineSeries;
// class QChart;
// class QChartView;
// QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void init_ltr();
    static QString module_name(WORD mid);
    void setup_plot();
    bool open_ltr114_for_capture();
    void close_ltr114_capture();
    void refresh_plot();
    bool open_capture_file();
    bool append_samples_to_file(const QVector<QPair<quint64, double>>& samples);
    void close_capture_file();
    double current_unit_factor() const;
    QString current_unit_name() const;

    const int CONNECTION_TIMEOUT_MS = 10000;
    const quint64 PLOT_WINDOW_TICKS = 300; // ширина наблюдаемого окна по X (в тиках)

    Ui::MainWindow *ui;
    std::unique_ptr<Crate> m_crate;        // управление крейтом
    std::unique_ptr<LTR11> m_ltr11;        // управление модулем LTR11
    std::unique_ptr<LTR114> m_ltr114;      // управление модулем LTR114
    std::unique_ptr<LTR212> m_ltr212;      // управление модулем LTR212

    QListWidget* modulesList;
    QTextEdit* infoText;
    QPushButton* startButton;
    QPushButton* stopButton;
    QSpinBox* sampleRateSpin;
    QSpinBox* plotEverySpin;
    QSpinBox* chunkSizeSpin;
    QCheckBox* saveToFileCheck;
    QComboBox* unitCombo;
    QChartView* chartView;
    QChart* chart;
    QLineSeries* lineSeries;
    QValueAxis* axisX;
    QValueAxis* axisY;

    QString m_crateSerial;
    int m_ltr114Slot = -1;
    int m_ltr212Slot = -1;

    bool m_usingLtr212 = false;
    bool m_usingLtr114 = false;

    // Общая синхронизация по tmark между worker-потоками.
    SyncState m_syncState;

    QThread* m_ltr114Thread = nullptr;
    QThread* m_ltr212Thread = nullptr;
    Ltr114Worker* m_ltr114Worker = nullptr;
    Ltr212Worker* m_ltr212Worker = nullptr;

    bool m_captureRunning = false;
    quint64 m_tickCounter = 0;
    QVector<QPointF> m_plotPoints;
    QVector<QPair<quint64, double>> m_allSamples;
    QVector<QPair<quint64, double>> m_pendingFileSamples;
    QFile* m_captureFile = nullptr;
    QTextStream* m_captureStream = nullptr;
    QString m_captureFilePath;
    bool m_simulationMode = false;
    double m_simulatedSampleAccumulator = 0.0;
    int m_simulatedSampleRate = 2000;
    quint64 m_simulatedSignalTick = 0;
    QTimer* m_simulationTimer = nullptr;

    QMap<int, QWidget*> moduleWidgets;

    void init_ui_replace();
    void appendInfo(const QString &msg, bool isError = false);
    QWidget* createModuleItemWidget(int slot, const QString &name, bool ok);
    void setModuleStatus(int slot, bool ok);
    void run_ltr11_module(const QString& crate_sn, int ltr11_slot);
    void run_ltr114_module(const QString& crate_sn, int ltr114_slot);
    void run_ltr212_module(const QString& crate_sn, int ltr212_slot);
    void process_voltage_samples(const QVector<double>& voltageSamples);
    QVector<double> generate_simulated_samples();

    void setup_crate_sync();
    void stop_worker_threads();
    bool open_ltr212_for_capture();
    void close_ltr212_capture();

private slots:
    void on_start_capture_clicked();
    void on_stop_capture_clicked();
};

#endif // MAINWINDOW_H
