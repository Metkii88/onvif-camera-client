#pragma once

#include <QWidget>
#include <QImage>
#include <QElapsedTimer>
#include "cameradecoder.h"

// Одна ячейка сетки: отображает видеопоток камеры
class VideoCell : public QWidget
{
    Q_OBJECT
public:
    explicit VideoCell(QWidget *parent = nullptr);

    void attach(CameraStream *stream, const QString &title);
    void setStreamCredentials(const QString &user, const QString &password);
    void detach();
    bool hasStream() const { return m_stream != nullptr; }
    // перезапустить текущий поток (после смены размера ячейки — чтобы кадры
    // масштабировались на новый размер, а не оставались в старом)
    void restartStream();

    // Двойной клик по ячейке -> запрос на разворот во весь экран
signals:
    void maximizeRequested(VideoCell *self);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private slots:
    void onFrame(const QImage &img);
    void onError(const QString &msg);
    void onConnected(int w, int h, const QString &codec);

private:
    CameraStream *m_stream = nullptr;
    QImage m_last;          // последний кадр (уже глубокая копия)
    QString m_title;
    QString m_status = QStringLiteral("нет потока");
    QElapsedTimer m_fpsTimer;
    int m_frames = 0;
    double m_fps = 0.0;
    // параметры текущего потока для перезапуска после ресайза
    QString m_url;
    QString m_user;
    QString m_password;
};
