import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtCore

ApplicationWindow {
    id: rootWindow
    width: 720
    height: 1280
    visible: true
    title: "Bus Information Terminal"
    color: "white"

    // Virtual design resolution (FHD)
    readonly property real designWidth: 720
    readonly property real designHeight: 1280

    // 2. Server Trigger via C++ Signal
    Connections {
        target: (typeof busController !== "undefined") ? busController : null
        function onRequestScreenCapture(filePath) {
            viewboxContainer.captureQhdScreenshot(filePath);
        }

        function onDataUpdated() {
            //restart the 400 ms timer when bus data arrived
            autoCaptureTimer.restart();
        }
    }
    // Equivalent to WPF Viewbox (Uniform Stretch)

    Item {
        id: viewboxContainer
        width: designWidth
        height: designHeight
        anchors.centerIn: parent

        // Uniformly scale to fit the actual window size while keeping aspect ratio
        scale: Math.min(rootWindow.width / designWidth, rootWindow.height / designHeight)

        function captureQhdScreenshot(savePath) {
            // Qt.size(2560, 1440) forces off-screen rendering at QHD resolution
            bitMainContent.grabToImage(function (result) {
                var success = result.saveToFile(savePath);
                if (success) {
                    console.log("QHD Screenshot saved successfully to:", savePath);
                    // Notify C++ backend to upload this image to the city server
                    busController.processCapturedImage(result.image);
                } else {
                    console.error("Failed to save QHD screenshot.");
                }
            }, Qt.size(1440, 2560));
        }

        function captureAndSendToDaemon() {
                bitMainContent.grabToImage(function(result) {
                    if (typeof busController !== "undefined" && busController) {
                        busController.sendImageToDaemon(result.image);
                    } else {
                        console.log("Preview mode: Captured QHD frame.");
                    }
                }, Qt.size(1440, 2560));
            }

        Timer {
            id: autoCaptureTimer
            interval: 400
            repeat: false
            onTriggered: viewboxContainer.captureAndSendToDaemon()
        }

        // 1. Keyboard Shortcut to trigger capture manually (e.g., pressing F12)
        Shortcut {
            sequence: "F12" // You can also use "Ctrl+P" or "Ctrl+Shift+S"
            onActivated: {
                // Get the system's Pictures folder (works on both Windows & Raspberry Pi)
                // StandardPaths returns a URL like "file:///C:/Users/.../Pictures", so we strip "file:///"
                var picturesDir = StandardPaths.writableLocation(StandardPaths.PicturesLocation).toString();
                var cleanPath = picturesDir.replace(/^(file:\/{3})/, "");

                // On Linux/Pi, keep the leading slash (e.g., "/home/pi/Pictures")
                if (Qt.platform.os !== "windows" && !cleanPath.startsWith("/")) {
                    cleanPath = "/" + cleanPath;
                }

                // Generate a timestamped filename
                var timestamp = Qt.formatDateTime(new Date(), "yyyyMMdd_HHmmss");
                var fullFilePath = cleanPath + "/BIT_Capture_" + timestamp + ".png";

                console.log("Triggering manual capture to:", fullFilePath);
                viewboxContainer.captureQhdScreenshot(fullFilePath);
            }
        }

        Shortcut {
            sequence: "F11"
            onActivated: viewboxContainer.captureAndSendToDaemon()
        }

        Rectangle {
            id: bitMainContent
            anchors.fill: parent
            color: "white"

        ColumnLayout {

            anchors.fill: parent
            anchors.margins: 24
            spacing: 16


            // 1. Header: Station Name
            Rectangle {
                Layout.fillWidth: true
                height: 80
                color: "white"
                radius: 10
                border.color: "black"

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 24
                    text: busController ? busController.stationName : "Preview Station"
                    color: "black"
                    font.pixelSize: 36
                    font.bold: true
                }
            }

            // 2. Main Content: Arriving Buses List
            ListView {
                id: arrivalListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 12

                // Bind directly to the C++ QVariantList
                model: busController ? busController.busList : []

                delegate: Rectangle {
                    width: arrivalListView.width
                    height: 150
                    color: "white"
                    radius: 8
                    border.color: "black"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 10

                        // Bus Route Number Badge
                        Rectangle {
                            width: 100
                            height: 58

                            Text {
                                anchors.centerIn: parent
                                text: modelData.routeNumber
                                color: "black"
                                font.pixelSize: 60
                                font.bold: true
                            }
                        }



                        // ETA & Remaining Stops
                        ColumnLayout {
                            Layout.alignment: Qt.AlignRight
                            spacing: 2

                            Text {
                                Layout.alignment: Qt.AlignRight
                                // Highlight buses arriving within 3 minutes in amber/red
                                text: modelData.etaMinutes <= 1 ? "Arriving Soon" : modelData.etaMinutes + " sec"
                                color: "black"
                                font.pixelSize: 32
                                font.bold: true
                            }

                            Text {
                                Layout.alignment: Qt.AlignRight
                                text: modelData.remainingStops + " stops away"
                                color: "black"
                                font.pixelSize: 18
                            }

                            // Destination
                            Text {
                                Layout.fillWidth: true
                                text: "To: " + modelData.destination
                                color: "black"
                                font.pixelSize: 26
                            }
                        }
                    }
                }
            }

            // 3. Footer: Connection Status
            Text {
                Layout.alignment: Qt.AlignRight
                text: "Status: " + (busController ? busController.statusMessage : "Offline")
                color: "black"
                font.pixelSize: 16
            }
        }

        }
        }
}
