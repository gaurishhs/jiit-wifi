import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus

Kirigami.ScrollablePage {
    id: page
    title: i18n("Networks")
    property bool busy: false
    property string message: ""

    function call(method, args, done) {
        SessionBus.asyncCall({service: "org.jiit.FirewallManager", path: "/org/jiit/FirewallManager", iface: "org.jiit.FirewallManager", member: method, arguments: args || []},
            function(reply) { if (done) done(reply.values.length ? String(reply.values[0]) : "") },
            function(error) { page.busy = false; page.message = error.message })
    }
    function toList(text) {
        var result = []
        var parts = String(text || "").split(",")
        for (var i = 0; i < parts.length; ++i)
            if (parts[i].trim().length) result.push(parts[i].trim())
        return result
    }
    function load() {
        page.busy = true
        call("GetSettings", [], function(value) {
            try {
                var settings = JSON.parse(value)
                networkModel.clear()
                for (var i = 0; i < settings.networks.length; ++i) {
                    var n = settings.networks[i]
                    networkModel.append({name: n.name, ssid: n.ssid, bssids: n.bssids.join(", "), interfaces: n.interfaces.join(", "), networkEnabled: n.enabled})
                }
                page.message = ""
            } catch (e) { page.message = i18n("Could not read networks: %1", e) }
            page.busy = false
        })
    }
    function addCurrentNetwork() {
        page.busy = true
        call("DetectCurrentNetwork", [], function(value) {
            page.busy = false
            try {
                var n = JSON.parse(value)
                if (!n.connected || !n.ssid) {
                    page.message = i18n("No connected Wi-Fi network was detected.")
                    return
                }
                networkModel.append({name: n.ssid, ssid: n.ssid, bssids: "", interfaces: "", networkEnabled: true})
                page.message = i18n("Added %1. Save networks to apply it.", n.ssid)
            } catch (e) { page.message = i18n("Could not detect the current network: %1", e) }
        })
    }
    function save() {
        if (page.busy) return
        page.busy = true
        call("GetSettings", [], function(value) {
            try {
                var settings = JSON.parse(value)
                var networks = []
                for (var i = 0; i < networkModel.count; ++i) {
                    var n = networkModel.get(i)
                    networks.push({name: n.name, ssid: n.ssid, bssids: page.toList(n.bssids), interfaces: page.toList(n.interfaces), enabled: n.networkEnabled})
                }
                settings.networks = networks
                call("SetSettings", [JSON.stringify(settings)], function(result) {
                    page.busy = false
                    try {
                        var response = JSON.parse(result)
                        page.message = response.ok ? i18n("Networks saved and applied.") : response.message
                    } catch (e) { page.message = result }
                })
            } catch (e) { page.busy = false; page.message = i18n("Could not prepare network settings: %1", e) }
        })
    }
    function addNetwork() {
        if (!newName.text.trim() || !newSsid.text.trim()) {
            page.message = i18n("A profile name and SSID are required.")
            return
        }
        networkModel.append({name: newName.text.trim(), ssid: newSsid.text.trim(), bssids: newBssids.text, interfaces: newInterfaces.text, networkEnabled: newEnabled.checked})
        newName.clear(); newSsid.clear(); newBssids.clear(); newInterfaces.clear(); newEnabled.checked = true
        networkDialog.close()
    }

    ListModel { id: networkModel }
    Component.onCompleted: page.load()

    ColumnLayout {
        width: page.width
        spacing: Kirigami.Units.largeSpacing
        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18n("Profiles match by SSID by default. Optional BSSID and interface lists restrict a profile; separate multiple values with commas.")
        }
        RowLayout {
            Controls.Button { text: i18n("Add network"); enabled: !page.busy; onClicked: networkDialog.open() }
            Controls.Button { text: i18n("Add current Wi-Fi"); enabled: !page.busy; onClicked: page.addCurrentNetwork() }
        }
        Repeater {
            model: networkModel
            delegate: Controls.GroupBox {
                required property int index
                required property string name
                required property string ssid
                required property string bssids
                required property string interfaces
                required property bool networkEnabled
                Layout.fillWidth: true
                title: name
                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    Controls.TextField { Layout.fillWidth: true; text: name; placeholderText: i18n("Profile name"); onTextEdited: networkModel.setProperty(index, "name", text) }
                    Controls.TextField { Layout.fillWidth: true; text: ssid; placeholderText: i18n("SSID"); onTextEdited: networkModel.setProperty(index, "ssid", text) }
                    Controls.TextField { Layout.fillWidth: true; text: bssids; placeholderText: i18n("BSSIDs (optional, comma separated)"); onTextEdited: networkModel.setProperty(index, "bssids", text) }
                    Controls.TextField { Layout.fillWidth: true; text: interfaces; placeholderText: i18n("Interfaces (optional, comma separated)"); onTextEdited: networkModel.setProperty(index, "interfaces", text) }
                    RowLayout {
                        Controls.CheckBox { text: i18n("Enabled"); checked: networkEnabled; onToggled: networkModel.setProperty(index, "networkEnabled", checked) }
                        Item { Layout.fillWidth: true }
                        Controls.Button { text: i18n("Remove"); enabled: !page.busy; onClicked: networkModel.remove(index) }
                    }
                }
            }
        }
        RowLayout {
            Controls.Button { text: i18n("Save networks"); enabled: !page.busy; onClicked: page.save() }
            Controls.BusyIndicator { running: page.busy; visible: page.busy; implicitWidth: 28; implicitHeight: 28 }
            Controls.Label { text: page.message; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }

    Controls.Dialog {
        id: networkDialog
        title: i18n("Add JIIT network")
        modal: true
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        onAccepted: page.addNetwork()
        contentItem: ColumnLayout {
            Controls.TextField { id: newName; Layout.fillWidth: true; placeholderText: i18n("Profile name") }
            Controls.TextField { id: newSsid; Layout.fillWidth: true; placeholderText: i18n("SSID") }
            Controls.TextField { id: newBssids; Layout.fillWidth: true; placeholderText: i18n("BSSIDs (optional, comma separated)") }
            Controls.TextField { id: newInterfaces; Layout.fillWidth: true; placeholderText: i18n("Interfaces (optional, comma separated)") }
            Controls.CheckBox { id: newEnabled; text: i18n("Enabled"); checked: true }
        }
    }
}
