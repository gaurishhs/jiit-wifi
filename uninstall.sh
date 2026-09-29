#!/usr/bin/env bash
set -euo pipefail
systemctl --user disable --now jiit-firewall.service 2>/dev/null || true
rm -f "$HOME/.config/systemd/user/jiit-firewall.service" "$HOME/.local/share/systemd/user/jiit-firewall.service" "$HOME/.local/share/dbus-1/services/org.jiit.FirewallManager.service"
rm -f "$HOME/.local/bin/jiit-firewall-daemon" "$HOME/.local/bin/jiit-firewall-settings" "$HOME/.local/share/applications/jiit-firewall-settings.desktop"
systemctl --user daemon-reload
if command -v kpackagetool6 >/dev/null; then kpackagetool6 --type Plasma/Applet --remove com.gaurishhs.wifi 2>/dev/null || true; fi
echo "Removed the app, daemon and applet. Config, logs, and KDE Wallet entries were preserved."
