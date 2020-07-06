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

import QtQuick 2.5
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import Vedder.vesc.vescinterface 1.0
import Vedder.vesc.commands 1.0
import Vedder.vesc.configparams 1.0

Item {
    id: rtData
    property Commands mCommands: VescIf.commands()
    property bool isHorizontal: width > height
    property double voltageIn: 0
    property double gpsSpeed: 0
    property double currentIn: 0
    property double powerIn: 0
    property int averagePoints: 0

    Component.onCompleted: {
        mCommands.emitEmptyValues()
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: parent.width
        clip: true


        GridLayout {
            anchors.fill: parent
            rowSpacing: 20
            columnSpacing: 20
            rows: 1
            columns: isHorizontal ? 2 : 1

            Image {
                id: image
                Layout.columnSpan: isHorizontal ? 2 : 1
                Layout.preferredWidth: Math.min(rtData.width, rtData.height)
                Layout.preferredHeight: (sourceSize.height * Layout.preferredWidth) / sourceSize.width
                Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
                source: "qrc:/res/calibike_logo_mobile.png"
                visible: true
            }

            Rectangle {
                id: batteryDetailsWrapper
                Layout.preferredWidth: parent.width
                Layout.preferredHeight: mainColumnWrapper.height
                Layout.columnSpan: 1
                Layout.alignment: Qt.AlignHCenter
                color: "#303030"
                Column {
                    id: mainColumnWrapper
                    width: parent.width
                    Text {
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        id: gPSSpeedTitle
                        text: "Speed (km/h)"
                        font.family: "DejaVu Sans Mono"
                        font.pointSize: 25
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        id: gPSSpeedValue
                        text: "<span style='font-size: 60px;'>0.00</b></span>"
                        textFormat: Text.RichText
                        font.family: "DejaVu Sans Mono"
                        font.pointSize: 25
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        id: batteryVoltageTitle
                        text: "Voltage (V)"
                        font.family: "DejaVu Sans Mono"
                        font.pointSize: 25
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        id: batteryVoltageValue
                        text: "<span style='font-size: 60px;'>0.00</b></span>"
                        textFormat: Text.RichText
                        font.family: "DejaVu Sans Mono"
                        font.pointSize: 25
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        id: batteryCurrentTitle
                        text: "Current (A)"
                        font.family: "DejaVu Sans Mono"
                        font.pointSize: 25
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        id: batteryCurrentValue
                        text: "<span style='font-size: 60px;'>0.00</b></span>"
                        textFormat: Text.RichText
                        font.family: "DejaVu Sans Mono"
                        font.pointSize: 25
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        id: batteryPowerTitle
                        text: "Power (W)"
                        font.family: "DejaVu Sans Mono"
                        font.pointSize: 25
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        id: batteryPowerValue
                        text: "<span style='font-size: 60px;'>0.00</b></span>"
                        textFormat: Text.RichText
                        font.family: "DejaVu Sans Mono"
                        font.pointSize: 25
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

    }

    Connections {
        target: mCommands

        onValuesReceived: {
            VescIf.getPosition()

            if (averagePoints == 0)
            {
                /* onValuesReceived is exec one time when app is initialized */
                averagePoints++
            }
            else if (averagePoints <= 10)
            {
                currentIn += values.current_in
                voltageIn += values.v_in
                gpsSpeed += VescIf.getGPSSpeed()
                averagePoints++
            }
            else
            {
                voltageIn = voltageIn/10
                currentIn = currentIn/10
                gpsSpeed = gpsSpeed/10
                powerIn = voltageIn * currentIn
                /* Just use parseFloat when you're displaying rather than for making calculations */
                gPSSpeedValue.text = "<span style='font-size: 60px;'>" + parseFloat(gpsSpeed).toFixed(2) + "</b></span>"
                batteryVoltageValue.text = "<span style='font-size: 60px;'>" + parseFloat(voltageIn).toFixed(2) + "</b></span>"
                batteryCurrentValue.text = "<span style='font-size: 60px;'>" + parseFloat(currentIn).toFixed(2) + "</b></span>"
                batteryPowerValue.text = "<span style='font-size: 60px;'>" + parseFloat(powerIn).toFixed(2) + "</b></span>"
                voltageIn = 0
                currentIn = 0
                gpsSpeed = 0
                powerIn = 0
                averagePoints = 0
            }

        }

    }

}
