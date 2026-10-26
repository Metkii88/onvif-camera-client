#pragma once

#include <QObject>
#include <QThread>
#include <QImage>
#include <QString>
#include <QMutex>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class CameraDecoder;

// Поток, в котором работает FFmpeg (чтение + декодирование вне GUI)
class DecodeThread : public QThread
{
    Q_OBJECT
public:
    explicit DecodeThread(const QString &url, const QString &user, const QString &password,
                          QObject *parent = nullptr);
    void requestStop();

signals:
    // кадр YUV420P передаётся как уже разложенный QImage (плоскости скопированы)
    void frameReady(const QImage &frame);
    void errorOccurred(const QString &msg);
    void connected(int width, int height, QString codecName);

protected:
    void run() override;

private:
    QString m_url;
    QString m_user;
    QString m_password;
    volatile bool m_stop = false;
};

// Обёртка: управляет DecodeThread, предоставляет uniform API для UI
class CameraStream : public QObject
{
    Q_OBJECT
public:
    explicit CameraStream(QObject *parent = nullptr);
    ~CameraStream();

    void open(const QString &url, const QString &user = QString(),
              const QString &password = QString());
    void close();
    bool isOpen() const { return m_thread != nullptr; }
    QString url() const { return m_url; }

signals:
    void frameReady(const QImage &frame);
    void errorOccurred(const QString &msg);
    void connected(int width, int height, const QString &codec);

private:
    QString m_url;
    DecodeThread *m_thread = nullptr;
};
