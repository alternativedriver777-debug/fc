#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QListWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVector>
#include <QPair>
#include <QPointF>
#include <QElapsedTimer>

#include <QMainWindow>
#include <QString>
#include <QMap>
#include "LTR/ltrapi.h"
#include "LTR/ltr11api.h"
#include "LTR/ltr114api.h"
#include "crate.h"
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Crate;
class LTR11;
class LTR114;
class QPushButton;
class QSpinBox;

QT_BEGIN_NAMESPACE
class QValueAxis;
class QLineSeries;
class QChart;
class QChartView;
QT_END_NAMESPACE

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
    bool save_capture_to_file(const QString& file_path);

    const int CONNECTION_TIMEOUT_MS = 10000;
    const int POLL_INTERVAL_MS = 200;

    Ui::MainWindow *ui;
    std::unique_ptr<Crate> m_crate;        // управление крейтом
    std::unique_ptr<LTR11> m_ltr11;        // управление модулем LTR11
    std::unique_ptr<LTR114> m_ltr114;      // управление модулем LTR114

    QListWidget* modulesList;
    QTextEdit* infoText;
    QPushButton* startButton;
    QPushButton* stopButton;
    QSpinBox* freqDividerSpin;
    QSpinBox* plotEverySpin;
    QSpinBox* chunkSizeSpin;
    QChartView* chartView;
    QChart* chart;
    QLineSeries* lineSeries;
    QValueAxis* axisX;
    QValueAxis* axisY;
    QTimer acquisitionTimer;

    QString m_crateSerial;
    int m_ltr114Slot;
    bool m_captureRunning;
    bool m_simulationMode;
    quint64 m_tickCounter;
    QElapsedTimer m_simulationTime;
    qint64 m_lastSimulationElapsedMs;
    double m_simulatedSampleAccumulator;
    QVector<QPointF> m_plotPoints;
    QVector<QPair<quint64, double>> m_allSamples;

    // map slot -> widget (for quick status updates)
    QMap<int, QWidget*> moduleWidgets;

    // initialization & helpers
    void init_ui_replace();          // построение виджетов в окне (без .ui правок)
    // void init_ltr();                 // основная логика LTR (заменяет QMessageBox на лог)
    // static QString module_name(WORD mid);

    // helpers to manage info log and module indicators
    void appendInfo(const QString &msg, bool isError = false);
    QWidget* createModuleItemWidget(int slot, const QString &name, bool ok);
    void setModuleStatus(int slot, bool ok);
    void run_ltr11_module(const QString& crate_sn, int ltr11_slot);
    void run_ltr114_module(const QString& crate_sn, int ltr114_slot);

private slots:
    void on_start_capture_clicked();
    void on_stop_capture_clicked();
    void poll_ltr114_data();
};

#endif // MAINWINDOW_H
