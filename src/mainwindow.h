#pragma once

#include <QMainWindow>
#include <QList>
#include "onvifdiscovery.h"
#include "camerastore.h"
#include "cameradialog.h"

class VideoCell;
class CameraStream;
class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;
class QGridLayout;
class QWidget;
class QKeyEvent;
class QFrame;
class QResizeEvent;
class QShowEvent;
class QTimer;

// Главное окно клиента камер
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // запуск с сохранённым набором (имена из хранилища) — для --profile/--fullscreen
    void startProfile(const QStringList &names, bool fullscreen);

protected:
    void keyPressEvent(QKeyEvent *ev) override;
    void resizeEvent(QResizeEvent *ev) override;   // перестроить сетку при изменении размера окна
    void showEvent(QShowEvent *ev) override;       // применить профиль, если окно ещё не показано
    void closeEvent(QCloseEvent *ev) override;     // сохранить геометрию/профиль при выходе

private slots:
    // ONVIF-обнаружение
    void startDiscovery();
    void onCameraFound(const CameraInfo &info);
    void onDiscoveryFinished();

    // Управление потоками
    void addSelectedCamera();
    void addManualStream();
    void removeSelectedStream();
    void setLayoutPreset(int cols);
    void onCellMaximizeRequested(VideoCell *cell);

    // Шаблоны / хранилище
    void newTemplate();
    void editSelectedSaved();
    void connectSelectedSaved();
    void deleteSelectedSaved();
    void openScanDialog();

    // Профили сетки (наборы камер + раскладка)
    void saveGridAsProfile();
    void loadSelectedProfile();
    void renameCurrentProfile();
    void deleteSelectedProfile();
    void refreshProfileList();
    void onProfileDoubleClicked();

private:
    struct StreamEntry {
        CameraStream *stream = nullptr;
        VideoCell *cell = nullptr;
        QString url;
        QString title;
    };

    void rebuildGrid();
    void createOneCell();
    void openStreamInCell(const QString &url, const QString &title,
                          const QString &user, const QString &password,
                          int cellIndex = -1);
    void closeAllStreams();
    void applyProfile(const CameraProfile &p);
    QString findSavedNameByUrl(const QString &url) const;
    QListWidgetItem *selectedProfileItem() const;
    int firstFreeCell() const;
    void refreshSavedList();
    void connectSaved(const SavedCamera &c);
    void toggleGridFullscreen();   // полный экран ТОЛЬКО для сетки камер
    bool eventFilter(QObject *obj, QEvent *ev) override;

    // UI
    QListWidget *m_camList = nullptr;     // найденные камеры (ONVIF multicast)
    QListWidget *m_savedList = nullptr;   // сохранённые настроенные камеры
    QComboBox *m_urlCombo = nullptr;      // ручной RTSP URL
    QLineEdit *m_userEdit = nullptr;
    QLineEdit *m_passEdit = nullptr;
    QComboBox *m_layoutCombo = nullptr;
    QPushButton *m_searchBtn = nullptr;
    QLabel *m_statusLabel = nullptr;
    QWidget *m_gridWidget = nullptr;
    QGridLayout *m_grid = nullptr;
    QWidget *m_rightPanel = nullptr;      // панель сетки (для полноэкранного режима)
    QWidget *m_leftPanel = nullptr;       // панель управления (прячется в FS)
    QFrame  *m_header = nullptr;          // шапка приложения (прячется в FS)
    QWidget *m_gridToolbar = nullptr;     // строка «Сетка: … / Полный экран» (прячется в FS)
    bool m_gridFullscreen = false;

    // Профили сетки
    QListWidget *m_profileList = nullptr;
    QString m_currentProfile;             // имя загруженного профиля (если есть)
    bool m_shownOnce = false;
    bool m_pendingFsStart = false;        // --fullscreen до показа окна
    QTimer *m_resizeTimer = nullptr;      // дебаунс перестроения сетки при ресайзе
    QSize m_lastGridSize;                 // последний размер сетки (для детекта реального ресайза)

    OnvifDiscovery *m_discovery = nullptr;
    QList<StreamEntry> m_streams;         // активные потоки (max 18)
    QVector<VideoCell *> m_cells;         // ячейки сетки
    int m_cols = 2;                       // колонок в сетке
    int m_rows = 2;                       // рядов в сетке
    VideoCell *m_maximized = nullptr;     // развёрнутая ячейка (двойной клик)
};
