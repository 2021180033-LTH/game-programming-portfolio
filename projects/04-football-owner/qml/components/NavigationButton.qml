import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
  id: root

  property bool active: false
  property string symbol: "•"
  property string label: ""
  property string badgeText: ""

  implicitHeight: 46
  leftPadding: 14
  rightPadding: 12
  hoverEnabled: true

  background: Rectangle {
    color: root.active ? "#1e4f45" : (root.hovered ? "#172d2c" : "transparent")
    radius: 7
    border.color: root.active ? "#347866" : "transparent"
  }

  contentItem: RowLayout {
    spacing: 12

    Text {
      text: root.symbol
      color: root.active ? "#78d2ad" : "#7f8d9e"
      font.pixelSize: 16
      font.weight: Font.DemiBold
      horizontalAlignment: Text.AlignHCenter
      Layout.preferredWidth: 20
    }

    Text {
      text: root.label
      color: root.active ? "#f2f7f5" : "#aab4c1"
      font.pixelSize: 14
      font.weight: root.active ? Font.DemiBold : Font.Normal
      Layout.fillWidth: true
    }

    Rectangle {
      visible: root.badgeText.length > 0
      implicitWidth: Math.max(22, badgeLabel.implicitWidth + 10)
      implicitHeight: 22
      radius: 11
      color: "#c85e58"

      Text {
        id: badgeLabel
        anchors.centerIn: parent
        text: root.badgeText
        color: "white"
        font.pixelSize: 11
        font.weight: Font.Bold
      }
    }
  }
}

