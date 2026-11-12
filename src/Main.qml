import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win

    visible: true
    width: 480
    height: 860
    minimumWidth: 360
    minimumHeight: 240
    title: "topscroll"
    color: win.bg

    readonly property color bg: "#101010"
    readonly property color card: "#181818"
    readonly property color line: "#262626"
    readonly property color danger: "#e5534b"
    readonly property color muted: "#8c8c8c"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.background: win.bg
    Material.roundedScale: theme.radius

    readonly property bool problem: backend.needsRestart || ["denied", "uinput", "busy", "stopped"].indexOf(backend.state) >= 0

    readonly property string statusText: {
        if (backend.needsRestart)
            return qsTr("topscroll was updated. Restart the service to use the new version.")
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
    function mm(v, digits) { return qsTr("%1 mm").arg(Number(v).toLocaleString(Qt.locale(), 'f', digits)) }
    function perSecond(v, digits) { return qsTr("%1 notches per second").arg(Number(v).toLocaleString(Qt.locale(), 'f', digits)) }

    // A rounded panel that stacks its children, with an optional title.
    component Card: Rectangle {
        id: card
        property string title: ""
        default property alias content: inner.data
        Layout.fillWidth: true
        implicitHeight: inner.implicitHeight + 32
        radius: theme.radius
        color: win.card
        border.width: 1
        border.color: win.line

        ColumnLayout {
            id: inner
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            spacing: 14

            Label {
                visible: card.title !== ""
                text: card.title
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
                color: win.muted
            }
        }
    }

    // A label with its current value on the right and a slider underneath.
    component SliderRow: ColumnLayout {
        id: row
        property string label
        property string valueText
        property real current
        property alias from: sl.from
        property alias to: sl.to
        property alias stepSize: sl.stepSize
        signal moved(real v)
        Layout.fillWidth: true
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Label { text: row.label; Layout.fillWidth: true }
            Label { text: row.valueText; color: theme.accent; font.weight: Font.Medium }
        }
        Slider {
            id: sl
            Layout.fillWidth: true
            focusPolicy: Qt.NoFocus
            onMoved: row.moved(value)
        }
        Binding { target: sl; property: "value"; value: row.current }
    }

    // A label with a switch on the right, and an optional description under it.
    component SwitchRow: ColumnLayout {
        id: srow
        property string label
        property string hint: ""
        property bool on
        property alias bold: title.font.bold
        signal toggled(bool v)
        Layout.fillWidth: true
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            Label { id: title; text: srow.label; Layout.fillWidth: true }
            Switch {
                id: sw
                focusPolicy: Qt.NoFocus
                onToggled: srow.toggled(checked)
            }
            Binding { target: sw; property: "checked"; value: srow.on }
        }
        Label {
            Layout.fillWidth: true
            visible: srow.hint !== ""
            text: srow.hint
            wrapMode: Text.Wrap
            color: win.muted
            font.pixelSize: 12
        }
    }

    // Tiled windows can be any size, so everything scrolls when it doesn't fit.
    ScrollView {
        id: scroll
        anchors.fill: parent
        padding: 20
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        // Arabic, Hebrew, Persian and Urdu read right to left.
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 14

            // --- header ---------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 2
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        text: "topscroll"
                        font.pixelSize: 28
                        font.weight: Font.Bold
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Scroll with one finger")
                        color: win.muted
                    }
                }
                Switch {
                    id: enabledSwitch
                    focusPolicy: Qt.NoFocus
                    onToggled: backend.enabled = checked
                }
                Binding { target: enabledSwitch; property: "checked"; value: backend.enabled }
            }

            // --- status ---------------------------------------------------
            Card {
                border.color: win.problem ? Qt.rgba(0.9, 0.33, 0.29, 0.6) : win.line
                implicitHeight: statusRow.implicitHeight + 28

                RowLayout {
                    id: statusRow
                    Layout.fillWidth: true
                    spacing: 12

                    Item {
                        implicitWidth: 14; implicitHeight: 14
                        Rectangle {   // a soft halo around a solid dot
                            anchors.centerIn: parent
                            width: 14; height: 14; radius: 7
                            color: win.problem ? win.danger : backend.state === "active" ? theme.accent : win.muted
                            opacity: 0.25
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: 8; height: 8; radius: 4
                            color: win.problem ? win.danger : backend.state === "active" ? theme.accent : win.muted
                        }
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
                            color: win.danger
                        }
                    }
                    Button {
                        visible: backend.state === "stopped"
                        text: qsTr("Start")
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.startService()
                    }
                    Button {
                        visible: backend.needsRestart
                        text: qsTr("Restart")
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.restartService()
                    }
                }
            }

            // --- touchpad preview ----------------------------------------
            // The touchpad drawn to scale. A finger demonstrates the gesture;
            // with cruise on, the band around where it lands is the dead zone.
            Card {
                id: previewCard
                Item {
                    id: preview
                    Layout.fillWidth: true
                    Layout.preferredHeight: 176

                    Rectangle {
                        id: pad
                        readonly property real mmW: backend.padWidthMm > 0 ? backend.padWidthMm : 110
                        readonly property real mmH: backend.padHeightMm > 0 ? backend.padHeightMm : 70
                        readonly property real px: Math.min(preview.width / mmW, preview.height / mmH)
                        // Where the demo finger is, as an offset from the middle of the strip.
                        property real offset: 0
                        width: mmW * px
                        height: mmH * px
                        anchors.centerIn: parent
                        radius: Math.min(10, theme.radius + 2)
                        color: "#111111"
                        border.width: 1
                        border.color: "#363636"
                        clip: true

                        readonly property real stripH: Math.min(height, backend.stripMm * px)

                        Rectangle {   // the scroll strip, inset so the pad's border stays visible
                            x: 1; y: 1
                            width: parent.width - 2
                            height: pad.stripH
                            color: Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.16)
                            Behavior on height { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

                            Rectangle {   // its lower edge
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: theme.accent
                                opacity: 0.5
                            }
                            Rectangle {   // the dead zone, shown while cruise is on
                                anchors.verticalCenter: parent.verticalCenter
                                x: (pad.width - 2) / 2 - width / 2
                                width: backend.deadMm * 2 * pad.px
                                height: parent.height
                                color: "#ffffff"
                                opacity: backend.cruise ? 0.14 : 0
                                Behavior on opacity { NumberAnimation { duration: 150 } }
                                Behavior on width { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                            }
                            Label {
                                visible: pad.stripH > 20
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                text: backend.invert ? "↑" : "↓"
                                color: theme.accent
                                font.pixelSize: 15
                            }
                            Label {
                                visible: pad.stripH > 20
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                text: backend.invert ? "↓" : "↑"
                                color: theme.accent
                                font.pixelSize: 15
                            }
                        }

                        Rectangle {   // the demo finger
                            width: 16; height: 16; radius: 8
                            x: pad.width / 2 + pad.offset - width / 2
                            y: Math.max(2, pad.stripH / 2 - height / 2)
                            color: theme.accent
                            border.width: 3
                            border.color: Qt.rgba(1, 1, 1, 0.25)
                            visible: backend.enabled
                        }
                    }

                    // Cruise: touch down, drift out of the dead zone, hold, return, repeat the other way.
                    SequentialAnimation {
                        id: cruiseDemo
                        running: win.visible && backend.enabled && backend.cruise
                        loops: Animation.Infinite
                        readonly property real out: Math.min(pad.width * 0.42, (backend.deadMm + 14) * pad.px)
                        PauseAnimation { duration: 700 }
                        NumberAnimation { target: pad; property: "offset"; to: cruiseDemo.out; duration: 650; easing.type: Easing.InOutQuad }
                        PauseAnimation { duration: 1000 }
                        NumberAnimation { target: pad; property: "offset"; to: 0; duration: 650; easing.type: Easing.InOutQuad }
                        PauseAnimation { duration: 500 }
                        NumberAnimation { target: pad; property: "offset"; to: -cruiseDemo.out; duration: 650; easing.type: Easing.InOutQuad }
                        PauseAnimation { duration: 1000 }
                        NumberAnimation { target: pad; property: "offset"; to: 0; duration: 650; easing.type: Easing.InOutQuad }
                    }
                    // Normal scrolling: slide back and forth along the strip.
                    SequentialAnimation {
                        id: slideDemo
                        running: win.visible && backend.enabled && !backend.cruise
                        loops: Animation.Infinite
                        readonly property real out: pad.width * 0.36
                        NumberAnimation { target: pad; property: "offset"; to: -slideDemo.out; duration: 900; easing.type: Easing.InOutSine }
                        NumberAnimation { target: pad; property: "offset"; to: slideDemo.out; duration: 1300; easing.type: Easing.InOutSine }
                        NumberAnimation { target: pad; property: "offset"; to: 0; duration: 900; easing.type: Easing.InOutSine }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: win.muted
                    text: backend.invert
                          ? qsTr("Slide right along the shaded strip to scroll down, left to scroll up.")
                          : qsTr("Slide right along the shaded strip to scroll up, left to scroll down.")
                }
            }

            // --- scrolling ------------------------------------------------
            Card {
                title: qsTr("Scrolling")

                SliderRow {
                    label: qsTr("Strip height")
                    valueText: win.mm(backend.stripMm, 0)
                    current: backend.stripMm
                    from: 4; to: 30; stepSize: 1
                    onMoved: v => backend.stripMm = v
                }
                SliderRow {
                    label: qsTr("Scroll speed")
                    valueText: qsTr("%1 mm per notch").arg(backend.mmPerNotch.toLocaleString(Qt.locale(), 'f', 1))
                    // Dragging right is faster, so the slider runs opposite to the travel per notch.
                    current: 14 - backend.mmPerNotch
                    from: 2; to: 12; stepSize: 0.5
                    onMoved: v => backend.mmPerNotch = 14 - v
                }
                SwitchRow {
                    label: qsTr("Reverse direction")
                    on: backend.invert
                    onToggled: v => backend.invert = v
                }
            }

            // --- cruise ---------------------------------------------------
            Card {
                title: qsTr("Cruise")

                SwitchRow {
                    label: qsTr("Cruise")
                    bold: true
                    hint: qsTr("Keep your finger down and it keeps scrolling. The further you slide from where you started, the faster it goes. Lift to stop.")
                    on: backend.cruise
                    onToggled: v => backend.cruise = v
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    enabled: backend.cruise
                    opacity: backend.cruise ? 1 : 0.4
                    Behavior on opacity { NumberAnimation { duration: 150 } }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: win.muted
                        font.pixelSize: 12
                        text: qsTr("Small movements around where you touched down are ignored. Past that, it starts scrolling at once.")
                    }
                    SliderRow {
                        label: qsTr("Dead zone")
                        valueText: win.mm(backend.deadMm, 1)
                        current: backend.deadMm
                        from: 0.5; to: 8; stepSize: 0.5
                        onMoved: v => backend.deadMm = v
                    }
                    SliderRow {
                        label: qsTr("Start speed")
                        valueText: win.perSecond(backend.cruiseStart, 1)
                        current: backend.cruiseStart
                        from: 0.5; to: 10; stepSize: 0.5
                        onMoved: v => backend.cruiseStart = v
                    }
                    SliderRow {
                        label: qsTr("Top speed")
                        valueText: win.perSecond(backend.cruiseMax, 0)
                        current: backend.cruiseMax
                        from: 10; to: 120; stepSize: 5
                        onMoved: v => backend.cruiseMax = v
                    }
                }
            }

            // --- footer ---------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 2
                Label {
                    Layout.fillWidth: true
                    color: win.muted
                    font.pixelSize: 12
                    text: qsTr("Press ? for keyboard shortcuts")
                }
                Button {
                    flat: true
                    text: qsTr("Reset to defaults")
                    focusPolicy: Qt.NoFocus
                    onClicked: backend.resetDefaults()
                }
            }
        }
    }

    Popup {
        id: help
        modal: true
        anchors.centerIn: parent
        padding: 20
        background: Rectangle { color: win.card; radius: theme.radius; border.width: 1; border.color: "#3a3a3a" }

        GridLayout {
            columns: 2
            columnSpacing: 24
            rowSpacing: 8
            Label { text: qsTr("Keyboard shortcuts"); font.bold: true; Layout.columnSpan: 2 }

            Repeater {
                model: [
                    { key: "Space", what: qsTr("Turn scrolling on or off") },
                    { key: "R", what: qsTr("Reverse direction") },
                    { key: "C", what: qsTr("Turn cruise on or off") },
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
    Shortcut { sequences: ["C"]; context: Qt.ApplicationShortcut; onActivated: backend.cruise = !backend.cruise }
    Shortcut { sequences: ["R"]; context: Qt.ApplicationShortcut; onActivated: backend.invert = !backend.invert }
    Shortcut { sequences: ["Up"]; context: Qt.ApplicationShortcut; onActivated: backend.stripMm = win.nudge(backend.stripMm, 1, 4, 30) }
    Shortcut { sequences: ["Down"]; context: Qt.ApplicationShortcut; onActivated: backend.stripMm = win.nudge(backend.stripMm, -1, 4, 30) }
    Shortcut { sequences: ["Right"]; context: Qt.ApplicationShortcut; onActivated: backend.mmPerNotch = win.nudge(backend.mmPerNotch, -0.5, 2, 12) }
    Shortcut { sequences: ["Left"]; context: Qt.ApplicationShortcut; onActivated: backend.mmPerNotch = win.nudge(backend.mmPerNotch, 0.5, 2, 12) }
    Shortcut { sequences: ["?", "F1"]; context: Qt.ApplicationShortcut; onActivated: help.opened ? help.close() : help.open() }
    // Settings apply and save as you change them, so there is never unsaved work to ask about.
    Shortcut { sequences: ["Q"]; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
}
