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
class QPushButton;
class QLabel;
class QGridLayout;
class QWidget;
class QKeyEvent;

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
                          const QString &user, const QString &password);
    int firstFreeCell() const;
    void refreshSavedList();
    void connectSaved(const SavedCamera &c);
    void toggleFullscreen();

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

    OnvifDiscovery *m_discovery = nullptr;
    QList<StreamEntry> m_streams;         // активные потоки (max 18)
    QVector<VideoCell *> m_cells;         // ячейки сетки
    int m_cols = 2;                       // колонок в сетке
    int m_rows = 2;                       // рядов в сетке
    VideoCell *m_maximized = nullptr;     // развёрнутая ячейка (двойной клик)
};
