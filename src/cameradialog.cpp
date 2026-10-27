#include "cameradialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QIntValidator>

CameraDialog::CameraDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Шаблон камеры"));

    m_name = new QLineEdit;
    m_name->setPlaceholderText(QStringLiteral("Название, напр. «Вход»"));

    m_ip = new QLineEdit;
    m_ip->setPlaceholderText(QStringLiteral("192.168.1.108 или диапазон 192.168.1.1-30"));

    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(554);

    m_path = new QLineEdit(QStringLiteral("/onvif1"));
    m_path->setPlaceholderText(QStringLiteral("/onvif1, /media/video1, /unicast ..."));

    m_user = new QLineEdit(QStringLiteral("admin"));
    m_pass = new QLineEdit;
    m_pass->setEchoMode(QLineEdit::Password);

    m_rtspChk = new QCheckBox(QStringLiteral("Использовать готовый RTSP URL вместо IP/порт/путь"));
    m_rtspUrl = new QLineEdit;
    m_rtspUrl->setPlaceholderText(QStringLiteral("rtsp://user:pass@192.168.1.10:554/onvif1"));
    m_rtspUrl->setVisible(false);

    connect(m_rtspChk, &QCheckBox::toggled, this, &CameraDialog::toggleMode);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Имя:"), m_name);
    form->addRow(QStringLiteral("IP-адрес:"), m_ip);
    form->addRow(QStringLiteral("RTSP-порт:"), m_port);
    form->addRow(QStringLiteral("Путь потока:"), m_path);
    form->addRow(QStringLiteral("Логин:"), m_user);
    form->addRow(QStringLiteral("Пароль:"), m_pass);
    form->addRow(QString(), m_rtspChk);
    form->addRow(QStringLiteral("RTSP URL:"), m_rtspUrl);

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
    root->addLayout(btnRow);
    resize(460, 340);
}

void CameraDialog::toggleMode(bool useRtsp)
{
    m_rtspUrl->setVisible(useRtsp);
    m_ip->setEnabled(!useRtsp);
    m_port->setEnabled(!useRtsp);
    m_path->setEnabled(!useRtsp);
}

void CameraDialog::setTemplate(const CameraTemplate &t)
{
    m_name->setText(t.name);
    m_ip->setText(t.ip);
    m_port->setValue(t.port);
    m_path->setText(t.path);
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
    t.user     = m_user->text().trimmed();
    t.password = m_pass->text();
    t.useRtspUrl = m_rtspChk->isChecked();
    t.rtspUrl    = m_rtspUrl->text().trimmed();
    if (t.name.isEmpty())
        t.name = t.useRtspUrl ? t.rtspUrl : t.ip;
    return t;
}
