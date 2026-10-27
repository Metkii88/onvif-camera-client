#include "cameradialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QIntValidator>
#include <QRegularExpression>

// Разобрать адрес из строки шаблона: "192.168.0.1-254", "192.168.0.1-30",
// "192.168.0.1", "192.168.0" (весь /24).
IpRangeParse parseIpRange(const QString &text)
{
    IpRangeParse r;
    const QString s = text.trimmed();
    if (s.isEmpty())
        return r;

    // x.x.x.a-b  |  x.x.x.a  |  x.x.x
    static const QRegularExpression re(
        QStringLiteral("^(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})"       // префикс
                       "(?:\\.(\\d{1,3})(?:\\s*[-–—:]\\s*(\\d{1,3}))?)?$"));
    const QRegularExpressionMatch m = re.match(s);
    if (!m.hasMatch())
        return r;

    const QStringList pfxParts = m.captured(1).split('.');
    bool okPfx = true;
    for (const QString &p : pfxParts) {
        bool ok = false;
        const int v = p.toInt(&ok);
        if (!ok || v < 0 || v > 255) { okPfx = false; break; }
    }
    if (!okPfx)
        return r;

    r.prefix = m.captured(1);
    r.ok = true;

    const QString a = m.captured(2);
    const QString b = m.captured(3);
    if (a.isEmpty()) {                      // только префикс — весь /24
        r.from = 1; r.to = 254; r.isRange = true;
        return r;
    }
    int av = a.toInt();
    if (av < 0 || av > 255) av = 1;
    if (b.isEmpty()) {                        // одиночный адрес
        r.from = av; r.to = av; r.isRange = false;
        return r;
    }
    int bv = b.toInt();
    if (bv < 0 || bv > 255) bv = 254;
    r.from = qMin(av, bv);
    r.to   = qMax(av, bv);
    r.isRange = true;
    return r;
}

CameraDialog::CameraDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Шаблон камеры"));

    m_name = new QLineEdit;
    m_name->setPlaceholderText(QStringLiteral("Название, напр. «Вход»"));

    m_ip = new QLineEdit;
    m_ip->setPlaceholderText(QStringLiteral("192.168.0.108 или диапазон 192.168.0.1-254"));

    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(554);

    m_path = new QLineEdit(QStringLiteral("/onvif1"));
    m_path->setPlaceholderText(QStringLiteral("/onvif1, /media/video1, /unicast ..."));

    m_codec = new QComboBox;
    m_codec->addItems({QStringLiteral("h264"), QStringLiteral("mjpeg"),
                       QStringLiteral("h265"), QStringLiteral("any")});
    m_codec->setEditable(true);   // можно ввести свой вариант
    m_codec->setCurrentText(QStringLiteral("h264"));
    m_codec->setToolTip(QStringLiteral(
        "Кодек потока. Используется при поиске по шаблону: если путь «/onvif1» не подошёл,\n"
        "сканер переберёт типовые пути для этого кодека (например /Streaming/Channels/101)."));

    m_user = new QLineEdit(QStringLiteral("admin"));
    m_pass = new QLineEdit(QStringLiteral("admin"));
    m_pass->setEchoMode(QLineEdit::Password);
    m_pass->setPlaceholderText(QStringLiteral("пароль"));

    m_rtspChk = new QCheckBox(QStringLiteral("Использовать готовый RTSP URL вместо IP/порт/путь"));
    m_rtspUrl = new QLineEdit;
    m_rtspUrl->setPlaceholderText(QStringLiteral("rtsp://user:pass@192.168.1.10:554/onvif1"));
    m_rtspUrl->setVisible(false);

    connect(m_rtspChk, &QCheckBox::toggled, this, &CameraDialog::toggleMode);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Имя:"), m_name);
    form->addRow(QStringLiteral("IP-адрес / диапазон:"), m_ip);
    form->addRow(QStringLiteral("RTSP-порт:"), m_port);
    form->addRow(QStringLiteral("Путь потока:"), m_path);
    form->addRow(QStringLiteral("Кодек потока:"), m_codec);
    form->addRow(QStringLiteral("Логин:"), m_user);
    form->addRow(QStringLiteral("Пароль:"), m_pass);
    form->addRow(QString(), m_rtspChk);
    form->addRow(QStringLiteral("RTSP URL:"), m_rtspUrl);

    // Подсказка под полем IP: показывает распознанный диапазон
    m_rangeHint = new QLabel;
    m_rangeHint->setObjectName("fieldHint");
    m_rangeHint->setWordWrap(true);
    auto updateHint = [this] {
        const IpRangeParse r = parseIpRange(m_ip->text());
        if (!r.ok) {
            m_rangeHint->setText(QStringLiteral(
                "Формат: 192.168.0.108 или диапазон 192.168.0.1-254"));
        } else if (r.isRange) {
            m_rangeHint->setText(QStringLiteral(
                "Будет просканировано: %1.%2 – %1.%3 (%4 адресов)")
                .arg(r.prefix).arg(r.from).arg(r.to).arg(r.to - r.from + 1));
        } else {
            m_rangeHint->setText(QStringLiteral("Одиночный адрес: %1.%2")
                                 .arg(r.prefix).arg(r.from));
        }
    };
    connect(m_ip, &QLineEdit::textChanged, this, updateHint);
    updateHint();

    m_ok = new QPushButton(QStringLiteral("Сохранить шаблон"));
    QPushButton *cancel = new QPushButton(QStringLiteral("Отмена"));
    connect(m_ok, &QPushButton::clicked, this, [this] {
        if (!m_rtspChk->isChecked() && m_ip->text().trimmed().isEmpty()) {
            m_ip->setFocus();
            return;
        }
        if (m_rtspChk->isChecked() && m_rtspUrl->text().trimmed().isEmpty()) {
            m_rtspUrl->setFocus();
            return;
        }
        accept();
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(m_ok);
    btnRow->addWidget(cancel);

    auto *root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(m_rangeHint);
    root->addLayout(btnRow);
    resize(480, 380);
}

void CameraDialog::toggleMode(bool useRtsp)
{
    m_rtspUrl->setVisible(useRtsp);
    m_ip->setEnabled(!useRtsp);
    m_port->setEnabled(!useRtsp);
    m_path->setEnabled(!useRtsp);
    m_codec->setEnabled(!useRtsp);
}

void CameraDialog::setTemplate(const CameraTemplate &t)
{
    m_name->setText(t.name);
    m_ip->setText(t.ip);
    m_port->setValue(t.port);
    m_path->setText(t.path);
    m_codec->setCurrentText(t.codec.isEmpty() ? QStringLiteral("h264") : t.codec);
    m_user->setText(t.user);
    m_pass->setText(t.password);
    m_rtspUrl->setText(t.rtspUrl);
    m_rtspChk->setChecked(t.useRtspUrl);
}

CameraTemplate CameraDialog::templateValue() const
{
    CameraTemplate t;
    t.name     = m_name->text().trimmed();
    t.ip       = m_ip->text().trimmed();
    t.port     = m_port->value();
    t.path     = m_path->text().trimmed();
    if (t.path.isEmpty() || !t.path.startsWith('/'))
        t.path.prepend('/');
    t.codec    = m_codec->currentText().trimmed().toLower();
    t.user     = m_user->text().trimmed();
    t.password = m_pass->text();
    t.useRtspUrl = m_rtspChk->isChecked();
    t.rtspUrl    = m_rtspUrl->text().trimmed();
    if (t.name.isEmpty()) {
        // для диапазона имя — по префиксу подсети
        const IpRangeParse r = parseIpRange(t.ip);
        if (t.useRtspUrl)
            t.name = t.rtspUrl;
        else if (r.ok && r.isRange)
            t.name = QStringLiteral("%1.%2-%3").arg(r.prefix).arg(r.from).arg(r.to);
        else
            t.name = t.ip;
    }
    return t;
}
