/*
    DroidStar recording-library browser.
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var libraryModel

    readonly property color pageColor: "#211f25"
    readonly property color cardColor: "#302c36"
    readonly property color selectedCardColor: "#3c3549"
    readonly property color accentColor: "#8b67d1"
    readonly property color rxColor: "#4682b4"
    readonly property color txColor: "#8b5fc7"
    readonly property color secondaryText: "#c9c3d2"
    readonly property color mutedText: "#9e96aa"

    function detailsText(
        talkgroup,
        sourceId,
        callsign,
        reflector,
        metadata,
        location
    ) {
        var parts = []

        if (talkgroup && talkgroup.length > 0) {
            parts.push("TG " + talkgroup)
        }

        if (sourceId && sourceId.length > 0) {
            parts.push("ID " + sourceId)
        }

        if (reflector && reflector.length > 0) {
            parts.push(reflector.replace(/_/g, " "))
        }

        if (parts.length === 0 &&
            callsign &&
            callsign.length > 0) {
            parts.push(callsign.replace(/_/g, " "))
        }

        if (parts.length === 0 &&
            metadata &&
            metadata.length > 0) {
            parts.push(metadata.replace(/_/g, " "))
        }

        if (location && location.length > 0) {
            parts.push(location)
        }

        return parts.join("  •  ")
    }

    onVisibleChanged: {
        if (visible && libraryModel) {
            libraryModel.refresh()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.pageColor
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 9

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Text {
                    text: qsTr("Recording Library")
                    color: "white"
                    font.pixelSize: 20
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    text: root.libraryModel
                          ? qsTr("%1 recording%2")
                                .arg(root.libraryModel.count)
                                .arg(root.libraryModel.count === 1
                                     ? ""
                                     : "s")
                          : qsTr("Loading recordings…")
                    color: root.secondaryText
                    font.pixelSize: 12
                }
            }

            Button {
                id: refreshButton

                text: qsTr("Refresh")
                Accessible.name: qsTr(
                    "Refresh recording library"
                )

                onClicked: {
                    if (root.libraryModel) {
                        root.libraryModel.refresh()
                    }
                }

                contentItem: Text {
                    text: refreshButton.text
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 12
                    font.bold: true
                }

                background: Rectangle {
                    radius: 8
                    color: refreshButton.down
                           ? "#6f4faa"
                           : root.accentColor
                    border.width: refreshButton.activeFocus ? 2 : 0
                    border.color: "white"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 34
            radius: 8
            color: "#29252f"
            border.width: 1
            border.color: "#433b4f"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    text: qsTr("Newest first")
                    color: root.secondaryText
                    font.pixelSize: 11
                }

                Item {
                    Layout.fillWidth: true
                }

                Rectangle {
                    implicitWidth: rxLegend.implicitWidth + 14
                    implicitHeight: 22
                    radius: 11
                    color: root.rxColor

                    Text {
                        id: rxLegend
                        anchors.centerIn: parent
                        text: "RX"
                        color: "white"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Rectangle {
                    implicitWidth: txLegend.implicitWidth + 14
                    implicitHeight: 22
                    radius: 11
                    color: root.txColor

                    Text {
                        id: txLegend
                        anchors.centerIn: parent
                        text: "TX"
                        color: "white"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.libraryModel &&
                     root.libraryModel.count === 0

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 30, 300)
                spacing: 8

                Text {
                    width: parent.width
                    text: "◉"
                    color: root.accentColor
                    font.pixelSize: 42
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
                    text: qsTr("No recordings yet")
                    color: "white"
                    font.pixelSize: 18
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
                    text: qsTr(
                        "Enable recording in Settings, then complete " +
                        "an RX or TX transmission."
                    )
                    color: root.secondaryText
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Check again")

                    onClicked: {
                        if (root.libraryModel) {
                            root.libraryModel.refresh()
                        }
                    }
                }
            }
        }

        ListView {
            id: recordingList

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.libraryModel &&
                     root.libraryModel.count > 0

            model: root.libraryModel
            clip: true
            spacing: 8
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 500

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                id: recordingCard

                required property int index
                required property string fileName
                required property string displayTitle
                required property string mode
                required property string direction
                required property string dateText
                required property string timeText
                required property string durationText
                required property string sizeText
                required property string talkgroup
                required property string sourceId
                required property string callsign
                required property string reflector
                required property string metadata
                required property string location

                width: recordingList.width
                height: width < 420 ? 116 : 102
                radius: 11
                color: ListView.isCurrentItem
                       ? root.selectedCardColor
                       : root.cardColor
                border.width: ListView.isCurrentItem ? 2 : 1
                border.color: ListView.isCurrentItem
                              ? root.accentColor
                              : "#48404f"

                Accessible.name: qsTr(
                    "%1 %2 recording, %3, %4"
                )
                    .arg(mode)
                    .arg(direction)
                    .arg(displayTitle)
                    .arg(durationText)

                MouseArea {
                    anchors.fill: parent
                    onClicked: recordingList.currentIndex =
                               recordingCard.index
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 46
                        radius: 23
                        color: recordingCard.direction === "TX"
                               ? root.txColor
                               : root.rxColor

                        Text {
                            anchors.centerIn: parent
                            text: recordingCard.direction.length > 0
                                  ? recordingCard.direction
                                  : "?"
                            color: "white"
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: recordingCard.mode.length > 0
                                      ? recordingCard.mode
                                      : qsTr("Unknown")
                                color: root.accentColor
                                font.pixelSize: 12
                                font.bold: true
                            }

                            Text {
                                Layout.fillWidth: true
                                text: recordingCard.displayTitle
                                color: "white"
                                font.pixelSize: 15
                                font.bold: true
                                elide: Text.ElideRight
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.detailsText(
                                recordingCard.talkgroup,
                                recordingCard.sourceId,
                                recordingCard.callsign,
                                recordingCard.reflector,
                                recordingCard.metadata,
                                recordingCard.location
                            )
                            color: root.secondaryText
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: recordingCard.dateText +
                                  "  " +
                                  recordingCard.timeText
                            color: root.mutedText
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: recordingCard.width < 420
                            text: recordingCard.fileName
                            color: root.mutedText
                            font.pixelSize: 9
                            elide: Text.ElideMiddle
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignRight |
                                          Qt.AlignVCenter
                        spacing: 3

                        Text {
                            Layout.alignment: Qt.AlignRight
                            text: recordingCard.durationText
                            color: "white"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        Text {
                            Layout.alignment: Qt.AlignRight
                            text: recordingCard.sizeText
                            color: root.secondaryText
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }
    }
}
