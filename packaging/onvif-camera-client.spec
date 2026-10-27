Name:           onvif-camera-client
Version:        2.0.0
Release:        1%{?dist}
Summary:        ONVIF camera discovery and multi-stream viewer
Summary(ru):    ONVIF-поиск камер и просмотр нескольких потоков
License:        MIT
URL:            https://github.com/Metkii88/onvif-camera-client
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake gcc-c++ qt5-qtbase-devel ffmpeg-devel

%description
Client for IP cameras: WS-Discovery (ONVIF) search in local network,
template-based subnet scan with login/password check, saved camera
profiles, multi-grid live view, fullscreen mode.

%description -l ru
Клиент IP-камер: поиск по ONVIF (WS-Discovery) в локальной сети,
сканирование подсети по шаблону с проверкой логина/пароля,
сохранённые наборы камер, вывод нескольких потоков сеткой,
полноэкранный режим и запуск с заданным набором (--profile).

%prep
%setup -q

%build
%cmake .
%cmake_build

%install
%cmake_install
install -Dm644 packaging/org.onvif.client.desktop \
    %{buildroot}%{_datadir}/applications/org.onvif.client.desktop
for s in 16 24 32 48 64 128 256; do
    install -Dm644 packaging/icons/hicolor/${s}x${s}/apps/org.onvif.client.png \
        %{buildroot}%{_datadir}/icons/hicolor/${s}x${s}/apps/org.onvif.client.png
done
install -Dm644 packaging/org.onvif.client.svg \
    %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/org.onvif.client.svg

%files
%license LICENSE
%doc README.md
%{_bindir}/onvif-client
%{_datadir}/applications/org.onvif.client.desktop
%{_datadir}/icons/hicolor/*/apps/org.onvif.client.*

%changelog
* Tue Oct 27 2026 Metkii88 <metkii88@example.local> - 2.0.0-1
- Camera search templates (IP/port/path/login/password)
- Subnet scanner (ONVIF multicast + RTSP DESCRIBE check)
- Saved camera store with select/connect/edit/delete
- Fullscreen mode (F11/Esc, --fullscreen)
- Start with saved profile: --profile "name1,name2"
- System installation: desktop entry + camera icon
