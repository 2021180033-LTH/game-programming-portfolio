import QtQuick
import QtQuick.Layouts

Rectangle {
  id: root

  property string label: ""
  property string value: ""
  property string hint: ""
  property color accent: "#4db58b"

  implicitWidth: 210
  implicitHeight: 118
  color: "#18212d"
  radius: 10
  border.color: "#263444"
  border.width: 1

  Rectangle {
    width: 4
    radius: 2
    color: root.accent
    anchors.left: parent.left
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.topMargin: 12
    anchors.bottomMargin: 12
  }

  ColumnLayout {
    anchors.fill: parent
    anchors.leftMargin: 18
    anchors.rightMargin: 14
    anchors.topMargin: 14
    anchors.bottomMargin: 14
    spacing: 4

    Text {
      text: root.label
      color: "#94a1b2"
      font.pixelSize: 12
      font.weight: Font.Medium
    }

    Text {
      text: root.value
      color: "#f2f5f7"
      font.pixelSize: 24
      font.weight: Font.DemiBold
      elide: Text.ElideRight
      Layout.fillWidth: true
    }

    Text {
      text: root.hint
      color: "#748296"
      font.pixelSize: 11
      elide: Text.ElideRight
      Layout.fillWidth: true
    }
  }
}

