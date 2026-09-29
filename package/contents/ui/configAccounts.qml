import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus

Kirigami.ScrollablePage {
    id: page
    title: i18n("Accounts")
    property bool busy: false
    property string message: ""

    function call(method, args, done) {
        SessionBus.asyncCall({service: "org.jiit.FirewallManager", path: "/org/jiit/FirewallManager", iface: "org.jiit.FirewallManager", member: method, arguments: args || []},
            function(reply) { if (done) done(reply.values.length ? String(reply.values[0]) : "") },
            function(error) { page.busy = false; page.message = error.message })
    }
    function load() {
        page.busy = true
        call("GetSettings", [], function(value) {
            try {
                var settings = JSON.parse(value)
                accountModel.clear()
                var ordered = []
                for (var i = 0; i < settings.accounts.length; ++i)
                    ordered.push({account: settings.accounts[i], sequence: i})
                ordered.sort(function(a, b) {
                    return Number(a.account.priority) - Number(b.account.priority) || a.sequence - b.sequence
                })
                for (var j = 0; j < ordered.length; ++j) {
                    var a = ordered[j].account
                    accountModel.append({accountKey: a.id, accountName: a.name, username: a.username, priority: a.priority, accountEnabled: a.enabled})
                }
                page.message = ""
            } catch (e) { page.message = i18n("Could not read accounts: %1", e) }
            page.busy = false
        })
    }
    function saveAccount(data, done) {
        page.busy = true
        call("SaveAccount", [JSON.stringify(data)], function(value) {
            page.busy = false
            try {
                var response = JSON.parse(value)
                page.message = response.message || ""
                if (response.ok) page.load()
                if (done) done(response.ok)
            } catch (e) { page.message = value; if (done) done(false) }
        })
    }
    function saveRow(index, passwordField) {
        var a = accountModel.get(index)
        saveAccount({id: a.accountKey, name: a.accountName, username: a.username, password: passwordField.text, priority: a.priority, enabled: a.accountEnabled}, function(ok) {
            if (ok) passwordField.clear()
        })
    }
    function addAccount() {
        saveAccount({id: "", name: newName.text, username: newUsername.text, password: newPassword.text, priority: accountModel.count + 1, enabled: newEnabled.checked}, function(ok) {
            if (ok) {
                newName.clear(); newUsername.clear(); newPassword.clear(); newEnabled.checked = true
                accountDialog.close()
            }
        })
    }

    ListModel { id: accountModel }
    Component.onCompleted: page.load()

    ColumnLayout {
        width: page.width
        spacing: Kirigami.Units.largeSpacing
        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18n("Enabled accounts are tried in priority order (1 is first). Saving a priority moves that account in this list. Passwords are stored in KDE Wallet.")
        }
        Controls.Button { text: i18n("Add account"); enabled: !page.busy; onClicked: accountDialog.open() }
        Repeater {
            model: accountModel
            delegate: Controls.GroupBox {
                required property int index
                required property string accountKey
                required property string accountName
                required property string username
                required property int priority
                required property bool accountEnabled
                Layout.fillWidth: true
                title: accountName
                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    Controls.TextField {
                        id: nameField
                        Layout.fillWidth: true
                        placeholderText: i18n("Account label")
                        text: accountName
                        onTextEdited: accountModel.setProperty(index, "accountName", text)
                    }
                    Controls.TextField {
                        id: userField
                        Layout.fillWidth: true
                        placeholderText: i18n("Sophos login ID")
                        text: username
                        onTextEdited: accountModel.setProperty(index, "username", text)
                    }
                    Controls.TextField {
                        id: passwordField
                        Layout.fillWidth: true
                        placeholderText: i18n("New password (leave blank to keep saved password)")
                        echoMode: TextInput.Password
                    }
                    RowLayout {
                        Controls.SpinBox {
                            from: 1; to: 99; value: priority
                            Accessible.name: i18n("Login priority")
                            onValueModified: accountModel.setProperty(index, "priority", value)
                        }
                        Controls.CheckBox {
                            text: i18n("Enabled")
                            checked: accountEnabled
                            onToggled: accountModel.setProperty(index, "accountEnabled", checked)
                        }
                        Item { Layout.fillWidth: true }
                        Controls.Button { text: i18n("Save"); enabled: !page.busy; onClicked: page.saveRow(index, passwordField) }
                        Controls.Button {
                            text: i18n("Remove"); enabled: !page.busy
                            onClicked: {
                                page.busy = true
                                page.call("RemoveAccount", [accountKey], function(value) {
                                    page.busy = false
                                    try { var result = JSON.parse(value); page.message = result.message || ""; if (result.ok) page.load() }
                                    catch (e) { page.message = value }
                                })
                            }
                        }
                    }
                }
            }
        }
        RowLayout {
            Controls.Button {
                text: i18n("Try enabled accounts now")
                enabled: !page.busy && accountModel.count > 0
                onClicked: page.call("Login", [], function(value) {
                    try { var result = JSON.parse(value); page.message = result.message || "" }
                    catch (e) { page.message = value }
                })
            }
            Controls.BusyIndicator { running: page.busy; visible: page.busy; implicitWidth: 28; implicitHeight: 28 }
            Controls.Label { text: page.message; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }

    Controls.Dialog {
        id: accountDialog
        title: i18n("Add Sophos account")
        modal: true
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        onAccepted: page.addAccount()
        contentItem: ColumnLayout {
            Controls.TextField { id: newName; Layout.fillWidth: true; placeholderText: i18n("Account label") }
            Controls.TextField { id: newUsername; Layout.fillWidth: true; placeholderText: i18n("Sophos login ID") }
            Controls.TextField { id: newPassword; Layout.fillWidth: true; placeholderText: i18n("Password"); echoMode: TextInput.Password }
            Controls.CheckBox { id: newEnabled; text: i18n("Enabled"); checked: true }
        }
    }
}
