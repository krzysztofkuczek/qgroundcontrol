/*=====================================================================

 QGroundControl Open Source Ground Control Station

 (c) 2009 - 2015 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>

 This file is part of the QGROUNDCONTROL project

 QGROUNDCONTROL is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 QGROUNDCONTROL is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with QGROUNDCONTROL. If not, see <http://www.gnu.org/licenses/>.

 ======================================================================*/

/// @file
///     @brief Battery, propeller and magnetometer settings
///     @author Gus Grubba <mavlink@grubba.com>

import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Dialogs 1.2

import QGroundControl.FactSystem 1.0
import QGroundControl.FactControls 1.0
import QGroundControl.Palette 1.0
import QGroundControl.Controls 1.0
import QGroundControl.ScreenTools 1.0
import QGroundControl.Controllers 1.0

QGCView {
    id:         rootQGCView
    viewPanel:  panel

    property int textEditWidth:    ScreenTools.defaultFontPixelWidth * 8

    property Fact battNumCells:     controller.getParameterFact(-1, "BAT_N_CELLS")
    property Fact battHighVolt:     controller.getParameterFact(-1, "BAT_V_CHARGED")
    property Fact battLowVolt:      controller.getParameterFact(-1, "BAT_V_EMPTY")
    property Fact battVoltLoadDrop: controller.getParameterFact(-1, "BAT_V_LOAD_DROP")

    function getBatteryImage()
    {
        switch(battNumCells.value) {
            case 1:  return "/qmlimages/PowerComponentBattery_01cell.svg";
            case 2:  return "/qmlimages/PowerComponentBattery_02cell.svg"
            case 3:  return "/qmlimages/PowerComponentBattery_03cell.svg"
            case 4:  return "/qmlimages/PowerComponentBattery_04cell.svg"
            case 5:  return "/qmlimages/PowerComponentBattery_05cell.svg"
            case 6:  return "/qmlimages/PowerComponentBattery_06cell.svg"
            default: return "/qmlimages/PowerComponentBattery_01cell.svg";
        }
    }

    function drawArrowhead(ctx, x, y, radians)
    {
        ctx.save();
        ctx.beginPath();
        ctx.translate(x,y);
        ctx.rotate(radians);
        ctx.moveTo(0,0);
        ctx.lineTo(5,10);
        ctx.lineTo(-5,10);
        ctx.closePath();
        ctx.restore();
        ctx.fill();
    }

    function drawLineWithArrow(ctx, x1, y1, x2, y2)
    {
        ctx.beginPath();
        ctx.moveTo(x1, y1);
        ctx.lineTo(x2, y2);
        ctx.stroke();
        var rd = Math.atan((y2 - y1) / (x2 - x1));
        rd += ((x2 > x1) ? 90 : -90) * Math.PI/180;
        drawArrowhead(ctx, x2, y2, rd);
    }

    PowerComponentController {
        id:         controller
        factPanel:  panel

        onOldFirmware:          showMessage("ESC Calibration", "QGroundControl cannot perform ESC Calibration with this version of firmware. You will need to upgrade to a newer firmware.", StandardButton.Ok)
        onNewerFirmware:        showMessage("ESC Calibration", "QGroundControl cannot perform ESC Calibration with this version of firmware. You will need to upgrade QGroundControl.", StandardButton.Ok)
        onBatteryConnected:     showMessage("ESC Calibration", "Performing calibration. This will take a few seconds..", 0)
        onCalibrationFailed:    showMessage("ESC Calibration failed", errorMessage, StandardButton.Ok)
        onCalibrationSuccess:   showMessage("ESC Calibration", "Calibration complete. You can disconnect your battery now if you like.", StandardButton.Ok)
        onConnectBattery:       showMessage("ESC Calibration", "<font color=\"yellow\">WARNING: Props must be removed from vehicle prior to performing ESC calibration.</font> Connect the battery now and calibration will begin.", 0)
        onDisconnectBattery:    showMessage("ESC Calibration failed", "You must disconnect the battery prior to performing ESC Calibration. Disconnect your battery and try again.", StandardButton.Ok)
    }

    QGCPalette { id: palette; colorGroupEnabled: panel.enabled }

    QGCViewPanel {
        id:             panel
        anchors.fill:   parent

        Flickable {
            anchors.fill:       parent
            flickableDirection: Flickable.VerticalFlick
            contentHeight:      innerColumn.height

            Column {
                id:             innerColumn
                anchors.left:   parent.left
                anchors.right:  parent.right
                spacing:        ScreenTools.defaultFontPixelHeight

                QGCLabel {
                    text: "POWER CONFIG"
                    font.pixelSize: ScreenTools.largeFontPixelSize
                }

                QGCLabel {
                    text: "Battery"
                    font.pixelSize: ScreenTools.mediumFontPixelSize
                }

                Rectangle {
                    width:  parent.width
                    height: voltageCol.height + ScreenTools.defaultFontPixelHeight
                    color:  palette.windowShade

                    Column {
                        id:                 voltageCol
                        anchors.margins:    ScreenTools.defaultFontPixelHeight / 2
                        anchors.left:       parent.left
                        anchors.top:        parent.top
                        spacing:            ScreenTools.defaultFontPixelHeight / 2

                        property real firstColumnWidth: Math.max(Math.max(cellsLabel.contentWidth, battHighLabel.contentWidth), battLowLabel.contentWidth) + ScreenTools.defaultFontPixelWidth

                        Row {
                            spacing: ScreenTools.defaultFontPixelWidth

                            QGCLabel {
                                id:                 cellsLabel
                                text:               "Number of Cells (in Series)"
                                anchors.baseline:   cellsField.baseline
                            }

                            FactTextField {
                                id:         cellsField
                                x:          voltageCol.firstColumnWidth
                                width:      textEditWidth
                                fact:       battNumCells
                                showUnits: true
                            }
                        }

                        Row {
                            spacing: ScreenTools.defaultFontPixelWidth

                            QGCLabel {
                                id:                 battHighLabel
                                text:               "Full Voltage (per cell)"
                                anchors.baseline:   battHighField.baseline
                                }

                            FactTextField {
                                id:         battHighField
                                x:          voltageCol.firstColumnWidth
                                width:      textEditWidth
                                fact:       battHighVolt
                                showUnits:  true
                            }
                        }

                        Row {
                            spacing: ScreenTools.defaultFontPixelWidth

                            QGCLabel {
                                id:                 battLowLabel
                                text:               "Empty Voltage (per cell)"
                                anchors.baseline:   battLowField.baseline
                            }

                            FactTextField {
                                id:         battLowField
                                x:          voltageCol.firstColumnWidth
                                width:      textEditWidth
                                fact:       battLowVolt
                                showUnits:  true
                            }
                        }
                    } // Column

                    QGCColoredImage {
                        id:                     batteryImage
                        anchors.verticalCenter: voltageCol.verticalCenter
                        x:                      voltageCol.firstColumnWidth + textEditWidth + (ScreenTools.defaultFontPixelWidth * 3)
                        width:                  height * 0.75
                        height:                 voltageCol.height
                        fillMode:               Image.PreserveAspectFit
                        smooth:                 true
                        color:                  palette.button
                        cache:                  false
                        source:                 getBatteryImage();
                    }

                    Column {
                        anchors.leftMargin:     ScreenTools.defaultFontPixelWidth * 2
                        anchors.left:           batteryImage.right
                        anchors.verticalCenter: voltageCol.verticalCenter
                        spacing:                ScreenTools.defaultFontPixelHeight

                        QGCLabel {
                            text: "Battery Max: " + (battNumCells.value * battHighVolt.value).toFixed(1) + ' V'
                        }

                        QGCLabel {
                            text: "Battery Min: " + (battNumCells.value * battLowVolt.value).toFixed(1) + ' V'
                        }
                    }
                } // Rectangle - Battery settings

                QGCLabel {
                    text:           "ESC Calibration"
                    font.pixelSize: ScreenTools.mediumFontPixelSize
                }

                Rectangle {
                    width:  parent.width
                    height: escCalColumn.height + ScreenTools.defaultFontPixelHeight
                    color:  palette.windowShade

                    Column {
                        id :                escCalColumn
                        anchors.margins:    ScreenTools.defaultFontPixelHeight / 2
                        anchors.left:       parent.left
                        anchors.top:        parent.top
                        spacing:            ScreenTools.defaultFontPixelWidth

                        QGCLabel {
                            color:  palette.warningText
                            text:   "<font color=\"yellow\">WARNING: Propellers must be removed from vehicle prior to performing ESC calibration.</font>"
                        }

                        QGCLabel {
                            text: "You must use USB connection for this operation."
                        }

                        QGCButton {
                            text:       "Calibrate"
                            onClicked:  controller.calibrateEsc()
                        }
                    }
                }

                QGCLabel {
                    text:           "UAVCAN ESC Configuration"
                    font.pixelSize: ScreenTools.mediumFontPixelSize
                }

                Rectangle {
                    width:  parent.width
                    height: uavCanEscCalColumn.height + ScreenTools.defaultFontPixelHeight
                    color:  palette.windowShade

                    Column {
                        id:                 uavCanEscCalColumn
                        anchors.margins:    ScreenTools.defaultFontPixelHeight / 2
                        anchors.left:       parent.left
                        anchors.top:        parent.top
                        spacing:            ScreenTools.defaultFontPixelWidth

                        QGCLabel {
                            color:  palette.warningText
                            text:   "<font color=\"yellow\">WARNING: Propellers must be removed from vehicle prior to performing UAVCAN ESC configuration.</font>"
                        }

                        QGCLabel {
                            text: "You must use USB connection for this operation."
                        }

                        QGCButton {
                            text:       "Start Configuration"
                            onClicked:  controller.busConfigureActuators()
                        }

                        QGCButton {
                            text:       "End Configuration"
                            onClicked:  controller.StopBusConfigureActuators()
                        }
                    }
                }

                QGCCheckBox {
                    id:     showAdvanced
                    text:   "Show Advanced Settings"
                }

                QGCLabel {
                    text:           "Advanced Power Settings"
                    font.pixelSize: ScreenTools.mediumFontPixelSize
                    visible:        showAdvanced.checked
                }

                Rectangle {
                    id:         batteryRectangle
                    width:      parent.width
                    height:     advBatteryColumn.height + ScreenTools.defaultFontPixelHeight
                    color:      palette.windowShade
                    visible:    showAdvanced.checked

                    Column {
                        id: advBatteryColumn
                        anchors.margins:    ScreenTools.defaultFontPixelHeight / 2
                        anchors.left:       parent.left
                        anchors.right:      parent.right
                        anchors.top:        parent.top
                        spacing:            ScreenTools.defaultFontPixelWidth

                        Row {
                            spacing: ScreenTools.defaultFontPixelWidth

                            QGCLabel {
                                text:               "Voltage Drop on Full Load (per cell)"
                                anchors.baseline:   battDropField.baseline
                            }

                            FactTextField {
                                id:         battDropField
                                width:      textEditWidth
                                fact:       battVoltLoadDrop
                                showUnits:  true
                            }
                        }

                        QGCLabel {
                            width:      parent.width
                            wrapMode:   Text.WordWrap
                            text:       "Batteries show less voltage at high throttle. Enter the difference in Volts between idle throttle and full " +
                                            "throttle, divided by the number of battery cells. Leave at the default if unsure. " +
                                            "<font color=\"yellow\">If this value is set too high, the battery might be deep discharged and damaged.</font>"
                        }

                        Row {
                            spacing: ScreenTools.defaultFontPixelWidth

                            QGCLabel {
                                text: "Compensated Minimum Voltage:"
                            }

                            QGCLabel {
                                text: ((battNumCells.value * battLowVolt.value) - (battNumCells.value * battVoltLoadDrop.value)).toFixed(1) + ' V'
                            }
                        }
                    }
                } // Rectangle - Advanced power settings
            } // Column
        } // Flickable
    } // QGCViewPanel
} // QGCView
