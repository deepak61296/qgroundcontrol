/****************************************************************************
 *
 * (c) 2024 QGroundControl Project. All rights reserved.
 *
 * AI Chat Panel - Right-side overlay for AI chat assistant
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

Rectangle {
    id: root
    width: parent.width * 0.35
    color: qgcPal.window
    border.color: qgcPal.groupBorder
    border.width: 1

    property bool isOpen: false

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    AIChatController { id: chatController }

    Component.onCompleted: {
        chatController.checkConnection()
        chatController.fetchModels()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: ScreenTools.defaultFontPixelWidth
        spacing: ScreenTools.defaultFontPixelHeight / 2

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                text: qsTr("AI Assistant")
                font.pointSize: ScreenTools.largeFontPointSize
                font.bold: true
                Layout.fillWidth: true
            }

            // Connection status indicator
            Rectangle {
                width: ScreenTools.defaultFontPixelHeight
                height: width
                radius: width / 2
                color: chatController.isConnected ? "#00C853" : "#FF6B6B"

                QGCMouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: chatController.checkConnection()
                    
                    ToolTip.visible: containsMouse
                    ToolTip.text: chatController.connectionStatus
                }
            }

            // Close button
            QGCButton {
                text: "X"
                implicitWidth: ScreenTools.defaultFontPixelHeight * 2
                implicitHeight: implicitWidth
                onClicked: root.isOpen = false
            }
        }

        // Mode and Model selectors
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                text: qsTr("Mode:")
            }

            QGCComboBox {
                id: modeComboBox
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 12
                model: ["Ask", "Agent"]
                currentIndex: QGroundControl.settingsManager.appSettings.aiBackendMode.rawValue

                onActivated: (index) => {
                    QGroundControl.settingsManager.appSettings.aiBackendMode.rawValue = index
                    if (index === 1) {
                        // Agent mode warning
                        chatController.clearHistory()
                    }
                }
            }

            QGCLabel {
                text: qsTr("Model:")
            }

            QGCComboBox {
                id: modelComboBox
                Layout.fillWidth: true
                model: chatController.availableModels
                currentIndex: {
                    var currentModel = QGroundControl.settingsManager.appSettings.aiBackendModel.rawValue
                    var idx = chatController.availableModels.indexOf(currentModel)
                    return idx >= 0 ? idx : 0
                }

                onActivated: (index) => {
                    if (index >= 0 && index < chatController.availableModels.length) {
                        QGroundControl.settingsManager.appSettings.aiBackendModel.rawValue = chatController.availableModels[index]
                    }
                }
            }
        }

        // Chat history
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: qgcPal.windowShade
            border.color: qgcPal.groupBorder
            radius: ScreenTools.defaultFontPixelHeight / 4

            QGCFlickable {
                id: chatFlickable
                anchors.fill: parent
                anchors.margins: ScreenTools.defaultFontPixelWidth / 2
                contentWidth: width
                contentHeight: chatText.height
                clip: true

                function scrollToBottom() {
                    if (contentHeight > height) {
                        contentY = contentHeight - height
                    }
                }

                TextArea {
                    id: chatText
                    width: parent.width
                    readOnly: true
                    textFormat: TextEdit.RichText
                    wrapMode: Text.WordWrap
                    text: chatController.chatHistory
                    color: qgcPal.text
                    font.pointSize: ScreenTools.defaultFontPointSize
                    background: null

                    onTextChanged: chatFlickable.scrollToBottom()
                }
            }
        }

        // Input area
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth / 2

            QGCTextField {
                id: inputField
                Layout.fillWidth: true
                placeholderText: chatController.isProcessing ? qsTr("Processing...") : qsTr("Type a message...")
                enabled: !chatController.isProcessing

                Keys.onReturnPressed: sendMessage()
                Keys.onEnterPressed: sendMessage()

                function sendMessage() {
                    if (text.trim().length > 0 && !chatController.isProcessing) {
                        chatController.sendMessage(text)
                        text = ""
                    }
                }
            }

            QGCButton {
                id: sendButton
                text: chatController.isProcessing ? qsTr("Cancel") : qsTr("Send")
                enabled: chatController.isProcessing || inputField.text.trim().length > 0

                onClicked: {
                    if (chatController.isProcessing) {
                        chatController.cancelRequest()
                    } else {
                        inputField.sendMessage()
                    }
                }
            }
        }

        // Status bar
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCLabel {
                text: modeComboBox.currentIndex === 1 ?
                    qsTr("Agent Mode - Commands will be executed") :
                    qsTr("Ask Mode - Read-only queries")
                font.pointSize: ScreenTools.smallFontPointSize
                color: modeComboBox.currentIndex === 1 ? "#FFA500" : "#00C853"
                Layout.fillWidth: true
            }

            QGCButton {
                text: qsTr("Clear")
                implicitHeight: ScreenTools.defaultFontPixelHeight * 1.5
                onClicked: chatController.clearHistory()
            }
        }
    }

    // Animation for opening/closing
    states: [
        State {
            name: "open"
            when: root.isOpen
            PropertyChanges { target: root; x: parent.width - root.width }
        },
        State {
            name: "closed"
            when: !root.isOpen
            PropertyChanges { target: root; x: parent.width }
        }
    ]

    transitions: [
        Transition {
            from: "closed"
            to: "open"
            NumberAnimation { property: "x"; duration: 200; easing.type: Easing.OutQuad }
        },
        Transition {
            from: "open"
            to: "closed"
            NumberAnimation { property: "x"; duration: 200; easing.type: Easing.InQuad }
        }
    ]
}
