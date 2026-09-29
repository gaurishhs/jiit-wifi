import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus

Kirigami.ScrollablePage {
    id: page
    title: i18n("Portal and Xray")

    property bool loaded: false
    property bool busy: false
    property string message: ""
    property string gateway: "http://172.16.68.6:8090/"
    property int timeoutSeconds: 8
    property int retryCount: 4
    property int retryIntervalSeconds: 15
    property int pollIntervalSeconds: 5
    property string xrayMode: "system"
    property string xrayService: "xray.service"
    property bool debug: false
    property bool xrayEnabled: true
    property bool startXrayAfterLogin: true
    property bool stopXrayOnLeave: true
    property bool logoutBeforeSleep: true
    property bool autoLoginAfterWake: true

    function call(method, args, done) {
        SessionBus.asyncCall({service: "org.jiit.FirewallManager", path: "/org/jiit/FirewallManager", iface: "org.jiit.FirewallManager", member: method, arguments: args || []},
            function(reply) { if (done) done(reply.values.length ? String(reply.values[0]) : "") },
            function(error) { page.busy = false; page.message = error.message })
    }
    function load() {
        page.busy = true
        call("GetSettings", [], function(value) {
            try {
                var s = JSON.parse(value)
                page.gateway = s.gateway
                page.timeoutSeconds = s.timeout
                page.retryCount = s.retry_count
                page.retryIntervalSeconds = s.retry_interval
                page.pollIntervalSeconds = s.detection_interval
                page.xrayMode = s.xray_mode
                page.xrayService = s.xray_service
                page.debug = s.debug
                page.xrayEnabled = s.xray_enabled
                page.startXrayAfterLogin = s.start_xray_after_login
                page.stopXrayOnLeave = s.stop_xray_on_leave
                page.logoutBeforeSleep = s.logout_before_sleep
                page.autoLoginAfterWake = s.auto_login_after_wake
                page.loaded = true
                page.message = ""
            } catch (e) { page.message = i18n("Could not read settings: %1", e) }
            page.busy = false
        })
    }
    function save() {
        if (!page.loaded || page.busy) return
        page.busy = true
        page.message = ""
        call("GetSettings", [], function(value) {
            try {
                var s = JSON.parse(value)
                s.gateway = gateway
                s.timeout = timeoutSeconds
                s.retry_count = retryCount
                s.retry_interval = retryIntervalSeconds
                s.detection_interval = pollIntervalSeconds
                s.xray_mode = xrayMode
                s.xray_service = xrayService
                s.debug = debug
                s.xray_enabled = xrayEnabled
                s.start_xray_after_login = startXrayAfterLogin
                s.stop_xray_on_leave = stopXrayOnLeave
                s.logout_before_sleep = logoutBeforeSleep
                s.auto_login_after_wake = autoLoginAfterWake
                call("SetSettings", [JSON.stringify(s)], function(result) {
                    page.busy = false
                    try {
                        var response = JSON.parse(result)
                        page.message = response.ok ? i18n("Settings saved and applied.") : response.message
                    } catch (e) { page.message = result }
                })
            } catch (e) {
                page.busy = false
                page.message = i18n("Could not prepare settings: %1", e)
            }
        })
    }
    Component.onCompleted: load()

    ColumnLayout {
        width: page.width
        spacing: Kirigami.Units.largeSpacing
        Kirigami.FormLayout {
            Layout.fillWidth: true
            Controls.TextField {
                Kirigami.FormData.label: i18n("Sophos gateway:")
                text: page.gateway
                enabled: page.loaded && !page.busy
                onTextEdited: page.gateway = text
            }
            Controls.SpinBox {
                Kirigami.FormData.label: i18n("Request timeout (seconds):")
                from: 1; to: 120; value: page.timeoutSeconds
                enabled: page.loaded && !page.busy
                onValueModified: page.timeoutSeconds = value
            }
            Controls.SpinBox {
                Kirigami.FormData.label: i18n("Portal retry count:")
                from: 0; to: 12; value: page.retryCount
                enabled: page.loaded && !page.busy
                onValueModified: page.retryCount = value
            }
            Controls.SpinBox {
                Kirigami.FormData.label: i18n("Retry interval (seconds):")
                from: 2; to: 600; value: page.retryIntervalSeconds
                enabled: page.loaded && !page.busy
                onValueModified: page.retryIntervalSeconds = value
            }
            Controls.SpinBox {
                Kirigami.FormData.label: i18n("Network check interval (seconds):")
                from: 2; to: 120; value: page.pollIntervalSeconds
                enabled: page.loaded && !page.busy
                onValueModified: page.pollIntervalSeconds = value
            }
            Controls.ComboBox {
                Kirigami.FormData.label: i18n("Xray service mode:")
                model: ["system", "user"]
                currentIndex: model.indexOf(page.xrayMode)
                enabled: page.loaded && !page.busy
                onActivated: page.xrayMode = currentText
            }
            Controls.TextField {
                Kirigami.FormData.label: i18n("Xray unit:")
                text: page.xrayService
                enabled: page.loaded && !page.busy
                onTextEdited: page.xrayService = text
            }
        }
        Controls.CheckBox { text: i18n("Manage Xray automatically"); checked: page.xrayEnabled; enabled: page.loaded && !page.busy; onToggled: page.xrayEnabled = checked }
        Controls.CheckBox { text: i18n("Start Xray after portal login"); checked: page.startXrayAfterLogin; enabled: page.loaded && !page.busy; onToggled: page.startXrayAfterLogin = checked }
        Controls.CheckBox { text: i18n("Stop Xray when leaving JIIT Wi-Fi"); checked: page.stopXrayOnLeave; enabled: page.loaded && !page.busy; onToggled: page.stopXrayOnLeave = checked }
        Controls.CheckBox { text: i18n("Log out before sleep"); checked: page.logoutBeforeSleep; enabled: page.loaded && !page.busy; onToggled: page.logoutBeforeSleep = checked }
        Controls.CheckBox { text: i18n("Authenticate after waking"); checked: page.autoLoginAfterWake; enabled: page.loaded && !page.busy; onToggled: page.autoLoginAfterWake = checked }
        Controls.CheckBox { text: i18n("Debug logging"); checked: page.debug; enabled: page.loaded && !page.busy; onToggled: page.debug = checked }
        RowLayout {
            Controls.Button { text: i18n("Save settings"); enabled: page.loaded && !page.busy; onClicked: page.save() }
            Controls.BusyIndicator { running: page.busy; visible: page.busy; implicitWidth: 28; implicitHeight: 28 }
            Controls.Label { text: page.message; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
}
