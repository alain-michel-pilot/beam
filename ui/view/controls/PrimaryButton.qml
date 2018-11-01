import QtQuick 2.11
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import "."

CustomButton {
    palette.button: Style.bright_teal
    palette.buttonText: Style.marine

    SFText {
        id: text
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter

        font.pixelSize: 12
        font.styleName: "Bold"; font.weight: Font.Bold

        color: Style.white

        text: parent.text
        visible: false
    }
}
