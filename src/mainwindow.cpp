#include "mainwindow.h"
#include "cameradecoder.h"
#include "videocell.h"
#include "camerascanner.h"

#include <QWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
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
#include <QUrl>
#include <QKeyEvent>

static const int MAX_STREAMS = 16;

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

    auto *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(tabs);

    QWidget *leftPanel = new QWidget;
    leftPanel->setLayout(leftLayout);

    // --- Правая панель: сетка видео ---
    m_layoutCombo = new QComboBox;
    m_layoutCombo->addItem(QStringLiteral("1x1"), 1);
    m_layoutCombo->addItem(QStringLiteral("2x2"), 2);
    m_layoutCombo->addItem(QStringLiteral("3x3"), 3);
    m_layoutCombo->addItem(QStringLiteral("4x4"), 4);
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
    QPushButton *fsBtn = new QPushButton(QStringLiteral("⛶ Полный экран (F11)"));
    connect(fsBtn, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);
    topRow->addWidget(fsBtn);
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

    setLayoutPreset(2);
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
                                  const QString &user, const QString &password)
{
    if (m_streams.size() >= MAX_STREAMS) {
        QMessageBox::warning(this, QStringLiteral("Потоки"),
                             QStringLiteral("Максимум %1 потоков.").arg(MAX_STREAMS));
        return;
    }

    int idx = firstFreeCell();
    if (idx < 0) {
        // расширить сетку на строку
        if (m_cells.size() + m_cols > MAX_STREAMS) {
            QMessageBox::warning(this, QStringLiteral("Потоки"),
                                 QStringLiteral("Нет свободных ячеек."));
            return;
        }
        while (m_cells.size() < size_t((m_cells.size() / m_cols + 1) * m_cols))
            createOneCell();
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
    m_cols = m_layoutCombo->itemData(comboIdx).toInt();
    const int total = m_cols * m_cols;

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

    const int total = m_cols * m_cols;
    for (int i = 0; i < qMin(total, int(m_cells.size())); ++i) {
        m_cells[i]->setVisible(true);
        m_grid->addWidget(m_cells[i], i / m_cols, i % m_cols);
    }
    for (int i = total; i < m_cells.size(); ++i)
        m_cells[i]->setVisible(false);

    for (int c = 0; c < m_cols; ++c)
        m_grid->setColumnStretch(c, 1);
    for (int r = 0; r < m_cols; ++r)
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
    if (item && m_savedList->row(item) < list.size())
        tpl = CameraTemplate::fromSaved(list[m_savedList->row(item)]);
    else {
        tpl.ip = QStringLiteral("192.168.1");
        tpl.user = QStringLiteral("admin");
    }

    CameraScanner sc(tpl, this);
    sc.exec();
    refreshSavedList(); // сканер мог что-то сохранить
}

// ---------- Полный экран ----------

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
        statusBar()->show();
    } else {
        showFullScreen();
        statusBar()->hide();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *ev)
{
    if (ev->key() == Qt::Key_F11) {
        toggleFullscreen();
        return;
    }
    if (ev->key() == Qt::Key_Escape && isFullScreen()) {
        toggleFullscreen();
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
        int cols = 1;
        if (n > 1)  cols = 2;
        if (n > 4)  cols = 3;
        if (n > 9)  cols = 4;
        for (int i = 0; i < m_layoutCombo->count(); ++i)
            if (m_layoutCombo->itemData(i).toInt() == cols) {
                setLayoutPreset(i);
                m_layoutCombo->setCurrentIndex(i);
                break;
            }
        QMetaObject::invokeMethod(this, [this] { showFullScreen(); },
                                  Qt::QueuedConnection);
    }
}
