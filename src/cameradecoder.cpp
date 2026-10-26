#include "cameradecoder.h"

#include <QMutexLocker>
#include <QDebug>
#include <cstring>

DecodeThread::DecodeThread(const QString &url, const QString &user, const QString &password,
                           QObject *parent)
    : QThread(parent), m_url(url), m_user(user), m_password(password)
{
}

void DecodeThread::requestStop()
{
    m_stop = true;
}

void DecodeThread::run()
{
    AVFormatContext *fmtCtx = nullptr;
    AVCodecContext *codecCtx = nullptr;
    SwsContext *swsCtx = nullptr;
    const AVCodec *codec = nullptr;
    int videoIdx = -1;

    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);   // TCP надёжнее UDP в LAN
    av_dict_set(&opts, "stimeout", "5000000", 0);     // 5 с на сетевые операции
    av_dict_set(&opts, "buffer_size", "4194304", 0);  // 4 МБ приёмный буфер

    // Логин/пароль можно передать и в самом URL
    QString fullUrl = m_url;
    if (!m_user.isEmpty()) {
        // rtsp://[user:pass@]host/path
        if (!m_url.contains('@')) {
            fullUrl = m_url;
            const int sep = fullUrl.indexOf("://");
            if (sep > 0)
                fullUrl.insert(sep + 3, QStringLiteral("%1:%2@")
                                                .arg(m_user, m_password));
        }
    }

    if (avformat_open_input(&fmtCtx, fullUrl.toUtf8().constData(), nullptr, &opts) < 0) {
        emit errorOccurred(QStringLiteral("Не удалось открыть поток: %1").arg(m_url));
        av_dict_free(&opts);
        return;
    }
    av_dict_free(&opts);

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        emit errorOccurred(QStringLiteral("Нет информации о потоке"));
        avformat_close_input(&fmtCtx);
        return;
    }

    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIdx = int(i);
            break;
        }
    }
    if (videoIdx < 0) {
        emit errorOccurred(QStringLiteral("В потоке нет видео"));
        avformat_close_input(&fmtCtx);
        return;
    }

    const AVCodecParameters *op = fmtCtx->streams[videoIdx]->codecpar;
    codec = avcodec_find_decoder(op->codec_id);
    if (!codec) {
        emit errorOccurred(QStringLiteral("Декодер не найден"));
        avformat_close_input(&fmtCtx);
        return;
    }
    codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, op);
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        emit errorOccurred(QStringLiteral("Не удалось открыть кодек"));
        goto cleanup;
    }

    swsCtx = sws_getContext(codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
                            codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB24,
                            SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsCtx) {
        emit errorOccurred(QStringLiteral("Не удалось создать контекст масштабирования"));
        goto cleanup;
    }

    emit connected(codecCtx->width, codecCtx->height,
                   QString::fromUtf8(codec->name));

    {
        AVFrame *frame = av_frame_alloc();
        AVPacket *pkt = av_packet_alloc();

        while (!m_stop) {
            int ret = av_read_frame(fmtCtx, pkt);
            if (ret < 0) {
                if (ret == AVERROR_EOF || ret == AVERROR(ETIMEDOUT)) {
                    emit errorOccurred(QStringLiteral("Поток завершён/таймаут"));
                    break;
                }
                continue;
            }
            if (pkt->stream_index != videoIdx) {
                av_packet_unref(pkt);
                continue;
            }
            ret = avcodec_send_packet(codecCtx, pkt);
            av_packet_unref(pkt);
            if (ret < 0)
                continue;

            while (!m_stop && avcodec_receive_frame(codecCtx, frame) == 0) {
                const int w = frame->width, h = frame->height;
                // Конвертируем сразу в RGB24 -> QImage с непрерывным буфером
                QImage img(w, h, QImage::Format_RGB888);
                if (!img.isNull()) {
                    uint8_t *dstPlane[1] = { const_cast<uint8_t *>(img.bits()) };
                    int dstStride[1] = { w * 3 };
                    sws_scale(swsCtx, frame->data, frame->linesize, 0, h,
                              dstPlane, dstStride);
                    emit frameReady(img);
                }
            }
        }

        av_frame_free(&frame);
        av_packet_free(&pkt);
    }

cleanup:
    if (swsCtx)
        sws_freeContext(swsCtx);
    if (codecCtx)
        avcodec_free_context(&codecCtx);
    if (fmtCtx)
        avformat_close_input(&fmtCtx);
}

CameraStream::CameraStream(QObject *parent) : QObject(parent) {}

CameraStream::~CameraStream()
{
    close();
}

void CameraStream::open(const QString &url, const QString &user, const QString &password)
{
    close();
    m_url = url;
    m_thread = new DecodeThread(url, user, password, this);
    connect(m_thread, &DecodeThread::frameReady, this, &CameraStream::frameReady);
    connect(m_thread, &DecodeThread::errorOccurred, this, [this](const QString &e) {
        emit errorOccurred(e);
    });
    connect(m_thread, &DecodeThread::connected, this, &CameraStream::connected);
    m_thread->start();
}

void CameraStream::close()
{
    if (!m_thread)
        return;
    DecodeThread *t = m_thread;
    m_thread = nullptr;
    t->requestStop();
    t->disconnect(this);
    if (!t->wait(4000)) {
        t->terminate();
        t->wait(1000);
    }
    t->deleteLater();
}
