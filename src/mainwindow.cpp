#include "mainwindow.h"
#include "cameradecoder.h"
#include "videocell.h"
#include "camerascanner.h"

#include <QWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFrame>
#include <QTabWidget>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QApplication>
#include <QSettings>
#include <QUrl>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QCloseEvent>
#include <QTimer>

static const int MAX_STREAMS = 18;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Видеоклиент — ONVIF камеры"));

    // --- Левая панель: обнаруженные камеры ---
    m_camList = new QListWidget;
    m_camList->setAlternatingRowColors(true);
    m_searchBtn = new QPushButton(QStringLiteral("🔍 Поиск ONVIF"));
    m_searchBtn->setObjectName("btnPrimary");
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

    // --- Вкладки: ONVIF-поиск / сохранённые камеры / ручной URL ---
    auto *tabs = new QTabWidget;

    // вкладка 1: обнаружение по multicast
    QWidget *onvifTab = new QWidget;
    auto *onvifLayout = new QVBoxLayout(onvifTab);
    onvifLayout->addWidget(m_searchBtn);
    onvifLayout->addWidget(m_camList, 1);
    onvifLayout->addWidget(addCamBtn);
    tabs->addTab(onvifTab, QStringLiteral("ONVIF (multicast)"));

    // вкладка 2: сохранённые настроенные камеры («предметы»)
    m_savedList = new QListWidget;
    m_savedList->setAlternatingRowColors(true);
    connect(m_savedList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *) { connectSelectedSaved(); });

    QPushButton *newTplBtn = new QPushButton(QStringLiteral("➕ Новый шаблон..."));
    connect(newTplBtn, &QPushButton::clicked, this, &MainWindow::newTemplate);
    QPushButton *scanBtn = new QPushButton(QStringLiteral("📡 Поиск по шаблону (подсеть)..."));
    connect(scanBtn, &QPushButton::clicked, this, &MainWindow::openScanDialog);
    QPushButton *editBtn = new QPushButton(QStringLiteral("✏ Изменить"));
    connect(editBtn, &QPushButton::clicked, this, &MainWindow::editSelectedSaved);
    QPushButton *connBtn = new QPushButton(QStringLiteral("▶ Подключить выбранную"));
    connect(connBtn, &QPushButton::clicked, this, &MainWindow::connectSelectedSaved);
    QPushButton *delBtn = new QPushButton(QStringLiteral("🗑 Удалить из хранилища"));
    connect(delBtn, &QPushButton::clicked, this, &MainWindow::deleteSelectedSaved);

    QWidget *savedTab = new QWidget;
    auto *savedLayout = new QVBoxLayout(savedTab);
    savedLayout->addWidget(newTplBtn);
    savedLayout->addWidget(scanBtn);
    savedLayout->addWidget(m_savedList, 1);
    auto *savedBtnRow = new QHBoxLayout;
    savedBtnRow->addWidget(editBtn);
    savedBtnRow->addWidget(delBtn);
    savedLayout->addLayout(savedBtnRow);
    savedLayout->addWidget(connBtn);
    tabs->addTab(savedTab, QStringLiteral("Сохранённые камеры"));

    // вкладка 3: ручной RTSP
    QWidget *manualTab = new QWidget;
    auto *manualLayout = new QVBoxLayout(manualTab);
    manualLayout->addWidget(new QLabel(QStringLiteral("<b>Ручной RTSP-URL:</b>")));
    manualLayout->addWidget(m_urlCombo);
    auto *credRow = new QHBoxLayout;
    credRow->addWidget(m_userEdit);
    credRow->addWidget(m_passEdit);
    manualLayout->addLayout(credRow);
    manualLayout->addWidget(addBtn);
    QPushButton *saveManualBtn = new QPushButton(QStringLiteral("💾 Сохранить в хранилище"));
    connect(saveManualBtn, &QPushButton::clicked, this, [this] {
        CameraTemplate t;
        t.useRtspUrl = true;
        t.rtspUrl = m_urlCombo->currentText().trimmed();
        t.user = m_userEdit->text().trimmed();
        t.password = m_passEdit->text();
        if (t.rtspUrl.isEmpty()) return;
        bool ok = false;
        t.name = QInputDialog::getText(this, QStringLiteral("Имя камеры"),
                                       QStringLiteral("Название:"), QLineEdit::Normal,
                                       QUrl(t.rtspUrl).host(), &ok);
        if (!ok || t.name.isEmpty()) return;
        CameraStore::addOrUpdate(t.toSaved());
        refreshSavedList();
        m_statusLabel->setText(QStringLiteral("Камера «%1» сохранена.").arg(t.name));
    });
    manualLayout->addWidget(saveManualBtn);
    manualLayout->addWidget(removeBtn);
    manualLayout->addStretch();
    tabs->addTab(manualTab, QStringLiteral("Ручной URL"));

    // вкладка 4: профили сетки (сохранённые наборы камер + раскладка)
    m_profileList = new QListWidget;
    m_profileList->setAlternatingRowColors(true);
    connect(m_profileList, &QListWidget::itemDoubleClicked,
            this, &MainWindow::onProfileDoubleClicked);

    QPushButton *saveProfBtn = new QPushButton(QStringLiteral("💾 Сохранить сетку как профиль..."));
    saveProfBtn->setToolTip(QStringLiteral(
        "Запомнить все подключённые камеры и их позиции в сетке.\n"
        "Двойной клик по профилю — загрузить сетку."));
    connect(saveProfBtn, &QPushButton::clicked, this, &MainWindow::saveGridAsProfile);
    QPushButton *loadProfBtn = new QPushButton(QStringLiteral("▶ Загрузить выбранный профиль"));
    connect(loadProfBtn, &QPushButton::clicked, this, &MainWindow::loadSelectedProfile);
    QPushButton *renameProfBtn = new QPushButton(QStringLiteral("✏ Переименовать"));
    connect(renameProfBtn, &QPushButton::clicked, this, &MainWindow::renameCurrentProfile);
    QPushButton *delProfBtn = new QPushButton(QStringLiteral("🗑 Удалить профиль"));
    connect(delProfBtn, &QPushButton::clicked, this, &MainWindow::deleteSelectedProfile);

    QPushButton *startFsProfBtn = new QPushButton(
        QStringLiteral("▶▶ Запустить выбранный профиль в полном экране"));
    startFsProfBtn->setToolTip(QStringLiteral(
        "Загрузить сетку из профиля и сразу развернуть её на весь экран (F11 — выход)."));
    connect(startFsProfBtn, &QPushButton::clicked, this, [this] {
        loadSelectedProfile();
        if (!m_gridFullscreen)
            toggleGridFullscreen();
    });

    QWidget *profTab = new QWidget;
    auto *profLayout = new QVBoxLayout(profTab);
    profLayout->addWidget(saveProfBtn);
    profLayout->addWidget(m_profileList, 1);
    auto *profBtnRow = new QHBoxLayout;
    profBtnRow->addWidget(renameProfBtn);
    profBtnRow->addWidget(delProfBtn);
    profLayout->addLayout(profBtnRow);
    profLayout->addWidget(loadProfBtn);
    profLayout->addWidget(startFsProfBtn);
    tabs->addTab(profTab, QStringLiteral("Профили"));

    auto *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(tabs);

    m_leftPanel = new QWidget;
    m_leftPanel->setLayout(leftLayout);

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

    // дебаунс-таймер: после завершения перетаскивания окна перестроить сетку
    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(250);
    connect(m_resizeTimer, &QTimer::timeout, this, [this] {
        if (!m_gridWidget)
            return;
        const QSize sz = m_gridWidget->size();
        const bool changed = (sz != m_lastGridSize);
        m_lastGridSize = sz;
        if (!changed)
            return;                       // размер не менялся — ничего делать не нужно

        rebuildGrid();                    // ячейки заново распределятся по новому размеру

        // Перезапуск декодеров делаем только при РЕАЛЬНОЙ смене размера сетки
        // (ресайз/разворот окна). При простом перемещении окна size не меняется,
        // сюда мы не доходим — потоки не дёргаются.
        for (VideoCell *cell : std::as_const(m_cells)) {
            if (cell->hasStream() && cell->isVisible())
                cell->restartStream();
        }
    });

    m_gridWidget = new QWidget;
    m_gridWidget->setObjectName("gridRoot");   // в FS-режиме: фон без рамки (см. style.qss)
    m_grid = new QGridLayout(m_gridWidget);
    m_grid->setSpacing(2);
    m_grid->setContentsMargins(2, 2, 2, 2);

    auto *rightLayout = new QVBoxLayout;
    rightLayout->setContentsMargins(4, 4, 4, 4);
    m_gridToolbar = new QWidget;                 // прячется в полноэкранном режиме сетки
    auto *topRow = new QHBoxLayout(m_gridToolbar);
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addWidget(new QLabel(QStringLiteral("Сетка:")));
    topRow->addWidget(m_layoutCombo);
    QPushButton *fsBtn = new QPushButton(QStringLiteral("⛶ Только сетка на весь экран (F11)"));
    fsBtn->setToolTip(QStringLiteral(
        "Развернуть на весь экран ТОЛЬКО сетку из камер:\n"
        "панель управления, шапка и строка состояния скрываются.\n"
        "Выход — F11 или Esc."));
    connect(fsBtn, &QPushButton::clicked, this, &MainWindow::toggleGridFullscreen);
    topRow->addWidget(fsBtn);
    topRow->addStretch();
    rightLayout->addWidget(m_gridToolbar);
    rightLayout->addWidget(m_gridWidget, 1);

    m_rightPanel = new QWidget;
    m_rightPanel->setLayout(rightLayout);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(m_leftPanel);
    splitter->addWidget(m_rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({340, 900});
    setCentralWidget(splitter);

    m_statusLabel = new QLabel(QStringLiteral("Готов. Нажмите «Поиск ONVIF» для поиска камер."));
    statusBar()->addPermanentWidget(m_statusLabel);

    // --- Восстановление последнего состояния (размер/позиция окна, профиль) ---
    {
        QSettings st;
        const QByteArray geom = st.value("window/geometry").toByteArray();
        if (!geom.isEmpty())
            restoreGeometry(geom);
        else
            resize(1280, 720);
        const int preset = st.value("window/layoutPreset", 202).toInt();
        for (int i = 0; i < m_layoutCombo->count(); ++i) {
            if (m_layoutCombo->itemData(i).toInt() == preset) {
                m_layoutCombo->setCurrentIndex(i);
                setLayoutPreset(i);
                break;
            }
        }
        m_currentProfile = st.value("profile/current").toString();
    }

    refreshProfileList();

    // --- Верхняя шапка с названием приложения ---
    {
        m_header = new QFrame;
        m_header->setObjectName("appHeader");
        m_header->setStyleSheet("QFrame#appHeader{background:#171b22;border-bottom:1px solid #3a4250;}");
        auto *hl = new QHBoxLayout(m_header);
        hl->setContentsMargins(14, 8, 14, 8);
        auto *title = new QLabel(QStringLiteral(
            "<span style='font-size:22px;'>📷</span>  "
            "<span style='font-size:16px;font-weight:bold;color:#e8eef5;'>Камеры</span>"
            "<span style='font-size:12px;color:#8fa2ba;'>  ·  ONVIF клиент v2.2</span>"));
        title->setTextFormat(Qt::RichText);
        hl->addWidget(title);
        hl->addStretch();
        // centralWidget — QSplitter, поэтому оборачиваем его в свой макет
        QWidget *root = new QWidget;
        auto *rootLay = new QVBoxLayout(root);
        rootLay->setContentsMargins(0, 0, 0, 0);
        rootLay->setSpacing(0);
        splitter->setParent(nullptr);
        rootLay->addWidget(m_header);
        rootLay->addWidget(splitter, 1);
        setCentralWidget(root);
    }

    // --- Обнаружение ---
    m_discovery = new OnvifDiscovery(this);
    connect(m_discovery, &OnvifDiscovery::cameraFound,
            this, &MainWindow::onCameraFound);
    connect(m_discovery, &OnvifDiscovery::finished,
            this, &MainWindow::onDiscoveryFinished);

    setLayoutPreset(m_layoutCombo->currentIndex());
    resize(1280, 720);

    refreshSavedList();
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
                                  const QString &user, const QString &password,
                                  int cellIndex)
{
    if (m_streams.size() >= MAX_STREAMS) {
        QMessageBox::warning(this, QStringLiteral("Потоки"),
                             QStringLiteral("Максимум %1 потоков.").arg(MAX_STREAMS));
        return;
    }

    // явный слот профиля: освободить место и создать недостающие ячейки
    if (cellIndex >= 0) {
        for (int i = 0; i < m_streams.size(); ++i) {
            if (m_streams[i].cell == m_cells.value(cellIndex)) {
                m_streams[i].cell->detach();
                m_streams[i].stream->close();
                delete m_streams[i].stream;
                m_streams.removeAt(i);
                break;
            }
        }
        while (m_cells.size() <= cellIndex)
            createOneCell();
        if (cellIndex >= m_cols * m_rows) {
            m_rows = (cellIndex + m_cols) / m_cols;   // расширить сетку рядами
            rebuildGrid();
        }
    } else {
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
        cellIndex = idx;
    }

    auto *stream = new CameraStream(this);
    connect(stream, &CameraStream::errorOccurred, this, [this, title](const QString &e) {
        m_statusLabel->setText(QStringLiteral("%1: %2").arg(title, e));
    });

    StreamEntry entry;
    entry.stream = stream;
    entry.cell = m_cells[cellIndex];
    entry.url = url;
    entry.title = title;
    m_streams.append(entry);

    entry.cell->attach(stream, title);
    entry.cell->setStreamCredentials(user, password);
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

    // Принудительно пересчитать геометрию — ячейки сразу получают новый размер.
    if (m_gridWidget && m_gridWidget->isVisible())
        m_gridWidget->layout()->activate();
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

// ---------- Шаблоны и хранилище ----------

void MainWindow::refreshSavedList()
{
    if (!m_savedList)
        return;
    m_savedList->clear();
    const QList<SavedCamera> list = CameraStore::load();
    for (const SavedCamera &c : list) {
        auto *item = new QListWidgetItem(c.displayString());
        item->setToolTip(QStringLiteral("Имя: %1\nURL: %2\nЛогин: %3")
                             .arg(c.name, c.url(), c.user));
        m_savedList->addItem(item);
    }
}

void MainWindow::newTemplate()
{
    CameraDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const CameraTemplate t = dlg.templateValue();
    const SavedCamera c = t.toSaved();
    if (!CameraStore::addOrUpdate(c)) {
        QMessageBox::warning(this, QStringLiteral("Хранилище"),
                             QStringLiteral("Не удалось сохранить камеру."));
        return;
    }
    refreshSavedList();
    m_statusLabel->setText(QStringLiteral("Шаблон «%1» сохранён.").arg(c.name));
    connectSaved(c);
}

void MainWindow::editSelectedSaved()
{
    auto *item = m_savedList->currentItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("Хранилище"),
                                 QStringLiteral("Выберите камеру в списке."));
        return;
    }
    const int idx = m_savedList->row(item);
    const QList<SavedCamera> list = CameraStore::load();
    if (idx < 0 || idx >= list.size())
        return;

    CameraDialog dlg(this);
    dlg.setTemplate(CameraTemplate::fromSaved(list[idx]));
    if (dlg.exec() != QDialog::Accepted)
        return;
    const SavedCamera updated = dlg.templateValue().toSaved();
    QList<SavedCamera> mod = list;
    mod[idx] = updated;
    CameraStore::save(mod);
    refreshSavedList();
    m_statusLabel->setText(QStringLiteral("Камера «%1» обновлена.").arg(updated.name));
}

void MainWindow::deleteSelectedSaved()
{
    auto *item = m_savedList->currentItem();
    if (!item)
        return;
    const int idx = m_savedList->row(item);
    if (QMessageBox::question(this, QStringLiteral("Хранилище"),
                              QStringLiteral("Удалить «%1» из сохранённых?").arg(item->text()))
        != QMessageBox::Yes)
        return;
    CameraStore::removeAt(idx);
    refreshSavedList();
}

void MainWindow::connectSelectedSaved()
{
    auto *item = m_savedList->currentItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("Хранилище"),
                                 QStringLiteral("Выберите камеру в списке "
                                                "(двойной клик — быстрое подключение)."));
        return;
    }
    const int idx = m_savedList->row(item);
    const QList<SavedCamera> list = CameraStore::load();
    if (idx < 0 || idx >= list.size())
        return;
    connectSaved(list[idx]);
}

void MainWindow::connectSaved(const SavedCamera &c)
{
    openStreamInCell(c.url(), c.name.isEmpty() ? c.ip : c.name, c.user, c.password);
}

void MainWindow::openScanDialog()
{
    CameraTemplate tpl;
    // если есть выделенная сохранённая камера — взять её параметры как основу
    auto *item = m_savedList->currentItem();
    const QList<SavedCamera> list = CameraStore::load();
    if (item && m_savedList->row(item) < list.size()) {
        tpl = CameraTemplate::fromSaved(list[m_savedList->row(item)]);
    } else {
        // шаблон по умолчанию: 192.168.0.1-254:554, h264, rtsp, admin/admin
        tpl.ip       = QStringLiteral("192.168.0.1-254");
        tpl.port     = 554;
        tpl.user     = QStringLiteral("admin");
        tpl.password = QStringLiteral("admin");
        tpl.codec    = QStringLiteral("h264");
        tpl.path     = QStringLiteral("/onvif1");
    }

    // Если в имени/IP сохранённой камеры записан диапазон (192.168.0.1-254) —
    // parseIpRange в конструкторе сканера разберёт его и подставит в поля.
    CameraScanner sc(tpl, this);
    sc.exec();
    refreshSavedList(); // сканер мог что-то сохранить
}

// ---------- Полный экран ----------

void MainWindow::toggleGridFullscreen()
{
    m_gridFullscreen = !m_gridFullscreen;

    if (m_gridFullscreen) {
        // спрятать шапку/левую панель/тулбар — сетка остаётся одна на весь экран
        m_header->setVisible(false);
        m_leftPanel->setVisible(false);
        m_gridToolbar->setVisible(false);
        statusBar()->hide();
        showFullScreen();
    } else {
        showNormal();
        m_header->setVisible(true);
        m_leftPanel->setVisible(true);
        m_gridToolbar->setVisible(true);
        statusBar()->show();
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::keyPressEvent(QKeyEvent *ev)
{
    if (ev->key() == Qt::Key_F11) {
        toggleGridFullscreen();
        return;
    }
    if (ev->key() == Qt::Key_Escape && m_gridFullscreen) {
        toggleGridFullscreen();
        return;
    }
    QMainWindow::keyPressEvent(ev);
}

// ---------- Запуск с набором (профилем) ----------

void MainWindow::startProfile(const QStringList &names, bool fullscreen)
{
    const QList<SavedCamera> list = CameraStore::load();
    int connected = 0;
    for (const QString &want : names) {
        for (const SavedCamera &c : list) {
            if (c.name.compare(want, Qt::CaseInsensitive) == 0 || c.ip == want) {
                connectSaved(c);
                ++connected;
                break;
            }
        }
    }
    if (connected == 0)
        m_statusLabel->setText(QStringLiteral("Профиль не найден: %1")
                                   .arg(names.join(", ")));
    else
        m_statusLabel->setText(QStringLiteral("Профиль: подключено камер — %1").arg(connected));

    if (fullscreen) {
        // оптимальная сетка под количество потоков
        const int n = qMin(connected, MAX_STREAMS);
        int data = 101;               // колонки*100 + ряды
        if (n > 1)  data = 202;
        if (n > 4)  data = 303;
        if (n > 9)  data = 404;
        if (n > 16) data = 306;       // 17-18 потоков -> пресет 3x6
        for (int i = 0; i < m_layoutCombo->count(); ++i)
            if (m_layoutCombo->itemData(i).toInt() == data) {
                setLayoutPreset(i);
                m_layoutCombo->setCurrentIndex(i);
                break;
            }
        QMetaObject::invokeMethod(this, [this] { showFullScreen(); },
                                  Qt::QueuedConnection);
    }
}

// ---------- События окна: ресайз / показ / закрытие ----------

void MainWindow::resizeEvent(QResizeEvent *ev)
{
    QMainWindow::resizeEvent(ev);
    // Дебаунс: при ПЕРЕМЕЩЕНИИ окна size не меняется — по истечении таймера
    // изменения не обнаружится и сетку не тронем. При реальном ресайзе или
    // входе/выходе из полноэкранного режима (FS-сетка тоже масштабируется)
    // сетка будет перестроена один раз после завершения изменения размера.
    if (m_resizeTimer)
        m_resizeTimer->start();
}

void MainWindow::showEvent(QShowEvent *ev)
{
    QMainWindow::showEvent(ev);
    if (m_shownOnce)
        return;
    m_shownOnce = true;

    // восстановить последнюю сетку из профиля (только если ещё нет потоков)
    if (!m_currentProfile.isEmpty() && m_streams.isEmpty()) {
        const QList<CameraProfile> profiles = ProfileStore::load();
        for (const CameraProfile &p : profiles) {
            if (p.name.compare(m_currentProfile, Qt::CaseInsensitive) == 0) {
                applyProfile(p);
                break;
            }
        }
    }

    if (m_pendingFsStart && !m_gridFullscreen)
        toggleGridFullscreen();          // --fullscreen: разворот после показа

    m_lastGridSize = m_gridWidget ? m_gridWidget->size() : QSize();
}

void MainWindow::closeEvent(QCloseEvent *ev)
{
    QSettings st;
    st.setValue("window/geometry", saveGeometry());
    st.setValue("window/layoutPreset",
                m_layoutCombo ? m_layoutCombo->currentData().toInt() : 202);
    st.setValue("profile/current", m_currentProfile);
    QMainWindow::closeEvent(ev);
}

// ---------- Профили сетки ----------

QString MainWindow::findSavedNameByUrl(const QString &url) const
{
    const QList<SavedCamera> list = CameraStore::load();
    for (const SavedCamera &c : list) {
        if (c.url().compare(url, Qt::CaseInsensitive) == 0)
            return c.name.isEmpty() ? c.ip : c.name;
    }
    return QString();
}

void MainWindow::closeAllStreams()
{
    for (StreamEntry &e : m_streams) {
        if (e.cell)
            e.cell->detach();
        if (e.stream) {
            e.stream->close();
            delete e.stream;
        }
    }
    m_streams.clear();
}

void MainWindow::applyProfile(const CameraProfile &p)
{
    closeAllStreams();

    // раскладка из профиля
    m_cols = qMax(1, p.cols);
    m_rows = (p.entries.size() + m_cols - 1) / m_cols;
    if (m_rows < 1)
        m_rows = 1;
    const int total = qMin(m_cols * m_rows, MAX_STREAMS);
    while (m_cells.size() < total)
        createOneCell();
    for (int i = 0; i < m_cells.size(); ++i)
        m_cells[i]->setVisible(i < total);
    // подсветить пресет в комбобоксе, если есть совпадение
    if (m_layoutCombo) {
        const int data = m_cols * 100 + m_rows;
        for (int i = 0; i < m_layoutCombo->count(); ++i) {
            if (m_layoutCombo->itemData(i).toInt() == data) {
                m_layoutCombo->blockSignals(true);
                m_layoutCombo->setCurrentIndex(i);
                m_layoutCombo->blockSignals(false);
                break;
            }
        }
    }
    rebuildGrid();

    const QList<SavedCamera> saved = CameraStore::load();
    for (const ProfileEntry &e : p.entries) {
        QString url = e.url, user = e.user, pass = e.password, title = e.title;
        // приоритет у сохранённой камеры: актуальные реквизиты из хранилища
        if (!e.camName.isEmpty()) {
            for (const SavedCamera &c : saved) {
                if ((c.name.isEmpty() ? c.ip : c.name).compare(e.camName, Qt::CaseInsensitive) == 0
                    || c.name.compare(e.camName, Qt::CaseInsensitive) == 0) {
                    url = c.url();
                    user = c.user;
                    pass = c.password;
                    if (title.isEmpty())
                        title = c.name.isEmpty() ? c.ip : c.name;
                    break;
                }
            }
        }
        if (url.isEmpty())
            continue;
        if (title.isEmpty()) {
            const QString host = QUrl(url).host();
            title = host.isEmpty() ? url : host;
        }
        openStreamInCell(url, title, user, pass, e.cellIndex);
    }

    m_currentProfile = p.name;
    QSettings().setValue("profile/current", m_currentProfile);
    refreshProfileList();
    m_statusLabel->setText(QStringLiteral("Профиль «%1»: камер в сетке — %2")
                               .arg(p.name).arg(p.entries.size()));
}

void MainWindow::refreshProfileList()
{
    if (!m_profileList)
        return;
    const QString sel = m_profileList->currentItem()
                            ? m_profileList->currentItem()->text() : QString();
    m_profileList->clear();
    const QList<CameraProfile> profiles = ProfileStore::load();
    for (const CameraProfile &p : profiles) {
        auto *item = new QListWidgetItem(QStringLiteral("%1  (%2)")
                                             .arg(p.name)
                                             .arg(p.entries.size()));
        item->setToolTip(QStringLiteral("Камер: %1 · Колонки: %2\n"
                                        "Двойной клик — загрузить профиль")
                             .arg(p.entries.size()).arg(p.cols));
        item->setData(Qt::UserRole, p.name);
        if (p.name.compare(m_currentProfile, Qt::CaseInsensitive) == 0) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
            item->setText(QStringLiteral("● %1").arg(item->text()));
        }
        m_profileList->addItem(item);
        if (!sel.isEmpty() && p.name == sel)
            m_profileList->setCurrentItem(item);
    }
}

void MainWindow::saveGridAsProfile()
{
    if (m_streams.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Профили"),
                                 QStringLiteral("В сетке нет подключённых камер — "
                                                "сначала добавьте потоки."));
        return;
    }

    bool ok = false;
    QString def = m_currentProfile.isEmpty()
                      ? QStringLiteral("Профиль %1").arg(ProfileStore::load().size() + 1)
                      : m_currentProfile;
    const QString name = QInputDialog::getText(this, QStringLiteral("Сохранить профиль"),
                                               QStringLiteral("Имя профиля:"),
                                               QLineEdit::Normal, def, &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    CameraProfile p;
    p.name = name;
    p.cols = m_cols;
    for (const StreamEntry &e : m_streams) {
        if (!e.cell)
            continue;
        ProfileEntry pe;
        pe.cellIndex = m_cells.indexOf(e.cell);
        pe.url = e.url;
        pe.title = e.title;
        pe.camName = findSavedNameByUrl(e.url);
        // логин/пароль берём из ячейки (в Cell они уже заданы через setStreamCredentials)
        pe.user.clear();
        pe.password.clear();
        const QList<SavedCamera> saved = CameraStore::load();
        for (const SavedCamera &c : saved) {
            if (c.url().compare(e.url, Qt::CaseInsensitive) == 0) {
                pe.user = c.user;
                pe.password = c.password;
                break;
            }
        }
        p.entries.append(pe);
    }

    if (!ProfileStore::addOrUpdate(p)) {
        QMessageBox::warning(this, QStringLiteral("Профили"),
                             QStringLiteral("Не удалось сохранить профиль."));
        return;
    }
    m_currentProfile = name;
    QSettings().setValue("profile/current", name);
    refreshProfileList();
    m_statusLabel->setText(QStringLiteral("Профиль «%1» сохранен (%2 камер)")
                               .arg(name).arg(p.entries.size()));
}

QListWidgetItem *MainWindow::selectedProfileItem() const
{
    return m_profileList ? m_profileList->currentItem() : nullptr;
}

void MainWindow::loadSelectedProfile()
{
    QListWidgetItem *item = selectedProfileItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("Профили"),
                                 QStringLiteral("Выберите профиль в списке."));
        return;
    }
    const QString name = item->data(Qt::UserRole).toString();
    const QList<CameraProfile> profiles = ProfileStore::load();
    for (const CameraProfile &p : profiles) {
        if (p.name.compare(name, Qt::CaseInsensitive) == 0) {
            applyProfile(p);
            return;
        }
    }
    QMessageBox::warning(this, QStringLiteral("Профили"),
                         QStringLiteral("Профиль «%1» не найден в хранилище.").arg(name));
    refreshProfileList();
}

void MainWindow::renameCurrentProfile()
{
    QListWidgetItem *item = selectedProfileItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("Профили"),
                                 QStringLiteral("Выберите профиль в списке."));
        return;
    }
    const QString oldName = item->data(Qt::UserRole).toString();
    bool ok = false;
    const QString newName = QInputDialog::getText(this, QStringLiteral("Переименовать профиль"),
                                                  QStringLiteral("Новое имя:"),
                                                  QLineEdit::Normal, oldName, &ok).trimmed();
    if (!ok || newName.isEmpty() || newName == oldName)
        return;
    if (!ProfileStore::rename(oldName, newName)) {
        QMessageBox::warning(this, QStringLiteral("Профили"),
                             QStringLiteral("Не удалось переименовать профиль."));
        return;
    }
    if (m_currentProfile == oldName) {
        m_currentProfile = newName;
        QSettings().setValue("profile/current", newName);
    }
    refreshProfileList();
}

void MainWindow::deleteSelectedProfile()
{
    QListWidgetItem *item = selectedProfileItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("Профили"),
                                 QStringLiteral("Выберите профиль в списке."));
        return;
    }
    const QString name = item->data(Qt::UserRole).toString();
    const auto ret = QMessageBox::question(
        this, QStringLiteral("Профили"),
        QStringLiteral("Удалить профиль «%1»? Камеры в сетке останутся.").arg(name));
    if (ret != QMessageBox::Yes)
        return;
    ProfileStore::removeByName(name);
    if (m_currentProfile == name) {
        m_currentProfile.clear();
        QSettings().setValue("profile/current", QString());
    }
    refreshProfileList();
}

void MainWindow::onProfileDoubleClicked()
{
    loadSelectedProfile();
}

