/*
    Copyright 2017 - 2019 Benjamin Vedder	benjamin@vedder.se

    This file is part of VESC Tool.

    VESC Tool is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VESC Tool is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import Vedder.vesc.vescinterface 1.0
import Vedder.vesc.commands 1.0
import Vedder.vesc.configparams 1.0
import Vedder.vesc.utility 1.0

ApplicationWindow {
    id: appWindow
    property Commands mCommands: VescIf.commands()
    property ConfigParams mMcConf: VescIf.mcConfig()
    property ConfigParams mAppConf: VescIf.appConfig()
    property ConfigParams mInfoConf: VescIf.infoConfig()

    visible: true
    width: 640
    height: 480
    title: qsTr("Calibike tool")

    Component.onCompleted: {
        Utility.keepScreenOn(VescIf.keepScreenOn())
    }

    SwipeView {
        id: swipeView
        currentIndex: tabBar.currentIndex
        anchors.fill: parent

        Drawer {
            id: drawer
            width: 0.5 * appWindow.width
            height: appWindow.height - footer.height - tabBar.height
            y: tabBar.height
            dragMargin: 20

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 0

                Image {
                    Layout.preferredWidth: Math.min(parent.width, parent.height)
                    Layout.preferredHeight: (394 * Layout.preferredWidth) / 800
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
                    source: "qrc:/res/calibike_logo_mobile.png"
                }

                Item {
                    // Spacer
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                Button {
                    Layout.fillWidth: true
                    text: "About"
                    flat: true

                    onClicked: {
                        VescIf.emitMessageDialog(
                                    "About",
                                    Utility.aboutTextCalibike(),
                                    true, true)
                    }
                }

            }
        }

        Page {
            ConnectBle {
                id: connBle
                anchors.fill: parent
                /* offset due header menu */
                anchors.margins: 10
            }
        }

        Page {
            RowLayout {
                anchors.fill: parent
                spacing: 0

                // Gauges page
                SwipeView {
                    id: rtSwipeView
                    enabled: true
                    clip: true

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: Qt.Vertical

                    Page {
                        RtData {
                            anchors.fill: parent
                        }
                    }
                }
            }
        }

        Page {
            CalibikeUserConfig {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 10
            }
        }

        Page {
            Terminal {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 10
            }
        }

        Page {
            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 10

                GroupBox {
                    id: bleConnBox
                    title: qsTr("Realtime Data Logging")
                    Layout.fillWidth: true
                    Layout.columnSpan: 1

                    GridLayout {
                        anchors.topMargin: -5
                        anchors.bottomMargin: -5
                        anchors.fill: parent

                        clip: false
                        visible: true
                        rowSpacing: -10
                        columnSpacing: 5
                        rows: 3
                        columns: 2

                        Button {
                            text: "Help"
                            Layout.fillWidth: true

                            onClicked: {
                                VescIf.emitMessageDialog(
                                            mInfoConf.getLongName("help_rt_logging"),
                                            mInfoConf.getDescription("help_rt_logging"),
                                            true, true)
                            }
                        }

                        Button {
                            text: "Choose Log Directory..."
                            Layout.fillWidth: true

                            onClicked: {
                                if (Utility.requestFilePermission()) {
                                    logFilePicker.enabled = true
                                    logFilePicker.visible = true
                                } else {
                                    VescIf.emitMessageDialog(
                                                "File Permissions",
                                                "Unable to request file system permission.",
                                                false, false)
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.columnSpan: 2
                            Layout.topMargin: 6
                            Layout.bottomMargin: 6
                            height: rtLogFileText.implicitHeight + 14
                            border.width: 2
                            border.color: "#8d8d8d"
                            color: "#33a8a8a8"
                            radius: 3

                            TextInput {
                                color: "white"
                                id: rtLogFileText
                                anchors.fill: parent
                                anchors.margins: 7
                                font.pointSize: 12
                                text: "./log"
                            }
                        }

                        CheckBox {
                            id: rtLogEnBox
                            text: "Enable RT Data Logging"
                            Layout.fillWidth: true
                            Layout.columnSpan: 2

                            onClicked: {
                                if (rtLogEnBox.checked) {
                                    VescIf.openRtLogFile(rtLogFileText.text)
                                } else {
                                    VescIf.closeRtLogFile()
                                }
                            }

                            Timer {
                                repeat: true
                                running: true
                                interval: 500

                                onTriggered: {
                                    rtLogEnBox.checked = VescIf.isRtLogOpen()
                                }
                            }
                        }
                    }
                }

                GroupBox {
                    id: consoleLogBox
                    title: qsTr("Console Logging")
                    Layout.fillWidth: true
                    Layout.columnSpan: 1

                    GridLayout {
                        anchors.topMargin: -5
                        anchors.bottomMargin: -5
                        anchors.fill: parent

                        clip: false
                        visible: true
                        rowSpacing: -10
                        columnSpacing: 5
                        rows: 3
                        columns: 2

                        Button {
                            text: "Choose Log Directory..."
                            Layout.fillWidth: true

                            onClicked: {
                                if (Utility.requestFilePermission()) {
                                    logConsolePicker.enabled = true
                                    logConsolePicker.visible = true
                                } else {
                                    VescIf.emitMessageDialog(
                                                "File Permissions",
                                                "Unable to request file system permission.",
                                                false, false)
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.columnSpan: 2
                            Layout.topMargin: 6
                            Layout.bottomMargin: 6
                            height: consoleLogFileText.implicitHeight + 14
                            border.width: 2
                            border.color: "#8d8d8d"
                            color: "#33a8a8a8"
                            radius: 3

                            TextInput {
                                color: "white"
                                id: consoleLogFileText
                                anchors.fill: parent
                                anchors.margins: 7
                                font.pointSize: 12
                                text: "./consoleLog"
                            }
                        }

                        CheckBox {
                            id: consoleLogEnBox
                            text: "Enable Console Logging"
                            Layout.fillWidth: true
                            Layout.columnSpan: 2

                            onClicked: {
                                if (consoleLogEnBox.checked) {
                                    VescIf.openConsoleLogFile(consoleLogFileText.text)
                                } else {
                                    VescIf.closeConsoleLogFile()
                                }
                            }

                            Timer {
                                repeat: true
                                running: true
                                interval: 500

                                onTriggered: {
                                    consoleLogEnBox.checked = VescIf.isConsoleLogOpen()
                                }
                            }
                        }
                    }
                }

                Item {
                    // Spacer
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            DirectoryPicker {
                id: logFilePicker
                anchors.fill: parent
                showDotAndDotDot: true
                visible: false
                enabled: false

                onDirSelected: {
                    rtLogFileText.text = fileName
                }
            }

            DirectoryPicker {
                id: logConsolePicker
                anchors.fill: parent
                showDotAndDotDot: true
                visible: false
                enabled: false

                onDirSelected: {
                    consoleLogFileText.text = fileName
                }
            }
        }

    }

    header: Rectangle {
        color: "#5f5f5f"
        height: tabBar.height

        RowLayout {
            anchors.fill: parent
            spacing: 0

            ToolButton {
                Layout.preferredHeight: tabBar.height
                Layout.preferredWidth: tabBar.height - 10

                Image {
                    id: manuButton
                    anchors.centerIn: parent
                    width: tabBar.height * 0.5
                    height: tabBar.height * 0.5
                    opacity: 0.5
                    source: "qrc:/res/icons/Settings-96.png"
                }

                onClicked: {
                    if (drawer.visible) {
                        drawer.close()
                    } else {
                        drawer.open()
                    }
                }
            }

            TabBar {
                id: tabBar
                currentIndex: swipeView.currentIndex
                Layout.fillWidth: true
                implicitWidth: 0
                clip: true

                background: Rectangle {
                    opacity: 1
                    color: "#4f4f4f"
                }

                property int buttons: 3
                property int buttonWidth: 120

                TabButton {
                    text: qsTr("Start")
                    width: Math.max(tabBar.buttonWidth, tabBar.width / tabBar.buttons)
                }
                TabButton {
                    text: qsTr("Metrics")
                    width: Math.max(tabBar.buttonWidth, tabBar.width / tabBar.buttons)
                }
                TabButton {
                    text: qsTr("User config")
                    width: Math.max(tabBar.buttonWidth, tabBar.width / tabBar.buttons)
                }

                TabButton {
                    text: qsTr("Terminal")
                    width: Math.max(tabBar.buttonWidth, tabBar.width / tabBar.buttons)
                }
                TabButton {
                    text: qsTr("Developer")
                    width: Math.max(tabBar.buttonWidth, tabBar.width / tabBar.buttons)
                }
            }
        }
    }

    // The one that says por not connected.
    footer: Rectangle {
        id: connectedRect
        color: "#4f4f4f"

        Text {
            id: connectedText
            color: "white"
            text: VescIf.getConnectedPortName()
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            anchors.fill: parent
        }

        width: parent.width
        height: 20
    }

    Timer {
        id: statusTimer
        interval: 1600
        running: false
        repeat: false
        onTriggered: {
            // Asign to footer state
            connectedText.text = VescIf.getConnectedPortName()
            connectedRect.color = "#4f4f4f"
        }
    }

    Timer {
        id: uiTimer
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            if (!statusTimer.running && connectedText.text !== VescIf.getConnectedPortName()) {
                connectedText.text = VescIf.getConnectedPortName()
            }
        }
    }

    Timer {
        id: confTimer
        interval: 1000
        running: true
        repeat: true

        property bool mcConfRx: false
        property bool appConfRx: false

        onTriggered: {
            if (VescIf.isPortConnected()) {
                if (!mcConfRx) {
                    mCommands.getMcconf()
                }

                if (!appConfRx) {
                    mCommands.getAppConf()
                }
            }
        }
    }

    Timer {
        id: rtTimer
        interval: 50
        running: true
        repeat: true

        onTriggered: {
            if (VescIf.isPortConnected()) {
                if ((tabBar.currentIndex == 1 && rtSwipeView.currentIndex == 0) ||
                        VescIf.isRtLogOpen()) {
                    interval = 50
                    mCommands.getValues()
                }

                if (tabBar.currentIndex == 1 && rtSwipeView.currentIndex == 1) {
                    interval = 50
                    mCommands.getValuesSetup()
                }
            }
        }
    }


    Timer {
        id: movingSensingTimer
        interval: 10000
        running: true
        repeat: true

        onTriggered: {
            VescIf.getDeviceMovementState()
        }
    }

    Timer {
        id: aliveTimerMain
        interval: 200
        running: true
        repeat: true

        onTriggered: {
            if (VescIf.isPortConnected()) {
                mCommands.sendAlive()
            }
        }
    }

    Dialog {
        id: vescDialog
        standardButtons: Dialog.Ok
        modal: true
        focus: true
        width: parent.width - 20
        height: Math.min(implicitHeight, parent.height - 40)
        closePolicy: Popup.CloseOnEscape

        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        ScrollView {
            anchors.fill: parent
            clip: true
            contentWidth: parent.width - 20

            Text {
                id: vescDialogLabel
                color: "#ffffff"
                linkColor: "lightblue"
                verticalAlignment: Text.AlignVCenter
                anchors.fill: parent
                wrapMode: Text.WordWrap
                textFormat: Text.RichText
                onLinkActivated: {
                    Qt.openUrlExternally(link)
                }
            }
        }
    }

    Connections {
        target: VescIf
        onPortConnectedChanged: {
            connectedText.text = VescIf.getConnectedPortName()
            if (VescIf.isPortConnected() && (VescIf.getCalibikeBleState() === true) )  {

            }
        }

        onStatusMessage: {
            connectedText.text = msg
            connectedRect.color = isGood ? "green" : "red"
            statusTimer.restart()
        }

        onMessageDialog: {
            vescDialog.title = title
            vescDialogLabel.text = (richText ? "<style>a:link { color: lightblue; }</style>" : "") + msg
            vescDialogLabel.textFormat = richText ? Text.RichText : Text.AutoText
            vescDialog.open()
        }

        onFwRxChanged: {
            if (rx) {
                if (limited) {
                    swipeView.setCurrentIndex(5)
                } else {
                    mCommands.getMcconf()
                    mCommands.getAppConf()
                }
            }
        }
    }

    Connections {
        target: mMcConf

        onUpdated: {
            confTimer.mcConfRx = true
        }
    }

    Connections {
        target: mAppConf

        onUpdated: {
            confTimer.appConfRx = true
        }
    }
}
