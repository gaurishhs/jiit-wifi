import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus

Kirigami.ScrollablePage {
    id: page
    title: i18n("Logs")
    property bool busy: false
    property string message: ""
    property string logs: ""

    function refresh() {
        page.busy = true
        SessionBus.asyncCall({service: "org.jiit.FirewallManager", path: "/org/jiit/FirewallManager", iface: "org.jiit.FirewallManager", member: "GetLogs", arguments: []},
            function(reply) { page.logs = reply.values.length ? String(reply.values[0]) : ""; page.busy = false },
            function(error) { page.message = error.message; page.busy = false })
    }
    Component.onCompleted: refresh()

    ColumnLayout {
        width: page.width
        Controls.Label { text: page.message; visible: page.message.length > 0; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Controls.TextArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 400
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.NoWrap
            text: page.logs
        }
        Controls.Button { text: page.busy ? i18n("Loading…") : i18n("Refresh logs"); enabled: !page.busy; onClicked: page.refresh() }
    }
}
