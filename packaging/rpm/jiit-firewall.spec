Name:           jiit-firewall
Version:        1.0.0
Release:        1%{?dist}
Summary:        Native KDE utility for JIIT captive portal and Xray
License:        MIT
BuildArch:      x86_64
BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  plasma-workspace-devel
Requires:       NetworkManager
Requires:       qt6-qtbase

%description
C++20 and Qt 6 user daemon and Plasma applet with native Plasma configuration.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

%files
%{_bindir}/jiit-firewall-daemon
%{_datadir}/systemd/user/jiit-firewall.service
%{_datadir}/plasma/plasmoids/com.gaurishhs.wifi
%{_datadir}/dbus-1/services/org.jiit.FirewallManager.service
