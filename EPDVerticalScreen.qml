import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtCore

Item {
    id: verticalRoot

    //Virtual design resolution
    readonly property real designWidth: 720
    readonly property real designHeight: 1280

    //Server trigger via c++ Signal
    Connections {
        target: (typeof busController !== "undefined") ? busController : null
        function onRequestScreenCapture(filePath) {
            viewboxContainer.captureQhdScreenshot(filePath);
        }

        function onDataUpdated() {
            autoCaptureTimer.restart();
        }
    }

    Item {
        id: viewboxContainer
        width: verticalRoot.designWidth
        height: verticalRoot.designHeight
        anchors.centerIn: parent

        // Scale to fit whatever size the Loader gives this screen
        scale: Math.min(verticalRoot.width / verticalRoot.designWidth, verticalRoot.height / verticalRoot.designHeight)

        function captureQhdScreenshot(savePath) {
            bitMainContent.grabToImage(function (result) {
                var success = result.saveToFile(savePath);
                if (success) {
                    console.log("QHD Screenshot saved successfully to:", savePath);
                    busController.processCapturedImage(result.image);
                } else {
                    console.error("Failed to save QHD screenshot.");
                }
            }, Qt.size(1440, 2560));
        }

        function captureAndSendToDaemon() {
            bitMainContent.grabToImage(function (result) {
                if (typeof busController !== "undefined" && busController) {
                    console.log("sending image to daemon");
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

        Shortcut {
            sequence: "F12"
            context: Qt.ApplicationShortcut
            onActivated: {
                var picturesDir = StandardPaths.writableLocation(StandardPaths.PicturesLocation).toString();
                var cleanPath = picturesDir.replace(/^(file:\/{3})/, "");
                if (Qt.platform.os !== "windows" && !cleanPath.startsWith("/")) {
                    cleanPath = "/" + cleanPath;
                }
                var timestamp = Qt.formatDateTime(new Date(), "yyyyMMdd_HHmmss");
                var fullFilePath = cleanPath + "/BIT_Capture_" + timestamp + ".png";
                viewboxContainer.captureQhdScreenshot(fullFilePath);
            }
        }

        Shortcut {
            sequence: "F11"
            context: Qt.ApplicationShortcut
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
                        anchors.centerIn: parent
                        //anchors.left: parent.left
                        //anchors.leftMargin: 24
                        text: busController ? busController.stationName : "Preview Station"
                        color: "black"
                        font.pixelSize: 36
                        font.bold: true
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 80
                    color: "white"
                    border.color: "black"
                    radius: 10


                    Row {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 14
                            spacing: 6

                            Text {
                                id: arriveSoon
                                text: "Arrive Soon:"
                                font.pixelSize: 24
                            }

                            Text {
                                id: buses
                                text: busController ? busController.arriveSoonBuses : "9"
                                font.pixelSize: 24
                            }
                        }
                }

                // 2. Main Content: Arriving Buses List
                ListView {
                    id: arrivalListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 12

                    model: busController ? busController.busList : []

                    delegate: Rectangle {
                        width: arrivalListView.width
                        height: 180
                        color: "white"
                        radius: 8
                        border.color: "black"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Rectangle {
                                width: 100
                                height: 100
                                border.color: "black"


                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.routeNumber
                                    color: "black"
                                    font.pixelSize: 60
                                    font.bold: true
                                }
                            }

                            ColumnLayout {
                                Layout.alignment: Qt.AlignRight
                                spacing: 2



                                Text {
                                    Layout.alignment: Qt.AlignRight
                                    text: modelData.etaMinutes <= 1 ? "Arriving Soon" : modelData.etaMinutes + " min"
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


                            }



                        }

                        Text {
                            text: "To: " + modelData.destination
                            color: "black"
                            font.pixelSize: 26
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            anchors.bottom: parent.bottom
                            leftPadding: 15
                            bottomPadding: 5

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
