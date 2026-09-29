import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18n("Accounts")
        icon: "user-identity"
        source: "configAccounts.qml"
    }
    ConfigCategory {
        name: i18n("Networks")
        icon: "network-wireless"
        source: "configNetworks.qml"
    }
    ConfigCategory {
        name: i18n("Portal and Xray")
        icon: "configure"
        source: "configGeneral.qml"
    }
    ConfigCategory {
        name: i18n("Logs")
        icon: "view-list-text"
        source: "configLogs.qml"
    }
}
