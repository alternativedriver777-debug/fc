#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QListWidget>
#include <QTextEdit>

#include <QMainWindow>
#include <QString>
#include <QMap>
#include "LTR/ltrapi.h"
#include "LTR/ltr11api.h"
#include "LTR/ltr114api.h"
#include "crate.h"
#include <QMainWindow>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Crate;
class LTR11;
class LTR114;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void init_ltr();
    static QString module_name(WORD mid);

    const int CONNECTION_TIMEOUT_MS = 10000;
    const int POLL_INTERVAL_MS = 200;

    Ui::MainWindow *ui;
    std::unique_ptr<Crate> m_crate;        // управление крейтом
    std::unique_ptr<LTR11> m_ltr11;        // управление модулем LTR11
    std::unique_ptr<LTR114> m_ltr114;      // управление модулем LTR114

    QListWidget* modulesList;
    QTextEdit* infoText;

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
};

#endif // MAINWINDOW_H
