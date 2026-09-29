#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build"
rm -f "$HOME/.local/bin/jiit-firewall-settings" "$HOME/.local/share/applications/jiit-firewall-settings.desktop"
mkdir -p "$HOME/.config/systemd/user" "$HOME/.config/jiit-firewall" "$HOME/.local/state/jiit-firewall" "$HOME/.local/share/dbus-1/services"
chmod 700 "$HOME/.config/jiit-firewall" "$HOME/.local/state/jiit-firewall"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build "$BUILD" --parallel
cmake --install "$BUILD" --prefix "$HOME/.local"
install -m 0644 "$ROOT/systemd/jiit-firewall.service" "$HOME/.config/systemd/user/jiit-firewall.service"
install -m 0644 "$ROOT/dbus/org.jiit.FirewallManager.service" "$HOME/.local/share/dbus-1/services/org.jiit.FirewallManager.service"
if command -v kpackagetool6 >/dev/null; then
    kpackagetool6 --type Plasma/Applet --install "$ROOT/package" || kpackagetool6 --type Plasma/Applet --upgrade "$ROOT/package"
else
    echo "kpackagetool6 was not found. Install plasma-workspace-devel, then install the applet manually."
fi
systemctl --user daemon-reload
systemctl --user enable jiit-firewall.service
if systemctl --user is-active --quiet jiit-firewall.service; then
    systemctl --user restart jiit-firewall.service
else
    systemctl --user start jiit-firewall.service
fi
echo "Installed. Add ‘JIIT Firewall Manager’ from the Plasma panel's Add Widgets dialog."
echo "Right-click the widget and choose Configure to manage accounts, networks and service settings."
