/*
    DroidStar recording-library browser and player.
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Item {
    id: root

    property var libraryModel
    property int radioConnectionState: 0
    property bool pageActive: false
    property int selectedIndex: -1
    property var selectedEntry: ({})
    property string selectedPlaybackSource: ""
    property string playerError: ""

    readonly property bool radioBusy:
        radioConnectionState !== 0

    readonly property bool hasSelection:
        selectedIndex >= 0 &&
        selectedEntry &&
        selectedPlaybackSource.length > 0

    readonly property color pageColor: "#211f25"
    readonly property color cardColor: "#302c36"
    readonly property color selectedCardColor: "#3c3549"
    readonly property color accentColor: "#8b67d1"
    readonly property color rxColor: "#4682b4"
    readonly property color txColor: "#8b5fc7"
    readonly property color secondaryText: "#c9c3d2"
    readonly property color mutedText: "#9e96aa"
    readonly property color warningColor: "#d6a54b"

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

    function formatPlayerTime(milliseconds) {
        var value = Math.max(0, Number(milliseconds) || 0)
        var totalSeconds = Math.floor(value / 1000)
        var hours = Math.floor(totalSeconds / 3600)
        var minutes = Math.floor(
            (totalSeconds % 3600) / 60
        )
        var seconds = totalSeconds % 60

        function twoDigits(number) {
            return number < 10 ? "0" + number : "" + number
        }

        if (hours > 0) {
            return hours + ":" +
                   twoDigits(minutes) + ":" +
                   twoDigits(seconds)
        }

        return minutes + ":" + twoDigits(seconds)
    }

    function selectedTotalDuration() {
        if (libraryPlayer.duration > 0) {
            return libraryPlayer.duration
        }

        if (selectedEntry &&
            selectedEntry.durationMs) {
            return selectedEntry.durationMs
        }

        return 0
    }

    function selectRecording(index) {
        if (!libraryModel ||
            index < 0 ||
            index >= libraryModel.count) {
            return
        }

        var entry = libraryModel.get(index)

        if (!entry) {
            playerError = qsTr(
                "This recording could not be loaded."
            )
            return
        }

        var preparedSource =
            libraryModel.playbackSource(index)

        if (!preparedSource ||
            preparedSource.length === 0) {
            playerError = qsTr(
                "Could not prepare this recording for playback."
            )
            return
        }

        var changed =
            selectedIndex !== index ||
            selectedPlaybackSource !== preparedSource

        if (changed) {
            stopPlayback()
            libraryPlayer.source = ""
            selectedEntry = entry
            selectedIndex = index
            selectedPlaybackSource = preparedSource
            playerError = ""
            libraryPlayer.source = preparedSource
        }
        else {
            selectedIndex = index
        }

        recordingList.currentIndex = index
    }

    function togglePlayback() {
        if (!hasSelection) {
            return
        }

        if (radioBusy) {
            playerError = qsTr(
                "Disconnect the live radio before playing recordings."
            )
            return
        }

        playerError = ""

        if (libraryPlayer.playbackState ===
                MediaPlayer.PlayingState) {
            libraryPlayer.pause()
        }
        else {
            libraryPlayer.play()
        }
    }

    function stopPlayback() {
        libraryPlayer.stop()

        if (libraryPlayer.seekable) {
            libraryPlayer.position = 0
        }
    }

    function clearSelection() {
        stopPlayback()
        libraryPlayer.source = ""
        selectedEntry = ({})
        selectedIndex = -1
        selectedPlaybackSource = ""
        playerError = ""
        recordingList.currentIndex = -1
    }

    onRadioBusyChanged: {
        if (radioBusy) {
            stopPlayback()
        }
    }

    onPageActiveChanged: {
        if (pageActive) {
            if (libraryModel) {
                libraryModel.refresh()
            }
        }
        else {
            stopPlayback()
        }
    }

    onVisibleChanged: {
        if (!visible) {
            stopPlayback()
        }
    }

    MediaPlayer {
        id: libraryPlayer

        audioOutput: AudioOutput {
            id: libraryAudioOutput
            volume: 1.0
        }

        onErrorOccurred: function(error, errorString) {
            root.playerError =
                errorString && errorString.length > 0
                ? errorString
                : qsTr("The recording could not be played.")
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

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: visible ? 38 : 0
            visible: root.radioBusy
            radius: 8
            color: "#3a3022"
            border.width: 1
            border.color: root.warningColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    text: "!"
                    color: root.warningColor
                    font.bold: true
                    font.pixelSize: 16
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr(
                        "Live radio has priority. Disconnect to play recordings."
                    )
                    color: "white"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }

        Rectangle {
            id: playerPanel

            Layout.fillWidth: true
            implicitHeight: visible
                            ? (root.width < 420 ? 154 : 136)
                            : 0
            visible: root.hasSelection
            radius: 11
            color: "#29252f"
            border.width: 2
            border.color: root.accentColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 9
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: 18
                        color: root.selectedEntry.direction === "TX"
                               ? root.txColor
                               : root.rxColor

                        Text {
                            anchors.centerIn: parent
                            text: root.selectedEntry.direction || "?"
                            color: "white"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Now Playing")
                            color: root.accentColor
                            font.pixelSize: 10
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: (root.selectedEntry.mode || "") +
                                  "  " +
                                  (root.selectedEntry.displayTitle || "")
                            color: "white"
                            font.pixelSize: 14
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: (root.selectedEntry.dateText || "") +
                                  "  " +
                                  (root.selectedEntry.timeText || "")
                            color: root.secondaryText
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }

                    Button {
                        id: clearButton

                        text: qsTr("Clear")
                        onClicked: root.clearSelection()

                        contentItem: Text {
                            text: clearButton.text
                            color: root.secondaryText
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 10
                        }

                        background: Rectangle {
                            radius: 7
                            color: clearButton.down
                                   ? "#45404c"
                                   : "#38333e"
                            border.width: 1
                            border.color: "#51495a"
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    Text {
                        text: root.formatPlayerTime(
                            libraryPlayer.position
                        )
                        color: root.secondaryText
                        font.pixelSize: 10
                    }

                    Slider {
                        id: positionSlider

                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(
                            1,
                            root.selectedTotalDuration()
                        )
                        enabled: root.hasSelection &&
                                 !root.radioBusy &&
                                 libraryPlayer.seekable

                        onMoved: {
                            if (libraryPlayer.seekable) {
                                libraryPlayer.position = value
                            }
                        }

                        Binding {
                            target: positionSlider
                            property: "value"
                            value: libraryPlayer.position
                            when: !positionSlider.pressed
                        }
                    }

                    Text {
                        text: root.formatPlayerTime(
                            root.selectedTotalDuration()
                        )
                        color: root.secondaryText
                        font.pixelSize: 10
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    Button {
                        id: playPauseButton

                        Layout.preferredWidth: 84
                        text: libraryPlayer.playbackState ===
                              MediaPlayer.PlayingState
                              ? qsTr("Pause")
                              : qsTr("Play")
                        enabled: root.hasSelection &&
                                 !root.radioBusy
                        onClicked: root.togglePlayback()

                        contentItem: Text {
                            text: playPauseButton.text
                            color: playPauseButton.enabled
                                   ? "white"
                                   : root.mutedText
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 12
                            font.bold: true
                        }

                        background: Rectangle {
                            radius: 8
                            color: playPauseButton.down
                                   ? "#6f4faa"
                                   : root.accentColor
                            opacity: playPauseButton.enabled ? 1.0 : 0.45
                        }
                    }

                    Button {
                        id: stopButton

                        Layout.preferredWidth: 70
                        text: qsTr("Stop")
                        enabled: root.hasSelection
                        onClicked: root.stopPlayback()

                        contentItem: Text {
                            text: stopButton.text
                            color: stopButton.enabled
                                   ? "white"
                                   : root.mutedText
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 12
                        }

                        background: Rectangle {
                            radius: 8
                            color: stopButton.down
                                   ? "#45404c"
                                   : "#38333e"
                            border.width: 1
                            border.color: "#51495a"
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Text {
                        text: libraryPlayer.playbackState ===
                              MediaPlayer.PlayingState
                              ? qsTr("Playing")
                              : libraryPlayer.playbackState ===
                                MediaPlayer.PausedState
                                ? qsTr("Paused")
                                : qsTr("Ready")
                        color: root.secondaryText
                        font.pixelSize: 10
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.playerError.length > 0
                    text: root.playerError
                    color: "#ef9a9a"
                    font.pixelSize: 10
                    elide: Text.ElideRight
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
            currentIndex: -1

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
                required property int durationMs
                required property string durationText
                required property string sizeText
                required property string talkgroup
                required property string sourceId
                required property string callsign
                required property string reflector
                required property string metadata
                required property string location
                required property string playbackUrl

                width: recordingList.width
                height: width < 420 ? 116 : 102
                radius: 11
                color: root.selectedIndex === index
                       ? root.selectedCardColor
                       : root.cardColor
                border.width: root.selectedIndex === index ? 2 : 1
                border.color: root.selectedIndex === index
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

                    onClicked: {
                        root.selectRecording(
                            recordingCard.index
                        )
                    }
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

                        Text {
                            Layout.alignment: Qt.AlignRight
                            visible: root.selectedIndex ===
                                     recordingCard.index
                            text: qsTr("Selected")
                            color: root.accentColor
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }
            }
        }
    }
}
