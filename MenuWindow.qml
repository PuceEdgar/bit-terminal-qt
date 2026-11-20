// MenuWindow.qml
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Window {
    id: root
    width: 400
    height: 440
    title: "Terminal Settings & Diagnostics"
    visible: false
    flags: Qt.Dialog

    property string currentMode: "vertical"
    signal modeSelected(string newMode)

    function toggle() {
        if (visible) {
            close()
        } else {
            if (typeof busController !== "undefined" && busController) {
                busController.requestBoardStatus()
            }
            show()
            raise()
            requestActivate()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        // 1. Screen Mode Section
        Label {
            text: "Screen Layout Mode"
            font.bold: true
            font.pixelSize: 16
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

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#CCCCCC"
        }

        // 2. Live ENV Board Diagnostics
        Label {
            text: "Environment Board Status"
            font.bold: true
            font.pixelSize: 16
        }

        GridLayout {
            columns: 2
            Layout.fillWidth: true
            rowSpacing: 6
            columnSpacing: 12

            Label { text: "Serial Board:" }
            Label {
                text: (typeof busController !== "undefined" && busController && busController.boardConnected)
                      ? "Connected" : "Disconnected"
                color: (typeof busController !== "undefined" && busController && busController.boardConnected)
                       ? "green" : "red"
                font.bold: true
            }

            Label { text: "Battery:" }
            Label {
                text: (typeof busController !== "undefined" && busController)
                      ? busController.batteryLevel + " %" : "--"
            }

            Label { text: "Temperature:" }
            Label {
                text: (typeof busController !== "undefined" && busController)
                      ? busController.boardTemperature + " °C" : "--"
            }

            Label { text: "Humidity:" }
            Label {
                text: (typeof busController !== "undefined" && busController)
                      ? busController.boardHumidity + " %" : "--"
            }
        }

        // 3. Hardware Volume Control (0 - 10)
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label { text: "Board Volume:" }

            Slider {
                id: volumeSlider
                Layout.fillWidth: true
                from: 0
                to: 10
                stepSize: 1
                value: 5
                onMoved: {
                    if (typeof busController !== "undefined" && busController) {
                        busController.setBoardVolume(Math.round(value))
                    }
                }
            }

            Label {
                text: Math.round(volumeSlider.value)
                Layout.preferredWidth: 24
            }
        }

        Item { Layout.fillHeight: true }

        Button {
            text: "Close"
            Layout.fillWidth: true
            onClicked: root.close()
        }
    }
}
