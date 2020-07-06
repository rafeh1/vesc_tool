import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import Vedder.vesc.vescinterface 1.0
import Vedder.vesc.commands 1.0
import Vedder.vesc.configparams 1.0

Item {
    id: userConfig
    property Commands mCommands: VescIf.commands()
    property bool isHorizontal: width > height
    property string newPwd: ""
    property string currentPwd: ""
    property string autoLockEnable: "ul " + currentPwd + " enable"
    property string autoLockDisable: "ul " + currentPwd + " disable"
    property string setPwd: "sp " + currentPwd
    property string lockEnable: "lk"
    property string lockDisable: "ul " + currentPwd
    property string email: ""
    property double gpsSpeed: 0

    Component.onCompleted: {
        currentPwd = VescIf.getLastPwd()
        email = VescIf.getEmail()
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: parent.width
        clip: true

        GridLayout {
            id: grid
            anchors.fill: parent

            columns: isHorizontal ? 2 : 1
            columnSpacing: 20
            rowSpacing: 10

            Image {
                id: image
                Layout.columnSpan: isHorizontal ? 2 : 1
                Layout.preferredWidth: Math.min(userConfig.width, userConfig.height)
                Layout.preferredHeight: (sourceSize.height * Layout.preferredWidth) / sourceSize.width
                Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
                source: "qrc:/res/calibike_logo_mobile.png"
                visible: true
            }

            GroupBox {
                id: userConfigBox
                title: qsTr("Configurations")
                Layout.fillWidth: true
                Layout.columnSpan: isHorizontal ? 2 : 1

                GridLayout {
                    anchors.topMargin: -5
                    anchors.bottomMargin: -5
                    anchors.fill: parent

                    clip: false
                    visible: true
                    rowSpacing: -1
                    columnSpacing: 5
                    columns: 6
                    rows: 5


                    Button {
                        padding: 10
                        text: "Enable autolock"
                        Layout.fillWidth: true
                        Layout.columnSpan: 3

                        onClicked: {

                            terminalText.clear()
                            mCommands.sendTerminalCmd(autoLockEnable)
                        }
                    }

                    Button {
                        padding: 10
                        text: "Disable autolock"
                        Layout.fillWidth: true
                        Layout.columnSpan: 3

                        onClicked: {
                            terminalText.clear()
                            mCommands.sendTerminalCmd(autoLockDisable)
                        }
                    }

                    Button {
                        id: setpwdButton
                        padding: 10
                        text: "Set password"
                        Layout.fillWidth: true
                        Layout.columnSpan: 6

                        enabled: true
                        onClicked: {
                            setPwdDialog.title = "Set new password"
                            setPwdDialog.open()
                        }
                    }

                    // For debug purposes
                    Button {
                        id: setCurrentPwdButton
                        padding: 10
                        text: "Set current password"
                        Layout.fillWidth: true
                        Layout.columnSpan: 6
                        enabled: false
                        visible: false

                        onClicked: {
                            setCurrentPwdDialog.title = "Set current password"
                            setCurrentPwdDialog.open()
                        }
                    }

                    // For debug purposes
                    Button {
                        id: sendEmailButton
                        padding: 10
                        text: "Send email"
                        Layout.fillWidth: true
                        Layout.columnSpan: 6
                        enabled: false
                        visible: false

                        onClicked: {
                            VescIf.sendEmail()
                        }
                    }

                    Button {
                        id: setEmailButton
                        padding: 10
                        text: "Set email"
                        Layout.fillWidth: true
                        Layout.columnSpan: 6

                        enabled: true
                        onClicked: {
                            setEmailDialog.title = "Set an email"
                            setEmailDialog.open()
                        }
                    }


                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.columnSpan: 6
                        color: "#33a8a8a8"
                        height: emailPrint.implicitHeight + 14
                        Text {
                            id: emailPrint
                            color: "#ffffff"
                            anchors.fill: parent
                            anchors.margins: 12
                            font.pointSize: 12
                            text: email
                        }
                    }

                    Button {
                        padding: 10
                        text: "Lock"
                        Layout.fillWidth: true
                        Layout.columnSpan: 3

                        onClicked: {
                            terminalText.clear()
                            mCommands.sendTerminalCmd(lockEnable)
                        }

                    }

                    Button {
                        padding: 10
                        text: "Unlock"
                        Layout.fillWidth: true
                        Layout.columnSpan: 3

                        onClicked: {
                            terminalText.clear()
                            mCommands.sendTerminalCmd(lockDisable)
                        }

                    }
                }

            }

            GroupBox {
                id: userTerminal
                title: qsTr("Output")
                Layout.fillWidth: true
                Layout.columnSpan: isHorizontal ? 2 : 1
                Layout.preferredHeight: 120
                Layout.fillHeight: true

                ColumnLayout {
                    id: column
                    anchors.fill: parent
                    spacing: 0

                    ScrollView {
                        id: scroll
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: terminalText.width
                        height: parent.height
                        width: parent.width
                        clip: true

                        TextArea {
                            id: terminalText
                            anchors.fill: parent
                            readOnly: true
                            font.family: "DejaVu Sans Mono"
                            width: parent.width
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "Clear"
                        enabled: false
                        visible: false

                        onClicked: {
                            terminalText.clear()
                        }
                    }

                }
            }

        }
    }

    Connections {
        target: mCommands

        onPrintReceived: {
            terminalText.text += "\n" + str
        }

        onSystemLocked: {

            /* Avoid to read only one sample. Take more samples
               to actually know that user is movign over dt
            */
            for (var averagePoints = 0; averagePoints < 10; averagePoints++)
            {
                VescIf.getPosition()
                gpsSpeed += VescIf.getGPSSpeed()
            }

            if ( gpsSpeed > 0 )
            {
                gpsSpeed = 0
                terminalText.clear()
                mCommands.sendTerminalCmd(lockDisable)
            }
        }

        onVerifyPassword: {
            if (response) {
                currentPwd = newPwd
                VescIf.setLastPwd(newPwd)
                VescIf.sendEmail()
            }

        }
    }


    Dialog {
        id: setEmailDialog
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        focus: true
        title: "Set an email to send password when it's saved"

        width: parent.width-20
        height: 200
        parent: ApplicationWindow.overlay
        x: 10
        y: Math.max(parent.height / 4 - height / 2, 20)

        Rectangle {
            anchors.fill: parent
            height: stringInputEmail.implicitHeight + 14
            border.width: 2
            border.color: "#8d8d8d"
            color: "#a8a8a8"
            radius: 3
            TextInput {
                id: stringInputEmail
                color: "#ffffff"
                anchors.fill: parent
                anchors.margins: 7
                font.pointSize: 12
                focus: true
            }
        }

        onRejected: {
             stringInputEmail.clear()
        }

        onAccepted: {
            if (VescIf.validateEmail(stringInputEmail.text)) {
                VescIf.setEmail(stringInputEmail.text)
                email = stringInputEmail.text
            }
            else {
                VescIf.emitMessageDialog("Setting up email",
                "Wrong email, try again",
                false, false)
            }
            stringInputEmail.clear()
        }

    }

    Dialog {
        id: setCurrentPwdDialog
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        focus: true
        title: "Set current password. For debug purposes only."

        width: parent.width-20
        height: 200
        parent: ApplicationWindow.overlay
        x: 10
        y: Math.max(parent.height / 4 - height / 2, 20)

        Rectangle {
            anchors.fill: parent
            height: stringInputCurrentPwd.implicitHeight + 14
            border.width: 2
            border.color: "#8d8d8d"
            color: "#a8a8a8"
            radius: 3
            TextInput {
                id: stringInputCurrentPwd
                color: "#ffffff"
                anchors.fill: parent
                anchors.margins: 7
                font.pointSize: 12
                focus: true
            }
        }

        onRejected: {
            stringInputCurrentPwd.clear()
        }

        onAccepted: {

            if ( (stringInputCurrentPwd.text.length > 0) && (stringInputCurrentPwd.text.length < 8) ) {
                VescIf.emitMessageDialog("Password",
                "Password should be 8 character long",
                false, false)
            }
            else if (stringInputCurrentPwd.text.length == 8) {
                /* Executed when password is accepted from vesc */
                VescIf.setLastPwd(stringInputCurrentPwd.text)
                terminalText.clear()
                currentPwd = VescIf.getLastPwd()
            }
            /* when there are more than 8 character long */
            else {
                VescIf.emitMessageDialog("Password",
                "Password should be 8 character long",
                false, false)
            }

            stringInputCurrentPwd.clear()
        }

    }

    Dialog {
        id: setPwdDialog
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        focus: true
        title: "Set new password"

        width: parent.width-20
        height: 200
        parent: ApplicationWindow.overlay
        x: 10
        y: Math.max(parent.height / 4 - height / 2, 20)

        Rectangle {
            anchors.fill: parent
            height: stringInput.implicitHeight + 14
            border.width: 2
            border.color: "#8d8d8d"
            color: "#a8a8a8"
            radius: 3
            TextInput {
                id: stringInput
                color: "#ffffff"
                anchors.fill: parent
                anchors.margins: 7
                font.pointSize: 12
                focus: true
            }
        }

        onRejected: {
             stringInput.clear()
        }

        onAccepted: {

            if ( (stringInput.text.length > 0) && (stringInput.text.length < 8) ) {
                VescIf.emitMessageDialog("Password",
                "Password should be 8 character long",
                false, false)
            }

            else if (stringInput.text.length == 8) {
                newPwd = stringInput.text
                terminalText.clear()
                mCommands.sendTerminalCmd("sp " + newPwd)
            }
            /* when there are more than 8 character long */
            else {
                VescIf.emitMessageDialog("Password",
                "Password should be 8 character long",
                false, false)
            }

            stringInput.clear()
        }

    }
}




/*##^##
Designer {
    D{i:0;autoSize:true;height:480;width:640}
}
##^##*/
