#include "mainwindow.h"
#include "cameradecoder.h"
#include "videocell.h"

#include <QWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QApplication>
#include <QUrl>

static const int MAX_STREAMS = 18;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Видеоклиент — ONVIF камеры"));

    // --- Левая панель: обнаруженные камеры ---
    m_camList = new QListWidget;
    m_camList->setAlternatingRowColors(true);
    m_searchBtn = new QPushButton(QStringLiteral("🔍 Поиск ONVIF"));
    connect(m_searchBtn, &QPushButton::clicked, this, &MainWindow::startDiscovery);

    m_urlCombo = new QComboBox;
    m_urlCombo->setEditable(true);
    m_urlCombo->setInsertPolicy(QComboBox::NoInsert);
    m_urlCombo->addItem(QStringLiteral("rtsp://192.168.1.108:554/onvif1"));
    m_urlCombo->lineEdit()->setPlaceholderText(
        QStringLiteral("rtsp://user:pass@ip:554/путь или rtsp://ip/onvif1"));

    m_userEdit = new QLineEdit;
    m_userEdit->setPlaceholderText(QStringLiteral("логин (admin)"));
    m_passEdit = new QLineEdit;
    m_passEdit->setPlaceholderText(QStringLiteral("пароль"));
    m_passEdit->setEchoMode(QLineEdit::Password);

    QPushButton *addBtn = new QPushButton(QStringLiteral("Подключить выбранный поток"));
    connect(addBtn, &QPushButton::clicked, this, [this] {
        const QString url = m_urlCombo->currentText().trimmed();
        if (url.isEmpty())
            return;
        openStreamInCell(url, QUrl(url).host().isEmpty() ? url : QUrl(url).host(),
                         m_userEdit->text().trimmed(), m_passEdit->text());
        // запомнить в истории
        if (m_urlCombo->findText(url) < 0)
            m_urlCombo->insertItem(0, url);
    });

    QPushButton *addCamBtn = new QPushButton(QStringLiteral("▶ Добавить выделенную камеру"));
    connect(addCamBtn, &QPushButton::clicked, this, &MainWindow::addSelectedCamera);

    QPushButton *removeBtn = new QPushButton(QStringLiteral("✖ Отключить выделенный поток"));
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeSelectedStream);

    auto *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(new QLabel(QStringLiteral("<b>Найденные камеры (ONVIF):</b>")));
    leftLayout->addWidget(m_searchBtn);
    leftLayout->addWidget(m_camList, 1);
    leftLayout->addWidget(addCamBtn);
    leftLayout->addSpacing(10);
    leftLayout->addWidget(new QLabel(QStringLiteral("<b>Ручной RTSP-URL:</b>")));
    leftLayout->addWidget(m_urlCombo);
    auto *credRow = new QHBoxLayout;
    credRow->addWidget(m_userEdit);
    credRow->addWidget(m_passEdit);
    leftLayout->addLayout(credRow);
    leftLayout->addWidget(addBtn);
    leftLayout->addWidget(removeBtn);

    QWidget *leftPanel = new QWidget;
    leftPanel->setLayout(leftLayout);

    // --- Правая панель: сетка видео ---
    m_layoutCombo = new QComboBox;
    // Пресеты сетки: data = cols*100 + rows (напр. 306 => 3 колонки x 6 рядов)
    m_layoutCombo->addItem(QStringLiteral("1x1"), 101);
    m_layoutCombo->addItem(QStringLiteral("2x2"), 202);
    m_layoutCombo->addItem(QStringLiteral("3x3"), 303);
    m_layoutCombo->addItem(QStringLiteral("4x4"), 404);
    m_layoutCombo->addItem(QStringLiteral("3x6 (18)"), 318);
    m_layoutCombo->addItem(QStringLiteral("6x3 (18)"), 603);
    m_layoutCombo->setCurrentIndex(1);
    connect(m_layoutCombo, QOverload<int>::of(&QComboBox::activated),
            this, &MainWindow::setLayoutPreset);

    m_gridWidget = new QWidget;
    m_grid = new QGridLayout(m_gridWidget);
    m_grid->setSpacing(2);
    m_grid->setContentsMargins(2, 2, 2, 2);

    auto *rightLayout = new QVBoxLayout;
    auto *topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel(QStringLiteral("Сетка:")));
    topRow->addWidget(m_layoutCombo);
    topRow->addStretch();
    rightLayout->addLayout(topRow);
    rightLayout->addWidget(m_gridWidget, 1);

    QWidget *rightPanel = new QWidget;
    rightPanel->setLayout(rightLayout);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({340, 900});
    setCentralWidget(splitter);

    m_statusLabel = new QLabel(QStringLiteral("Готов. Нажмите «Поиск ONVIF» для поиска камер."));
    statusBar()->addPermanentWidget(m_statusLabel);

    // --- Обнаружение ---
    m_discovery = new OnvifDiscovery(this);
    connect(m_discovery, &OnvifDiscovery::cameraFound,
            this, &MainWindow::onCameraFound);
    connect(m_discovery, &OnvifDiscovery::finished,
            this, &MainWindow::onDiscoveryFinished);

    setLayoutPreset(m_layoutCombo->currentIndex());
    resize(1280, 720);
}

MainWindow::~MainWindow()
{
    for (auto &e : m_streams)
        delete e.stream;
}

void MainWindow::startDiscovery()
{
    m_camList->clear();
    m_searchBtn->setEnabled(false);
    m_statusLabel->setText(QStringLiteral("Поиск ONVIF-камер..."));
    m_discovery->start(5000);
}

void MainWindow::onCameraFound(const CameraInfo &info)
{
    auto *item = new QListWidgetItem(info.displayString());
    item->setData(Qt::UserRole, info.address);
    item->setData(Qt::UserRole + 1, info.rtspUrl);
    item->setToolTip(QStringLiteral("ONVIF: %1\nRTSP: %2")
                         .arg(info.xaddr, info.rtspUrl));
    m_camList->addItem(item);
}

void MainWindow::onDiscoveryFinished()
{
    m_searchBtn->setEnabled(true);
    m_statusLabel->setText(QStringLiteral("Найдено камер: %1").arg(m_camList->count()));
    if (m_camList->count() == 0)
        m_statusLabel->setText(QStringLiteral("Камеры не найдены. "
                             "Проверьте сеть/брандмауэр или добавьте RTSP вручную."));
}

void MainWindow::addSelectedCamera()
{
    auto *item = m_camList->currentItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("ONVIF"),
                                 QStringLiteral("Сначала выберите камеру из списка."));
        return;
    }
    const QString ip = item->data(Qt::UserRole).toString();
    const QString rtsp = item->data(Qt::UserRole + 1).toString();

    bool ok = false;
    QString user = QInputDialog::getText(this, QStringLiteral("Доступ к камере"),
                                         QStringLiteral("Логин (OK — без авторизации):"),
                                         QLineEdit::Normal, QStringLiteral("admin"),
                                         &ok);
    if (!ok)
        return;
    QString pass;
    if (!user.isEmpty())
        pass = QInputDialog::getText(this, QStringLiteral("Доступ к камере"),
                                     QStringLiteral("Пароль:"),
                                     QLineEdit::Password, QString(), &ok);
    if (!ok)
        return;

    openStreamInCell(rtsp, ip, user, pass);
}

void MainWindow::addManualStream()
{
    // обрабатывается в лямбде кнопки «Подключить»
}

void MainWindow::openStreamInCell(const QString &url, const QString &title,
                                  const QString &user, const QString &password)
{
    if (m_streams.size() >= MAX_STREAMS) {
        QMessageBox::warning(this, QStringLiteral("Потоки"),
                             QStringLiteral("Максимум %1 потоков.").arg(MAX_STREAMS));
        return;
    }

    int idx = firstFreeCell();
    if (idx < 0) {
        // расширить сетку на строку (не больше MAX_STREAMS ячеек)
        const int currentTotal = m_cols * m_rows;
        if (currentTotal >= MAX_STREAMS) {
            QMessageBox::warning(this, QStringLiteral("Потоки"),
                                 QStringLiteral("Нет свободных ячеек."));
            return;
        }
        const int need = qMin(currentTotal + m_cols, MAX_STREAMS);
        while (m_cells.size() < need)
            createOneCell();
        m_rows = (need + m_cols - 1) / m_cols;
        rebuildGrid();
        idx = firstFreeCell();
    }

    auto *stream = new CameraStream(this);
    connect(stream, &CameraStream::errorOccurred, this, [this, title](const QString &e) {
        m_statusLabel->setText(QStringLiteral("%1: %2").arg(title, e));
    });

    StreamEntry entry;
    entry.stream = stream;
    entry.cell = m_cells[idx];
    entry.url = url;
    entry.title = title;
    m_streams.append(entry);

    entry.cell->attach(stream, title);
    stream->open(url, user, password);
    m_statusLabel->setText(QStringLiteral("Подключаемся к %1 ...").arg(url));
}

int MainWindow::firstFreeCell() const
{
    for (int i = 0; i < m_cells.size(); ++i)
        if (!m_cells[i]->hasStream())
            return i;
    return -1;
}

void MainWindow::removeSelectedStream()
{
    // найти поток в выделенной ячейке; если нет выделения — последний
    VideoCell *target = nullptr;
    for (int i = 0; i < m_cells.size(); ++i)
        if (m_cells[i]->underMouse() && m_cells[i]->hasStream())
            target = m_cells[i];
    if (!target && !m_streams.isEmpty())
        target = m_streams.last().cell;

    for (int i = 0; i < m_streams.size(); ++i) {
        if (m_streams[i].cell == target) {
            m_streams[i].cell->detach();
            m_streams[i].stream->close();
            delete m_streams[i].stream;
            m_streams.removeAt(i);
            break;
        }
    }
    m_statusLabel->setText(QStringLiteral("Активных потоков: %1").arg(m_streams.size()));
}

void MainWindow::createOneCell()
{
    auto *cell = new VideoCell(m_gridWidget);
    connect(cell, &VideoCell::maximizeRequested,
            this, &MainWindow::onCellMaximizeRequested);
    m_cells.append(cell);
}

void MainWindow::setLayoutPreset(int comboIdx)
{
    const int data = m_layoutCombo->itemData(comboIdx).toInt();
    m_cols = data / 100;
    m_rows = data % 100;
    const int total = m_cols * m_rows;

    // создать недостающие ячейки
    while (m_cells.size() < total)
        createOneCell();

    // спрятать лишние (но не удалять — там могут быть потоки)
    for (int i = 0; i < m_cells.size(); ++i)
        m_cells[i]->setVisible(i < total);

    rebuildGrid();
}

void MainWindow::rebuildGrid()
{
    // отвязать все виджеты от layout без удаления
    while (m_grid->count())
        m_grid->takeAt(0);

    const int total = m_cols * m_rows;
    for (int i = 0; i < qMin(total, int(m_cells.size())); ++i) {
        m_cells[i]->setVisible(true);
        m_grid->addWidget(m_cells[i], i / m_cols, i % m_cols);
    }
    for (int i = total; i < m_cells.size(); ++i)
        m_cells[i]->setVisible(false);

    for (int c = 0; c < m_cols; ++c)
        m_grid->setColumnStretch(c, 1);
    for (int r = 0; r < m_rows; ++r)
        m_grid->setRowStretch(r, 1);
}

void MainWindow::onCellMaximizeRequested(VideoCell *cell)
{
    if (m_maximized == cell) {
        // повторный двойной клик — вернуть сетку
        m_maximized = nullptr;
        rebuildGrid();
        return;
    }
    m_maximized = cell;
    while (m_grid->count())
        m_grid->takeAt(0);
    m_grid->addWidget(cell, 0, 0);
    m_grid->setRowStretch(0, 1);
    m_grid->setColumnStretch(0, 1);
}
