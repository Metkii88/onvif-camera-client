#include "videocell.h"

#include <QPainter>
#include <QMouseEvent>

VideoCell::VideoCell(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(160, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_fpsTimer.start();
}

void VideoCell::attach(CameraStream *stream, const QString &title)
{
    detach();
    m_stream = stream;
    m_title = title;
    m_status = QStringLiteral("подключение...");
    if (m_stream) {
        // запоминаем параметры для перезапуска после ресайза ячейки
        m_url = m_stream->url();
        connect(m_stream, &CameraStream::frameReady, this, &VideoCell::onFrame);
        connect(m_stream, &CameraStream::errorOccurred, this, &VideoCell::onError);
        connect(m_stream, &CameraStream::connected, this, &VideoCell::onConnected);
    }
    update();
}

void VideoCell::setStreamCredentials(const QString &user, const QString &password)
{
    m_user = user;
    m_password = password;
}

void VideoCell::restartStream()
{
    if (!m_stream || m_url.isEmpty())
        return;
    // пересоздаём декодер: кадры будут масштабироваться под новый размер ячейки
    m_stream->open(m_url, m_user, m_password);
    m_status = QStringLiteral("переподключение после изменения размера...");
    m_last = QImage();
    update();
}

void VideoCell::detach()
{
    if (m_stream) {
        disconnect(m_stream, nullptr, this, nullptr);
        m_stream = nullptr;
    }
    m_last = QImage();
    m_title.clear();
    m_status = QStringLiteral("нет потока");
    m_url.clear();
    m_user.clear();
    m_password.clear();
    update();
}

void VideoCell::onFrame(const QImage &img)
{
    m_last = img; // QImage copy-on-write, данные уже наши
    ++m_frames;
    if (m_fpsTimer.elapsed() > 1000) {
        m_fps = m_frames * 1000.0 / m_fpsTimer.elapsed();
        m_frames = 0;
        m_fpsTimer.restart();
    }
    m_status.clear();
    update();
}

void VideoCell::onError(const QString &msg)
{
    m_status = msg;
    update();
}

void VideoCell::onConnected(int w, int h, const QString &codec)
{
    m_status = QStringLiteral("%1x%2 %3").arg(w).arg(h).arg(codec);
    update();
}

void VideoCell::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (!m_last.isNull()) {
        // Масштабируем с сохранением пропорций (letterbox)
        const QSize scaled = m_last.size().scaled(size(), Qt::KeepAspectRatio);
        const QRect target(QPoint((width() - scaled.width()) / 2,
                                  (height() - scaled.height()) / 2), scaled);
        p.drawImage(target, m_last);
    } else {
        p.setPen(Qt::darkGray);
        p.drawText(rect(), Qt::AlignCenter, m_status);
    }

    // Подпись сверху
    if (!m_title.isEmpty() || m_fps > 0) {
        QString label = m_title;
        if (m_fps > 0)
            label += QString("  [%1 fps]").arg(m_fps, 0, 'f', 1);
        QFont f = p.font();
        f.setPointSize(9);
        p.setFont(f);
        const int textH = fontMetrics().height() + 6;
        p.fillRect(0, 0, width(), textH, QColor(0, 0, 0, 150));
        p.setPen(Qt::white);
        p.drawText(QRect(4, 2, width() - 8, textH - 2),
                   Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    // Рамка активной ячейки
    p.setPen(QPen(QColor(70, 70, 70), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void VideoCell::mouseDoubleClickEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton)
        emit maximizeRequested(this);
    QWidget::mouseDoubleClickEvent(ev);
}
