import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Window {
    id: root
    width: 360
    height: 280
    title: "Screen Settings"
    visible: false
    flags: Qt.Dialog

    // These two declarations fix the "invalid properties" error in Main.qml
    property string currentMode: "vertical"
    signal modeSelected(string newMode)

    function toggle() {
        if (visible) {
            close()
        } else {
            show()
            raise()
            requestActivate()
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width - 48
        spacing: 12

        Label {
            text: "Select Screen Layout"
            font.bold: true
            font.pixelSize: 18
            Layout.alignment: Qt.AlignHCenter
        }

        Button {
            text: "EPD Vertical (720x1280)"
            highlighted: root.currentMode === "vertical"
            Layout.fillWidth: true
            onClicked: root.modeSelected("vertical")
        }

        Button {
            text: "EPD Horizontal (1280x720)"
            highlighted: root.currentMode === "horizontal"
            Layout.fillWidth: true
            onClicked: root.modeSelected("horizontal")
        }

        Button {
            text: "LED Screen (640x320)"
            highlighted: root.currentMode === "led"
            Layout.fillWidth: true
            onClicked: root.modeSelected("led")
        }

        Button {
            text: "Close"
            Layout.fillWidth: true
            Layout.topMargin: 8
            onClicked: root.close()
        }
    }
}