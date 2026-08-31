import QtQuick

Rectangle {
  id: root

  default property alias contentData: contentItem.data
  property int padding: 18

  color: "#18212d"
  radius: 10
  border.color: "#263444"
  border.width: 1

  Item {
    id: contentItem
    anchors.fill: parent
    anchors.margins: root.padding
  }
}

