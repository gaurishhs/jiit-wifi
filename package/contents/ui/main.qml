import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.workspace.dbus 1.0

PlasmoidItem {
    id: root
    implicitWidth: 32
    implicitHeight: 32
    property var statusData: ({
            state: "DISCONNECTED",
            network: ({}),
            xray: "Unknown",
            account: ""
        })
    property string message: ""
    function displayState() {
        return String(root.statusData.state || "DISCONNECTED").replace(/_/g, " ");
    }

    QtObject {
        id: backend
        function invoke(name, callback) {
            SessionBus.asyncCall({
                service: "org.jiit.FirewallManager",
                path: "/org/jiit/FirewallManager",
                iface: "org.jiit.FirewallManager",
                member: name,
                arguments: []
            }, function (reply) {
                if (callback)
                    callback(reply.values[0]);
            }, function (error) {
                root.message = error.message;
            });
        }
        function refresh() {
            invoke("GetStatus", function (reply) {
                try {
                    root.statusData = JSON.parse(reply);
                } catch (e) {}
            });
        }
        function action(name) {
            invoke(name, function (reply) {
                try {
                    root.message = JSON.parse(reply).message || "";
                } catch (e) {}
                refresh();
            });
        }
        Component.onCompleted: refresh()
    }
    Timer {
        interval: 3000
        running: true
        repeat: true
        onTriggered: backend.refresh()
    }

    compactRepresentation: PlasmaComponents.ToolButton {
        implicitWidth: 32
        implicitHeight: 32
        Accessible.name: "JIIT Firewall " + root.displayState()
        icon.name: root.statusData.state === "DISCONNECTED" ? "network-wireless-offline" : "network-wireless"
        onClicked: root.expanded = !root.expanded
    }
    fullRepresentation: PlasmaComponents.Page {
        Layout.minimumWidth: 330
        Layout.minimumHeight: 340
        header: PlasmaComponents.Label {
            text: "JIIT Firewall"
            font.bold: true
            font.pointSize: 14
        }
        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10
            PlasmaComponents.Label {
                text: "Status: " + root.displayState()
                font.bold: true
            }
            PlasmaComponents.Label {
                text: "Network: " + (root.statusData.network.ssid || "Disconnected")
            }
            PlasmaComponents.Label {
                text: "Access point: " + (root.statusData.network.bssid || "—")
            }
            PlasmaComponents.Label {
                text: "Account: " + (root.statusData.account || "—")
            }
            PlasmaComponents.Label {
                text: "Portal: " + (root.statusData.portal_reachable ? "Reachable" : "Unavailable")
            }
            PlasmaComponents.Label {
                text: "Xray: " + root.statusData.xray
            }
            PlasmaComponents.Label {
                text: root.message
                visible: root.message.length > 0
                wrapMode: Text.WordWrap
            }
            Row {
                spacing: 6
                PlasmaComponents.Button {
                    text: "Login"
                    enabled: !root.statusData.authenticated
                    onClicked: backend.action("Login")
                }
                PlasmaComponents.Button {
                    text: "Logout"
                    enabled: root.statusData.authenticated
                    onClicked: backend.action("Logout")
                }
                PlasmaComponents.Button {
                    text: "Retry"
                    onClicked: backend.action("Retry")
                }
            }
            Row {
                spacing: 6
                PlasmaComponents.Button {
                    text: "Start Xray"
                    onClicked: backend.action("StartXray")
                }
                PlasmaComponents.Button {
                    text: "Stop Xray"
                    onClicked: backend.action("StopXray")
                }
            }
            PlasmaComponents.Label {
                text: "Right-click the widget and choose Configure to manage accounts, networks, and service settings."
                wrapMode: Text.WordWrap
            }
        }
    }
}
