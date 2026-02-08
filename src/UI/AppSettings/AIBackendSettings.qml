import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.FactControls
import QGroundControl.Controls

SettingsPage {
    property var _settingsManager: QGroundControl.settingsManager
    property var _appSettings: _settingsManager.appSettings

    SettingsGroupLayout {
        Layout.fillWidth: true
        heading: qsTr("AI Backend")

        FactCheckBoxSlider {
            Layout.fillWidth: true
            text: qsTr("Enable AI Backend")
            fact: _appSettings.aiBackendEnabled
            visible: _appSettings.aiBackendEnabled.visible
        }

        LabelledFactTextField {
            Layout.fillWidth: true
            label: qsTr("Backend URL")
            fact: _appSettings.aiBackendUrl
            visible: _appSettings.aiBackendUrl.visible
        }

        LabelledFactComboBox {
            Layout.fillWidth: true
            label: qsTr("Mode")
            fact: _appSettings.aiBackendMode
            indexModel: false
            visible: _appSettings.aiBackendMode.visible
        }

        LabelledFactTextField {
            Layout.fillWidth: true
            label: qsTr("Model")
            fact: _appSettings.aiBackendModel
            visible: _appSettings.aiBackendModel.visible
        }

        QGCLabel {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Press Ctrl+L in Fly or Plan view to open the AI chat panel.")
            font.pointSize: ScreenTools.smallFontPointSize
        }
    }
}
