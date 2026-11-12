import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win

    visible: true
    width: 460
    height: 680
    minimumWidth: 400
    minimumHeight: 620
    title: "topscroll"
    color: "#121212"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.background: "#121212"
    Material.roundedScale: theme.radius

    readonly property bool problem: ["denied", "uinput", "busy", "stopped"].indexOf(backend.state) >= 0

    readonly property string statusText: {
        switch (backend.state) {
        case "active": return qsTr("Scrolling is on: %1").arg(backend.device)
        case "disabled": return qsTr("Scrolling is turned off")
        case "searching": return qsTr("Looking for a touchpad...")
        case "denied": return qsTr("No permission to read the touchpad. Log out and back in, then try again.")
        case "uinput": return qsTr("Can't create virtual input devices. Log out and back in, or run: sudo modprobe uinput")
        case "busy": return qsTr("Another program has taken over the touchpad.")
        default: return qsTr("The background service isn't running.")
        }
    }

    function nudge(value, step, lo, hi) { return Math.max(lo, Math.min(hi, value + step)) }

    Item {
        anchors.fill: parent
        // Arabic, Hebrew, Persian and Urdu read right to left.
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 18

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "topscroll"
                    font.pixelSize: 26
                    font.bold: true
                    Layout.fillWidth: true
                }
                Switch {
                    id: enabledSwitch
                    focusPolicy: Qt.NoFocus
                    onToggled: backend.enabled = checked
                }
                Binding { target: enabledSwitch; property: "checked"; value: backend.enabled }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: statusRow.implicitHeight + 24
                radius: theme.radius
                color: "#1b1b1b"
                border.width: 1
                border.color: win.problem ? "#e5534b" : "#2c2c2c"

                RowLayout {
                    id: statusRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 12
                    spacing: 10

                    Rectangle {
                        width: 10; height: 10; radius: 5   // a true circle, so it keeps a fixed radius
                        color: win.problem ? "#e5534b" : backend.state === "active" ? theme.accent : "#777777"
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            Layout.fillWidth: true
                            text: win.statusText
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: backend.serviceError !== ""
                            text: backend.serviceError
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                            color: "#e5534b"
                        }
                    }
                    Button {
                        visible: backend.state === "stopped"
                        text: qsTr("Start")
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.startService()
                    }
                }
            }

            // The touchpad, drawn to scale, with the scroll strip shaded.
            Item {
                id: preview
                Layout.fillWidth: true
                Layout.preferredHeight: 190

                Rectangle {
                    id: pad
                    readonly property real mmW: backend.padWidthMm > 0 ? backend.padWidthMm : 110
                    readonly property real mmH: backend.padHeightMm > 0 ? backend.padHeightMm : 70
                    readonly property real px: Math.min(preview.width / mmW, preview.height / mmH)
                    width: mmW * px
                    height: mmH * px
                    anchors.centerIn: parent
                    radius: Math.min(8, theme.radius)
                    color: "#1b1b1b"
                    border.width: 1
                    border.color: "#3a3a3a"
                    clip: true

                    Rectangle {
                        width: parent.width
                        height: Math.min(parent.height, backend.stripMm * pad.px)
                        color: theme.accent
                        opacity: 0.35
                        Behavior on height { NumberAnimation { duration: 90 } }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                opacity: 0.7
                text: backend.invert
                      ? qsTr("Slide right along the shaded strip to scroll down, left to scroll up.")
                      : qsTr("Slide right along the shaded strip to scroll up, left to scroll down.")
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Strip height"); Layout.fillWidth: true }
                    Label { text: backend.stripMm.toLocaleString(Qt.locale(), 'f', 0) + " mm"; opacity: 0.7 }
                }
                Slider {
                    id: stripSlider
                    Layout.fillWidth: true
                    from: 4; to: 30; stepSize: 1
                    focusPolicy: Qt.NoFocus
                    onMoved: backend.stripMm = value
                }
                Binding { target: stripSlider; property: "value"; value: backend.stripMm }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Scroll speed"); Layout.fillWidth: true }
                    Label { text: qsTr("%1 mm per notch").arg(backend.mmPerNotch.toLocaleString(Qt.locale(), 'f', 1)); opacity: 0.7 }
                }
                Slider {
                    id: speedSlider
                    Layout.fillWidth: true
                    // Dragging right is faster, so the slider runs opposite to the travel per notch.
                    from: 2; to: 12; stepSize: 0.5
                    focusPolicy: Qt.NoFocus
                    onMoved: backend.mmPerNotch = 14 - value
                }
                Binding { target: speedSlider; property: "value"; value: 14 - backend.mmPerNotch }
            }

            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Reverse direction"); Layout.fillWidth: true }
                Switch {
                    id: invertSwitch
                    focusPolicy: Qt.NoFocus
                    onToggled: backend.invert = checked
                }
                Binding { target: invertSwitch; property: "checked"; value: backend.invert }
            }

            Item { Layout.fillHeight: true }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.5
                font.pixelSize: 12
                text: qsTr("Press ? for keyboard shortcuts")
            }
        }
    }

    Popup {
        id: help
        modal: true
        anchors.centerIn: parent
        padding: 20
        background: Rectangle { color: "#1b1b1b"; radius: theme.radius; border.width: 1; border.color: "#3a3a3a" }

        GridLayout {
            columns: 2
            columnSpacing: 24
            rowSpacing: 8
            Label { text: qsTr("Keyboard shortcuts"); font.bold: true; Layout.columnSpan: 2 }

            Repeater {
                model: [
                    { key: "Space", what: qsTr("Turn scrolling on or off") },
                    { key: "R", what: qsTr("Reverse direction") },
                    { key: "Up / Down", what: qsTr("Taller or shorter strip") },
                    { key: "Left / Right", what: qsTr("Slower or faster scrolling") },
                    { key: "?", what: qsTr("Show this help") },
                    { key: "Q", what: qsTr("Quit") }
                ]
                delegate: RowLayout {
                    required property var modelData
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    spacing: 24
                    Label { text: modelData.key; color: theme.accent; Layout.preferredWidth: 90 }
                    Label { text: modelData.what; Layout.fillWidth: true }
                }
            }
        }
    }

    Shortcut { sequences: ["Space"]; context: Qt.ApplicationShortcut; onActivated: backend.enabled = !backend.enabled }
    Shortcut { sequences: ["R"]; context: Qt.ApplicationShortcut; onActivated: backend.invert = !backend.invert }
    Shortcut { sequences: ["Up"]; context: Qt.ApplicationShortcut; onActivated: backend.stripMm = win.nudge(backend.stripMm, 1, 4, 30) }
    Shortcut { sequences: ["Down"]; context: Qt.ApplicationShortcut; onActivated: backend.stripMm = win.nudge(backend.stripMm, -1, 4, 30) }
    Shortcut { sequences: ["Right"]; context: Qt.ApplicationShortcut; onActivated: backend.mmPerNotch = win.nudge(backend.mmPerNotch, -0.5, 2, 12) }
    Shortcut { sequences: ["Left"]; context: Qt.ApplicationShortcut; onActivated: backend.mmPerNotch = win.nudge(backend.mmPerNotch, 0.5, 2, 12) }
    Shortcut { sequences: ["?", "F1"]; context: Qt.ApplicationShortcut; onActivated: help.opened ? help.close() : help.open() }
    // Settings apply and save as you change them, so there is never unsaved work to ask about.
    Shortcut { sequences: ["Q"]; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
}
