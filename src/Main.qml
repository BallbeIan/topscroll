import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win

    visible: true
    width: 440
    height: 820
    minimumWidth: 340
    minimumHeight: 240
    title: "topscroll"
    color: win.bg

    // A calm dark palette; only the accent comes from the desktop theme.
    readonly property color bg: "#0c0c0e"
    readonly property color card: "#151518"
    readonly property color edge: "#212127"
    readonly property color track: "#2b2b32"
    readonly property color muted: "#8e8e98"
    readonly property color danger: "#ff6b61"
    readonly property int corner: 18

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.background: win.bg

    property bool fineTune: false

    readonly property bool problem: backend.needsRestart || ["denied", "uinput", "busy", "stopped"].indexOf(backend.state) >= 0

    // What the big status card says, in plain words.
    readonly property string statusTitle: {
        if (backend.needsRestart) return qsTr("Update ready")
        switch (backend.state) {
        case "active": return qsTr("Scrolling is on")
        case "disabled": return qsTr("Scrolling is off")
        case "searching": return qsTr("Looking for a touchpad...")
        case "denied": return qsTr("Needs permission")
        case "uinput": return qsTr("Can't start")
        case "busy": return qsTr("Touchpad in use")
        default: return qsTr("Not running")
        }
    }
    readonly property string statusHint: {
        if (backend.needsRestart) return qsTr("topscroll was updated. Restart the service to use the new version.")
        switch (backend.state) {
        case "denied": return qsTr("No permission to read the touchpad. Log out and back in, then try again.")
        case "uinput": return qsTr("Can't create virtual input devices. Log out and back in, or run: sudo modprobe uinput")
        case "busy": return qsTr("Another program has taken over the touchpad.")
        case "stopped": return qsTr("The background service isn't running.")
        default: return ""
        }
    }

    function nudge(value, step, lo, hi) { return Math.max(lo, Math.min(hi, value + step)) }

    // --- building blocks ----------------------------------------------------

    component Card: Rectangle {
        default property alias content: inner.data
        property int pad: 18
        Layout.fillWidth: true
        implicitHeight: inner.implicitHeight + pad * 2
        radius: win.corner
        color: win.card
        border.width: 1
        border.color: win.edge
        ColumnLayout {
            id: inner
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: parent.pad
            spacing: 16
        }
    }

    component Divider: Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: win.edge
    }

    component SoftSwitch: Switch {
        id: sw
        implicitWidth: 48
        implicitHeight: 30
        padding: 0
        focusPolicy: Qt.NoFocus
        indicator: Rectangle {
            x: sw.mirrored ? 0 : sw.width - width
            y: (sw.height - height) / 2
            width: 48; height: 30; radius: 15
            color: sw.checked ? theme.accent : win.track
            Behavior on color { ColorAnimation { duration: 150 } }
            Rectangle {
                y: 3
                x: sw.checked ? parent.width - width - 3 : 3
                width: 24; height: 24; radius: 12
                color: sw.checked ? "#0c0c0e" : "#e4e4ea"
                Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
            }
        }
        contentItem: Item {}
    }

    component SoftSlider: Slider {
        id: sl
        implicitHeight: 30
        focusPolicy: Qt.NoFocus
        background: Rectangle {
            x: sl.leftPadding
            y: sl.topPadding + sl.availableHeight / 2 - height / 2
            width: sl.availableWidth
            height: 6; radius: 3
            color: win.track
            Rectangle {
                x: sl.mirrored ? parent.width - width : 0
                width: sl.position * parent.width
                height: parent.height; radius: 3
                color: theme.accent
            }
        }
        handle: Rectangle {
            x: sl.leftPadding + sl.visualPosition * (sl.availableWidth - width)
            y: sl.topPadding + sl.availableHeight / 2 - height / 2
            width: 24; height: 24; radius: 12
            color: "#ffffff"
            scale: sl.pressed ? 1.12 : 1
            Behavior on scale { NumberAnimation { duration: 100 } }
        }
    }

    component PillButton: Button {
        id: pb
        property bool primary: true
        focusPolicy: Qt.NoFocus
        implicitHeight: 36
        leftPadding: 18
        rightPadding: 18
        background: Rectangle {
            radius: height / 2
            color: pb.primary ? theme.accent : win.track
            opacity: pb.down ? 0.75 : 1
        }
        contentItem: Label {
            text: pb.text
            color: pb.primary ? "#0c0c0e" : "#ffffff"
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // A title and optional hint on the left, a switch on the right.
    component SwitchRow: RowLayout {
        id: srow
        property string label
        property string hint: ""
        property bool on
        signal toggled(bool v)
        Layout.fillWidth: true
        spacing: 14
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Label { Layout.fillWidth: true; text: srow.label; font.pixelSize: 15; wrapMode: Text.Wrap }
            Label {
                Layout.fillWidth: true
                visible: srow.hint !== ""
                text: srow.hint
                color: win.muted
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
        }
        SoftSwitch {
            id: sw
            onToggled: srow.toggled(checked)
        }
        Binding { target: sw; property: "checked"; value: srow.on }
    }

    // A title, an optional hint, a slider, and words (not numbers) at each end.
    component SliderRow: ColumnLayout {
        id: row
        property string label
        property string hint: ""
        property string lowText
        property string highText
        property real current
        property alias from: sl.from
        property alias to: sl.to
        property alias stepSize: sl.stepSize
        signal moved(real v)
        Layout.fillWidth: true
        spacing: 3
        Label { Layout.fillWidth: true; text: row.label; font.pixelSize: 15 }
        Label {
            Layout.fillWidth: true
            visible: row.hint !== ""
            text: row.hint
            color: win.muted
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
        SoftSlider {
            id: sl
            Layout.fillWidth: true
            Layout.topMargin: 4
            onMoved: row.moved(value)
        }
        Binding { target: sl; property: "value"; value: row.current }
        RowLayout {
            Layout.fillWidth: true
            Label { text: row.lowText; color: win.muted; font.pixelSize: 11 }
            Item { Layout.fillWidth: true }
            Label { text: row.highText; color: win.muted; font.pixelSize: 11 }
        }
    }

    // --- the window -----------------------------------------------------------

    // Tiled windows can be any size, so everything scrolls when it doesn't fit.
    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        contentHeight: column.implicitHeight + 48
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        // Arabic, Hebrew, Persian and Urdu read right to left.
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            id: column
            x: (scroll.availableWidth - width) / 2
            y: 24
            width: Math.min(scroll.availableWidth - 40, 400)
            spacing: 14

            // --- header ---
            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 4
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label { Layout.fillWidth: true; text: "topscroll"; font.pixelSize: 26; font.weight: Font.Bold }
                    Label { Layout.fillWidth: true; text: qsTr("Scroll with one finger"); color: win.muted; font.pixelSize: 14 }
                }
                RoundButton {
                    text: "?"
                    focusPolicy: Qt.NoFocus
                    flat: true
                    Material.foreground: win.muted
                    onClicked: help.open()
                }
            }

            // --- the gesture, drawn on a touchpad ---
            Card {
                pad: 0
                Item {
                    id: preview
                    Layout.fillWidth: true
                    Layout.preferredHeight: 256

                    Rectangle {
                        id: pad
                        readonly property real mmW: backend.padWidthMm > 0 ? backend.padWidthMm : 110
                        readonly property real mmH: backend.padHeightMm > 0 ? backend.padHeightMm : 70
                        readonly property real px: Math.min((preview.width - 56) / mmW, (preview.height - 100) / mmH)
                        readonly property real stripH: Math.min(height - 2, backend.stripMm * px)
                        readonly property int r: 14
                        property real offset: 0   // where the demo finger is, from the middle of the strip
                        width: mmW * px
                        height: mmH * px
                        x: (preview.width - width) / 2
                        y: 22
                        radius: r
                        border.width: 1
                        border.color: "#34343c"
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#1d1d22" }
                            GradientStop { position: 1; color: "#131316" }
                        }

                        Item {   // clips the strip to the pad's rounded top corners
                            x: 1; y: 1
                            width: parent.width - 2
                            height: pad.stripH
                            clip: true
                            Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

                            Rectangle {
                                width: parent.width
                                height: parent.height + pad.r
                                radius: pad.r - 1
                                color: Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.2)
                            }
                            Rectangle {   // the area cruise ignores, shown while "hold to keep scrolling" is on
                                x: (parent.width - width) / 2
                                width: backend.deadMm * 2 * pad.px
                                height: parent.height
                                color: "#ffffff"
                                opacity: backend.cruise ? 0.12 : 0
                                Behavior on opacity { NumberAnimation { duration: 160 } }
                                Behavior on width { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                            }
                            Label {
                                visible: pad.stripH > 20
                                anchors.verticalCenter: parent.verticalCenter
                                x: 12
                                text: backend.invert ? "↑" : "↓"
                                color: theme.accent
                                font.pixelSize: 15
                            }
                            Label {
                                visible: pad.stripH > 20
                                anchors.verticalCenter: parent.verticalCenter
                                x: parent.width - width - 12
                                text: backend.invert ? "↓" : "↑"
                                color: theme.accent
                                font.pixelSize: 15
                            }
                        }

                        Rectangle {   // the demo finger, with a soft halo
                            id: finger
                            width: 18; height: 18; radius: 9
                            x: pad.width / 2 + pad.offset - width / 2
                            y: Math.max(3, pad.stripH / 2 - height / 2)
                            color: theme.accent
                            visible: backend.enabled
                            Rectangle {
                                anchors.centerIn: parent
                                width: 32; height: 32; radius: 16
                                color: theme.accent
                                opacity: 0.22
                            }
                        }
                    }

                    // Hold to keep scrolling: touch down, drift out of the ignored area, hold, come back, go the other way.
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

                    Label {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 16
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: win.muted
                        font.pixelSize: 13
                        text: backend.invert
                              ? qsTr("Slide right along the shaded strip to scroll down, left to scroll up.")
                              : qsTr("Slide right along the shaded strip to scroll up, left to scroll down.")
                    }
                }
            }

            // --- on / off, and what's wrong when something is ---
            Card {
                border.color: win.problem ? Qt.rgba(1, 0.42, 0.38, 0.55) : win.edge
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Rectangle {
                        implicitWidth: 12; implicitHeight: 12; radius: 6
                        color: win.problem ? win.danger : backend.state === "active" ? theme.accent : win.muted
                        Rectangle {
                            anchors.centerIn: parent
                            width: 24; height: 24; radius: 12
                            color: parent.color
                            opacity: 0.2
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Label { Layout.fillWidth: true; text: win.statusTitle; font.pixelSize: 17; font.weight: Font.DemiBold; wrapMode: Text.Wrap }
                        Label {
                            Layout.fillWidth: true
                            visible: win.statusHint !== ""
                            text: win.statusHint
                            color: win.muted
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: backend.serviceError !== ""
                            text: backend.serviceError
                            color: win.danger
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                    PillButton {
                        visible: backend.state === "stopped"
                        text: qsTr("Start")
                        onClicked: backend.startService()
                    }
                    PillButton {
                        visible: backend.needsRestart
                        text: qsTr("Restart")
                        onClicked: backend.restartService()
                    }
                    SoftSwitch {
                        id: powerSwitch
                        visible: !win.problem
                        onToggled: backend.enabled = checked
                    }
                    Binding { target: powerSwitch; property: "checked"; value: backend.enabled }
                }
            }

            // --- the basics ---
            Card {
                SliderRow {
                    label: qsTr("Speed")
                    lowText: qsTr("Slow"); highText: qsTr("Fast")
                    // Dragging right is faster, so the slider runs opposite to the travel per notch.
                    current: 14 - backend.mmPerNotch
                    from: 2; to: 12; stepSize: 0.5
                    onMoved: v => backend.mmPerNotch = 14 - v
                }
                Divider {}
                SliderRow {
                    label: qsTr("Scroll area")
                    hint: qsTr("The strip along the top edge that scrolls.")
                    lowText: qsTr("Small"); highText: qsTr("Large")
                    current: backend.stripMm
                    from: 4; to: 30; stepSize: 1
                    onMoved: v => backend.stripMm = v
                }
                Divider {}
                SwitchRow {
                    label: qsTr("Smooth stop")
                    hint: qsTr("Keep coasting after you lift your finger, then ease to a stop.")
                    on: backend.inertia
                    onToggled: v => backend.inertia = v
                }
                Divider {}
                SwitchRow {
                    label: qsTr("Hold to keep scrolling")
                    hint: qsTr("Keep your finger down and it keeps scrolling. The further you slide from where you started, the faster it goes. Lift to stop.")
                    on: backend.cruise
                    onToggled: v => backend.cruise = v
                }
                Divider {}
                SwitchRow {
                    label: qsTr("Flip direction")
                    hint: qsTr("Slide right to scroll down instead of up.")
                    on: backend.invert
                    onToggled: v => backend.invert = v
                }
            }

            // --- fine-tuning, out of the way ---
            Card {
                pad: 0
                Item {
                    Layout.fillWidth: true
                    implicitHeight: 56
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: win.fineTune = !win.fineTune
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        Label { Layout.fillWidth: true; text: qsTr("Fine-tune"); font.pixelSize: 15 }
                        Label {
                            text: "›"
                            color: win.muted
                            font.pixelSize: 24
                            rotation: win.fineTune ? 90 : 0
                            Behavior on rotation { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                        }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    Layout.bottomMargin: 18
                    spacing: 16
                    visible: win.fineTune

                    SliderRow {
                        label: qsTr("Smooth stop length")
                        lowText: qsTr("Short"); highText: qsTr("Long")
                        current: backend.glideSeconds
                        from: 0.2; to: 3; stepSize: 0.1
                        enabled: backend.inertia
                        opacity: backend.inertia ? 1 : 0.4
                        onMoved: v => backend.glideSeconds = v
                    }
                    SliderRow {
                        label: qsTr("Ignore small movements")
                        hint: qsTr("Small movements around where you touched down are ignored. Past that, it starts scrolling at once.")
                        lowText: qsTr("Small"); highText: qsTr("Large")
                        current: backend.deadMm
                        from: 0.5; to: 8; stepSize: 0.5
                        enabled: backend.cruise
                        opacity: backend.cruise ? 1 : 0.4
                        onMoved: v => backend.deadMm = v
                    }
                    SliderRow {
                        label: qsTr("Start speed")
                        lowText: qsTr("Slow"); highText: qsTr("Fast")
                        current: backend.cruiseStart
                        from: 0.5; to: 10; stepSize: 0.5
                        enabled: backend.cruise
                        opacity: backend.cruise ? 1 : 0.4
                        onMoved: v => backend.cruiseStart = v
                    }
                    SliderRow {
                        label: qsTr("Top speed")
                        lowText: qsTr("Slow"); highText: qsTr("Fast")
                        current: backend.cruiseMax
                        from: 10; to: 120; stepSize: 5
                        enabled: backend.cruise
                        opacity: backend.cruise ? 1 : 0.4
                        onMoved: v => backend.cruiseMax = v
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            visible: backend.device !== ""
                            text: qsTr("Touchpad: %1").arg(backend.device)
                            color: win.muted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                        Item { Layout.fillWidth: true; visible: backend.device === "" }
                        PillButton {
                            primary: false
                            text: qsTr("Reset to defaults")
                            onClicked: backend.resetDefaults()
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.topMargin: 2
                horizontalAlignment: Text.AlignHCenter
                color: win.muted
                font.pixelSize: 12
                text: qsTr("Press ? for keyboard shortcuts")
            }
        }
    }

    Popup {
        id: help
        objectName: "help"
        modal: true
        anchors.centerIn: parent
        padding: 22
        background: Rectangle { color: win.card; radius: win.corner; border.width: 1; border.color: win.edge }

        GridLayout {
            columns: 2
            columnSpacing: 24
            rowSpacing: 10
            Label { text: qsTr("Keyboard shortcuts"); font.bold: true; font.pixelSize: 16; Layout.columnSpan: 2 }

            Repeater {
                model: [
                    { key: "Space", what: qsTr("Turn scrolling on or off") },
                    { key: "R", what: qsTr("Reverse direction") },
                    { key: "C", what: qsTr("Turn hold to keep scrolling on or off") },
                    { key: "G", what: qsTr("Turn smooth stop on or off") },
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
    Shortcut { sequences: ["G"]; context: Qt.ApplicationShortcut; onActivated: backend.inertia = !backend.inertia }
    Shortcut { sequences: ["R"]; context: Qt.ApplicationShortcut; onActivated: backend.invert = !backend.invert }
    Shortcut { sequences: ["Up"]; context: Qt.ApplicationShortcut; onActivated: backend.stripMm = win.nudge(backend.stripMm, 1, 4, 30) }
    Shortcut { sequences: ["Down"]; context: Qt.ApplicationShortcut; onActivated: backend.stripMm = win.nudge(backend.stripMm, -1, 4, 30) }
    Shortcut { sequences: ["Right"]; context: Qt.ApplicationShortcut; onActivated: backend.mmPerNotch = win.nudge(backend.mmPerNotch, -0.5, 2, 12) }
    Shortcut { sequences: ["Left"]; context: Qt.ApplicationShortcut; onActivated: backend.mmPerNotch = win.nudge(backend.mmPerNotch, 0.5, 2, 12) }
    Shortcut { sequences: ["?", "F1"]; context: Qt.ApplicationShortcut; onActivated: help.opened ? help.close() : help.open() }
    // Settings apply and save as you change them, so there is never unsaved work to ask about.
    Shortcut { sequences: ["Q"]; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
}
