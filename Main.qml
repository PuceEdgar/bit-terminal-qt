import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtCore


ApplicationWindow {
    id: rootWindow
    visible: true
    title: "Bus Information Terminal"
    color: "white"

    // Read directly from C++ BusApiController (falls back to "vertical" in QML preview)
    readonly property string screenMode: (typeof busController !== "undefined" && busController)
                                         ? busController.screenMode : "vertical"

    width: screenMode === "horizontal" ? 1280 : (screenMode === "led" ? 640 : 720)
    height: screenMode === "horizontal" ? 720 : (screenMode === "led" ? 320 : 1280)

    Shortcut {
        sequences: ["M", "F1", "Ctrl+M"]
        context: Qt.ApplicationShortcut
        onActivated: menuWindow.toggle()
    }

    MenuWindow {
        id: menuWindow
        currentMode: rootWindow.screenMode
        onModeSelected: (newMode) => {
            if (typeof busController !== "undefined" && busController) {
                // Updates C++ property AND writes Display/ScreenMode to config.ini
                busController.screenMode = newMode
            }
        }
    }

    Loader {
        id: screenLoader
        anchors.fill: parent
        source: {
            if (rootWindow.screenMode === "horizontal")
                return "EPDHorizontalScreen.qml";
            if (rootWindow.screenMode === "led")
                return "LEDScreen.qml";
            return "EPDVerticalScreen.qml";
        }
    }
}