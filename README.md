# JIIT Firewall Manager

A native C++20/Qt 6 daemon and Plasma 6 panel widget for Sophos captive portal automation and optional Xray systemd control. Network identity uses configured SSIDs; BSSIDs are informational unless an advanced profile explicitly restricts them.

## Fedora 44 installation

Install the compiler and Qt/KDE development tools, then build and install into `~/.local`:

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtdeclarative-devel plasma-workspace-devel NetworkManager-devel
./install.sh
```

The installer builds and installs the background daemon and Plasma applet, then enables the daemon. Add **JIIT Firewall Manager** from the panel's Add Widgets dialog. Right-click the widget and choose **Configure** to manage accounts, networks, portal behavior, Xray, and logs.

In the widget's Configure dialog, add one or more accounts and configure each JIIT SSID. Passwords are sent to the daemon over the user's session bus only for storage in KDE Wallet; they are not written to the applet configuration or application JSON file. A normal profile matches its SSID on any access point. Optional BSSID restrictions are available for special cases.

```bash
systemctl --user status jiit-firewall
journalctl --user -u jiit-firewall -f
```

## Behavior and configuration

Non-secret configuration is stored at `~/.config/jiit-firewall/config.json` with owner-only permissions. Logs are written to `~/.local/state/jiit-firewall/daemon.log` and journald. The daemon checks NetworkManager through the `nmcli` JSON-like escaped output at the configured interval, debounces leaving for eight seconds, and ignores BSSID-only changes.

On a recognized SSID the daemon stops Xray if it is active, waits for the Sophos gateway, then tries enabled accounts by priority. Successful or already-authenticated portal XML responses count as authenticated. It starts Xray afterward if configured. It tracks whether the daemon started Xray and only stops an owned instance during automatic disconnect and suspend handling. Select `system` or `user` service mode in settings. System service requests go through systemd's normal polkit authorization path; the application does not use sudo or store a sudo password.

The D-Bus service is `org.jiit.FirewallManager`, object `/org/jiit/FirewallManager`. It exposes Login, Logout, Retry, GetStatus, GetAccounts, GetSettings, SetSettings, SaveAccount, RemoveAccount, GetLogs, StartXray, StopXray and DetectCurrentNetwork. Status, login, network and Xray changes are signaled over D-Bus. The daemon listens to logind `PrepareForSleep`; after wake it waits before checking the current SSID again.

The Sophos request maintains the reference `int(time.time()*100)` timestamp and form fields (`mode=191`/`193`, `producttype=0`). Portal connection failures retry with exponential backoff. XML parsing is strict and response text is retained in logs.

## Troubleshooting

- **Wallet errors:** unlock KDE Wallet in the Plasma session and check that the `kdewallet` wallet exists.
- **Network not detected:** check `nmcli device wifi` and NetworkManager status. Hidden SSIDs generally need to be saved as a NetworkManager profile before association.
- **Portal unavailable:** verify the SSID profile and access to `http://172.16.68.6:8090/`.
- **Xray control failure:** inspect `systemctl [--user] status xray.service`; for system units, check the user's polkit authorization for managing that unit.
- **Daemon state:** `systemctl --user status jiit-firewall`; logs are in `~/.local/state/jiit-firewall/daemon.log` and `journalctl --user -u jiit-firewall`.
- **D-Bus state:** `busctl --user introspect org.jiit.FirewallManager /org/jiit/FirewallManager`.

## Build and tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The workspace includes VS Code settings for clangd and CMake Tools. Install the recommended extensions if prompted, then open the repository folder; CMake Tools configures the `dev` preset and exports `build/compile_commands.json` for Qt-aware completion and diagnostics. You can also generate it manually with `cmake --preset dev`.

Unit tests cover timestamp and request fields, Sophos XML parsing/classification, SSID matching across access points, optional BSSID constraints and multiple recognized SSIDs. They do not contact the real gateway.

## RPM groundwork

`packaging/rpm/jiit-firewall.spec` provides an initial CMake RPM spec. Adjust dependency names and install paths for the target Fedora packaging environment before producing a distributable RPM.

## Uninstall

Run `./uninstall.sh`. It removes installed binaries, service files and the plasmoid while preserving configuration, logs and Wallet entries.
