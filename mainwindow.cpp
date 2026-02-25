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
    // сохраняем вызов setupUi чтобы проект не ломался, но интерфейс дополним программно
    ui->setupUi(this);

    init_ui_replace(); // создаём список модулей + информационный виджет
    appendInfo("Приложение запущено.");

    // стартуем инициализацию LTR (всё логируется в infoText)
    init_ltr();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::init_ui_replace()
{
    // Если в .ui есть centralWidget, используем его, иначе создаём
    QWidget* central = ui->centralwidget ? ui->centralwidget : new QWidget(this);
    setCentralWidget(central);

    // основная горизонтальная компоновка: слева — список модулей, справа — лог/информация
    QHBoxLayout* mainLay = new QHBoxLayout;
    central->setLayout(mainLay);

    // Список модулей
    modulesList = new QListWidget(this);
    modulesList->setMinimumWidth(320);
    modulesList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    modulesList->setSelectionMode(QAbstractItemView::NoSelection);
    modulesList->setFocusPolicy(Qt::NoFocus);

    // Информационное окно: read-only, моноширинный шрифт будет удобен для логов
    infoText = new QTextEdit(this);
    infoText->setReadOnly(true);
    infoText->setAcceptRichText(false);
    QFont f = infoText->font();
    f.setFamily("Monospace");
    f.setStyleHint(QFont::Monospace);
    infoText->setFont(f);

    // компоновка
    mainLay->addWidget(modulesList, 0); // левее
    mainLay->addWidget(infoText, 1);    // правее (растягивается)

    // опционально — заголовок в info
    appendInfo("Информационный виджет создан.");
}

void MainWindow::appendInfo(const QString &msg, bool isError)
{
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString line = QString("[%1] %2").arg(time, msg);
    if (isError) {
        // красный цвет для ошибок
        infoText->setTextColor(Qt::red);
        infoText->append(line);
        infoText->setTextColor(Qt::black);
    } else {
        infoText->append(line);
    }

    // и в debug
    qDebug() << line;
}

QWidget* MainWindow::createModuleItemWidget(int slot, const QString &name, bool ok)
{
    QWidget* w = new QWidget;
    QHBoxLayout* lay = new QHBoxLayout(w);
    lay->setContentsMargins(6, 3, 6, 3);

    QLabel* slotLabel = new QLabel(QString("Слот %1").arg(slot));
    slotLabel->setMinimumWidth(70);
    QLabel* nameLabel = new QLabel(name);
    nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QLabel* indicator = new QLabel;
    indicator->setFixedSize(14, 14);
    indicator->setObjectName(QString("indicator_%1").arg(slot));
    indicator->setStyleSheet(QString("border-radius:7px; background-color: %1;")
                                 .arg(ok ? "green" : "red"));

    lay->addWidget(slotLabel);
    lay->addWidget(nameLabel, 1);
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

void MainWindow::init_ltr()
{
    appendInfo("Начало поиска крейтов...");

    // Получаем список крейтов
    auto crates = Crate::enumerate_crates();
    if (crates.isEmpty()) {
        appendInfo("Нет подключенных крейтов", true);
        return;
    }

    appendInfo(QString("Найдено %1 крейт(ов):").arg(crates.size()));
    for (const auto& s : crates) appendInfo(QString("  %1").arg(s));

    // Открываем первый крейт
    QString crateSn = crates.first();
    appendInfo(QString("Открываем крейт SN: %1").arg(crateSn));

    m_crate = std::make_unique<Crate>(crateSn);
    if (!m_crate->is_open()) {
        appendInfo("Не удалось открыть крейт", true);
        m_crate.reset();
        return;
    }
    appendInfo("Крейт успешно открыт.");

    // Получаем список модулей
    auto modules = m_crate->get_modules();
    if (modules.isEmpty()) {
        appendInfo("В крейте нет модулей", true);
        return;
    }

    WORD slot_count = m_crate->get_slot_count();
    appendInfo(QString("Вместимость крейта: %1 слотов").arg(slot_count));

    // Для наглядности создаём элементы списка для всех слотов как минимум
    // заполнены значениями "EMPTY" по умолчанию
    for (int s = 0; s < slot_count; ++s) {
        QWidget* w = createModuleItemWidget(s, "EMPTY", false);
        QListWidgetItem* it = new QListWidgetItem(modulesList);
        it->setSizeHint(w->sizeHint());
        modulesList->addItem(it);
        modulesList->setItemWidget(it, w);
        moduleWidgets.insert(s, w);
    }

    // Обновляем информационный блок про модули
    appendInfo("Список модулей в крейте:");
    for (const auto& mod : modules) {
        int slot = mod.first;
        WORD mid = mod.second;
        QString name = module_name(mid);
        appendInfo(QString("  слот %1: %2").arg(slot).arg(name));
        // обновляем виджет указанного слота
        // создаём новый виджет и заменяем старый элемент в списке
        // (проще - обновим существующий)
        QWidget* w = moduleWidgets.value(slot, nullptr);
        if (w) {
            // обновка: найдём label с именем (мы ставили второй виджет) - проще: заменим весь itemWidget
            QWidget* newW = createModuleItemWidget(slot, name, true);
            // найдём соответствующий QListWidgetItem по индексу
            QListWidgetItem* item = modulesList->item(slot);
            if (item) {
                modulesList->setItemWidget(item, newW);
                moduleWidgets[slot] = newW;
            }
        }
    }

    // Ищем LTR11 среди модулей
    int ltr11Slot = -1;
    for (const auto& mod : modules) {
        if (mod.second == LTR_MID_LTR11) {
            ltr11Slot = mod.first;
            break;
        }
    }
    if (ltr11Slot == -1) {
        appendInfo("Модуль LTR11 не найден", true);
        return;
    } else {
        appendInfo(QString("Модуль LTR11 найден в слоте %1").arg(ltr11Slot));
        setModuleStatus(ltr11Slot, true); // индикатор зелёный
    }

    // Создаём и открываем LTR11
    m_ltr11 = std::make_unique<LTR11>();
    appendInfo(QString("Открываем LTR11 в слоте %1...").arg(ltr11Slot));
    if (!m_ltr11->open(crateSn, ltr11Slot)) {
        appendInfo(QString("Не удалось открыть LTR11 в слоте %1").arg(ltr11Slot), true);
        m_ltr11.reset();
        setModuleStatus(ltr11Slot, false);
        return;
    }
    appendInfo("LTR11 успешно открыт.");
    setModuleStatus(ltr11Slot, true);

    // Получаем конфигурацию
    if (!m_ltr11->get_config()) {
        appendInfo("Не удалось получить конфигурацию LTR11", true);
        return;
    }
    appendInfo(QString("Модуль: %1  SN: %2").arg(m_ltr11->module_name(), m_ltr11->module_serial()));

    // Настраиваем параметры
    m_ltr11->set_start_mode(LTR11_STARTADCMODE_INT);
    m_ltr11->set_input_mode(LTR11_INPMODE_INT);
    BYTE chTbl[] = { (0 << 6) | (0 << 4) | (0 << 0) }; // пример конфигурации
    m_ltr11->set_logical_channels(1, chTbl);
    m_ltr11->set_ADC_rate(1, 149);   // prescaler=1, divider=149
    // m_ltr11->set_adc_mode(LTR11_ADCMODE_ACQ);

    if (!m_ltr11->apply_config()) {
        appendInfo("Не удалось применить настройки АЦП", true);
        return;
    }
    appendInfo(QString("Параметры АЦП установлены, частота канала: %1 кГц").arg(m_ltr11->channel_rate()));

    // Запуск сбора
    if (!m_ltr11->start()) {
        appendInfo("Не удалось запустить сбор данных", true);
        return;
    }
    appendInfo("Сбор данных запущен.");

    // Приём данных
    int error = 0;
    QVector<DWORD> data = m_ltr11->receive_data(3000, &error);
    if (error != 0) {
        appendInfo(QString("Ошибка приёма данных: %1").arg(error), true);
        appendInfo("Сбор остановлен из-за ошибки.");
        // отмечаем модуль как проблемный
        setModuleStatus(ltr11Slot, false);
    } else {
        appendInfo(QString("Принято слов: %1").arg(data.size()));
        TLTR11* hmodule = m_ltr11->handle();

        QVector<double> voltages(data.size()); // выходные напряжения
        int data_size = data.size();

        if (data.isEmpty()) {
            appendInfo("НЕТ ДАННЫХ ДЛЯ ОБРАБОТКИ", true);
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
                appendInfo(QString("Ошибка обработки данных: %1").arg(proc_result), true);
            } else {
                appendInfo(QString("Обработано данных: %1 значений").arg(data_size));

                // Вывод первых значений (до 20)
                QString volt_vals;
                int channels = sizeof(chTbl) / sizeof(chTbl[0]);
                for (int i = 0; i < qMin(20, data_size); ++i) {
                    volt_vals += QString::number(voltages[i], 'f', 6) + " В  ";
                    if (channels > 1 && (i + 1) % channels == 0) volt_vals += "\n";
                }
                appendInfo("Первые напряжения (В):");
                appendInfo(volt_vals);

                // статистика
                double min_v = *std::min_element(voltages.begin(), voltages.begin() + data_size);
                double max_v = *std::max_element(voltages.begin(), voltages.begin() + data_size);
                double sum_v = std::accumulate(voltages.begin(), voltages.begin() + data_size, 0.0);
                double avg_v = sum_v / data_size;

                appendInfo(QString("Статистика: min=%1 В, max=%2 В, avg=%3 В, размах=%4 В")
                               .arg(QString::number(min_v, 'f', 6))
                               .arg(QString::number(max_v, 'f', 6))
                               .arg(QString::number(avg_v, 'f', 6))
                               .arg(QString::number(max_v - min_v, 'f', 6)));

                // Информационное сообщение в лог (вместо QMessageBox)
                appendInfo(QString("Получено %1 слов. Переведено %2 значений.")
                               .arg(data.size()).arg(data_size));
            }
        }
    }

    // Завершаем сбор и закрываем соединения
    if (m_ltr11) {
        m_ltr11->stop();
        m_ltr11->close();
        appendInfo("LTR11: сбор остановлен, модуль закрыт.");
    }
    m_ltr11.reset();
    m_crate.reset();
    appendInfo("Соединения с крейтом закрыты. Работа завершена.");
}
