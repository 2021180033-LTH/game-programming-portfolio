import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
  id: page

  property string selectedSaveId: ""
  property string selectedSaveName: ""
  property bool selectedAutosave: false

  function clearSelection() {
    selectedSaveId = ""
    selectedSaveName = ""
    selectedAutosave = false
  }

  Connections {
    target: game

    function onSavesChanged() {
      page.clearSelection()
    }
  }

  Rectangle {
    anchors.fill: parent
    color: "#101720"
  }

  ColumnLayout {
    anchors.fill: parent
    anchors.leftMargin: 56
    anchors.rightMargin: 56
    anchors.topMargin: 42
    anchors.bottomMargin: 42
    spacing: 22

    RowLayout {
      Layout.fillWidth: true
      spacing: 18

      Rectangle {
        Layout.preferredWidth: 58
        Layout.preferredHeight: 58
        radius: 29
        color: "#9d2430"
        border.color: "#d3b56d"
        border.width: 2

        Text {
          anchors.centerIn: parent
          text: "FO"
          color: "white"
          font.pixelSize: 17
          font.weight: Font.Bold
        }
      }

      ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Text {
          text: "Football Owner"
          color: "#f2f5f7"
          font.pixelSize: 32
          font.weight: Font.DemiBold
        }

        Text {
          text: "역사를 인수하고, 그다음 역사를 결정하십시오"
          color: "#8290a0"
          font.pixelSize: 13
        }
      }

      Text {
        text: "MVP 0.7.0"
        color: "#c7a35a"
        font.pixelSize: 12
        font.weight: Font.DemiBold
      }
    }

    RowLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      spacing: 20

      Panel {
        Layout.preferredWidth: 330
        Layout.fillHeight: true
        padding: 24

        ColumnLayout {
          anchors.fill: parent
          spacing: 12

          Text {
            text: "커리어"
            color: "#f0f4f6"
            font.pixelSize: 20
            font.weight: Font.DemiBold
          }

          Text {
            Layout.fillWidth: true
            text: "세이브는 이름으로 관리되며 개수 제한이 없습니다."
            color: "#7f8d9e"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
          }

          Item { Layout.preferredHeight: 8 }

          Button {
            id: continueButton
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            text: "이어하기"
            enabled: game.canContinue
            onClicked: game.continueCareer()

            background: Rectangle {
              radius: 7
              color: continueButton.enabled
                     ? (continueButton.hovered ? "#2d7b66" : "#256854")
                     : "#242d36"
              border.color: continueButton.enabled ? "#3f8d77" : "#303a45"
            }

            contentItem: Text {
              text: continueButton.text
              color: continueButton.enabled ? "white" : "#687482"
              font.pixelSize: 14
              font.weight: Font.DemiBold
              horizontalAlignment: Text.AlignHCenter
              verticalAlignment: Text.AlignVCenter
            }
          }

          Button {
            id: newButton
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            text: "새 게임"
            onClicked: {
              game.clearError()
              newSaveName.text = "맨유 1992-93"
              newOwnerName.text = "신임 구단주"
              newCareerDialog.open()
            }

            background: Rectangle {
              radius: 7
              color: newButton.hovered ? "#2a3948" : "#202c38"
              border.color: "#3a4a5a"
            }

            contentItem: Text {
              text: newButton.text
              color: "#dce3e8"
              font.pixelSize: 13
              horizontalAlignment: Text.AlignHCenter
              verticalAlignment: Text.AlignVCenter
            }
          }

          Button {
            id: loadButton
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            text: selectedAutosave ? "자동 저장 복원" : "선택한 세이브 불러오기"
            enabled: page.selectedSaveId.length > 0
            onClicked: game.loadSave(page.selectedSaveId)

            background: Rectangle {
              radius: 7
              color: loadButton.enabled
                     ? (loadButton.hovered ? "#2a3948" : "#202c38")
                     : "#1c242d"
              border.color: loadButton.enabled ? "#3a4a5a" : "#29323c"
            }

            contentItem: Text {
              text: loadButton.text
              color: loadButton.enabled ? "#dce3e8" : "#66717e"
              font.pixelSize: 13
              horizontalAlignment: Text.AlignHCenter
              verticalAlignment: Text.AlignVCenter
            }
          }

          RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
              id: renameButton
              Layout.fillWidth: true
              text: "이름 변경"
              enabled: page.selectedSaveId.length > 0 && !page.selectedAutosave
              onClicked: {
                renameField.text = page.selectedSaveName
                renameDialog.open()
              }
            }

            Button {
              id: deleteButton
              Layout.fillWidth: true
              text: "삭제"
              enabled: page.selectedSaveId.length > 0
              onClicked: deleteDialog.open()
            }
          }

          Item { Layout.fillHeight: true }

          Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: infoText.implicitHeight + 24
            radius: 7
            color: "#131d27"
            border.color: "#263442"

            Text {
              id: infoText
              anchors.fill: parent
              anchors.margins: 12
              text: "날짜 진행과 주요 결정 뒤에는 현재 세이브와 별도로 최근 자동 저장 3개가 순환 보관됩니다."
              color: "#8190a1"
              font.pixelSize: 11
              wrapMode: Text.WordWrap
            }
          }

          Button {
            Layout.fillWidth: true
            text: "게임 종료"
            onClicked: game.exitGame()
          }
        }
      }

      Panel {
        Layout.fillWidth: true
        Layout.fillHeight: true
        padding: 22

        ColumnLayout {
          anchors.fill: parent
          spacing: 14

          RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
              Layout.fillWidth: true
              spacing: 2

              Text {
                text: "저장된 게임"
                color: "#f0f4f6"
                font.pixelSize: 20
                font.weight: Font.DemiBold
              }

              Text {
                text: game.saveEntries.length + "개 항목 · 커리어와 복구용 자동 저장"
                color: "#758395"
                font.pixelSize: 11
              }
            }

            Rectangle {
              implicitWidth: 112
              implicitHeight: 28
              radius: 14
              color: "#20332e"
              border.color: "#315a4e"

              Text {
                anchors.centerIn: parent
                text: "FM식 이름 저장"
                color: "#79bea4"
                font.pixelSize: 11
                font.weight: Font.Medium
              }
            }
          }

          Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#2a3745"
          }

          Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
              anchors.centerIn: parent
              visible: game.saveEntries.length === 0
              text: "아직 저장된 커리어가 없습니다.\n새 게임으로 1992년 7월 1일부터 시작하십시오."
              color: "#728092"
              font.pixelSize: 13
              horizontalAlignment: Text.AlignHCenter
              lineHeight: 1.5
            }

            ListView {
              id: saveList
              anchors.fill: parent
              visible: game.saveEntries.length > 0
              clip: true
              spacing: 9
              model: game.saveEntries
              boundsBehavior: Flickable.StopAtBounds
              ScrollBar.vertical: ScrollBar { }

              delegate: Rectangle {
                id: saveCard

                required property var modelData
                property var rowData: modelData
                property bool selected: page.selectedSaveId === rowData.id

                width: ListView.view.width - 10
                height: 102
                radius: 8
                color: selected ? "#22394a" : (cardMouse.containsMouse ? "#1d2a36" : "#151f2a")
                border.color: selected ? "#4c7f9e" : "#293746"
                border.width: selected ? 2 : 1

                MouseArea {
                  id: cardMouse
                  anchors.fill: parent
                  hoverEnabled: true
                  onClicked: {
                    page.selectedSaveId = saveCard.rowData.id
                    page.selectedSaveName = saveCard.rowData.name
                    page.selectedAutosave = saveCard.rowData.autosave
                  }
                  onDoubleClicked: game.loadSave(saveCard.rowData.id)
                }

                RowLayout {
                  anchors.fill: parent
                  anchors.margins: 14
                  spacing: 14

                  Rectangle {
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 52
                    radius: 26
                    color: saveCard.rowData.autosave ? "#34414f" : "#9d2430"
                    border.color: saveCard.rowData.autosave ? "#596878" : "#d3b56d"

                    Text {
                      anchors.centerIn: parent
                      text: saveCard.rowData.autosave ? "A" : "MU"
                      color: "white"
                      font.pixelSize: 13
                      font.weight: Font.Bold
                    }
                  }

                  ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    RowLayout {
                      Layout.fillWidth: true

                      Text {
                        Layout.fillWidth: true
                        text: saveCard.rowData.name
                        color: "#eef2f4"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                      }

                      Rectangle {
                        implicitWidth: typeText.implicitWidth + 16
                        implicitHeight: 24
                        radius: 12
                        color: saveCard.rowData.autosave ? "#313a46" : "#263b34"
                        border.color: saveCard.rowData.autosave ? "#526071" : "#3d6b5d"

                        Text {
                          id: typeText
                          anchors.centerIn: parent
                          text: saveCard.rowData.typeLabel
                          color: saveCard.rowData.autosave ? "#a8b3bf" : "#80c0a8"
                          font.pixelSize: 10
                        }
                      }
                    }

                    Text {
                      text: saveCard.rowData.clubName + " · " + saveCard.rowData.season
                      color: "#9aa6b3"
                      font.pixelSize: 11
                    }

                    Text {
                      text: saveCard.rowData.gameDate
                            + "  ·  " + saveCard.rowData.playTime
                            + "  ·  " + saveCard.rowData.savedAt
                      color: "#6f7e8f"
                      font.pixelSize: 10
                    }
                  }
                }
              }
            }
          }
        }
      }
    }

    Rectangle {
      visible: game.errorMessage.length > 0 || game.statusMessage.length > 0
      Layout.fillWidth: true
      Layout.preferredHeight: 38
      radius: 7
      color: game.errorMessage.length > 0 ? "#5b2b2c" : "#172a26"
      border.color: game.errorMessage.length > 0 ? "#8b4547" : "#285244"

      Text {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        text: game.errorMessage.length > 0
              ? "처리 오류 · " + game.errorMessage
              : game.statusMessage
        color: game.errorMessage.length > 0 ? "#ffd9d6" : "#8ec7b1"
        font.pixelSize: 11
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
      }
    }
  }

  Dialog {
    id: newCareerDialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 480
    title: "새 게임"
    closePolicy: Popup.CloseOnEscape

    contentItem: ColumnLayout {
      spacing: 12

      Text {
        Layout.fillWidth: true
        text: "1992년 7월 1일 Manchester United 인수 시점에서 새 역사가 시작됩니다."
        color: "#7f8d9e"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
      }

      Label { text: "세이브 이름" }

      TextField {
        id: newSaveName
        Layout.fillWidth: true
        maximumLength: 80
        selectByMouse: true
      }

      Text {
        visible: game.saveNameExists(newSaveName.text)
        text: "같은 이름의 세이브가 이미 있습니다."
        color: "#d46b65"
        font.pixelSize: 11
      }

      Label { text: "구단주 이름" }

      TextField {
        id: newOwnerName
        Layout.fillWidth: true
        maximumLength: 60
        selectByMouse: true
      }

      RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 8

        Item { Layout.fillWidth: true }

        Button {
          text: "취소"
          onClicked: newCareerDialog.close()
        }

        Button {
          text: "커리어 시작"
          enabled: newSaveName.text.trim().length > 0
                   && !game.saveNameExists(newSaveName.text)
          onClicked: {
            if (game.createNewCareer(newSaveName.text, newOwnerName.text))
              newCareerDialog.close()
          }
        }
      }
    }
  }

  Dialog {
    id: renameDialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 440
    title: "세이브 이름 변경"

    contentItem: ColumnLayout {
      spacing: 12

      TextField {
        id: renameField
        Layout.fillWidth: true
        maximumLength: 80
        selectByMouse: true
      }

      Text {
        visible: game.saveNameExists(renameField.text)
        text: "같은 이름의 세이브가 이미 있습니다."
        color: "#d46b65"
        font.pixelSize: 11
      }

      RowLayout {
        Layout.fillWidth: true
        Item { Layout.fillWidth: true }
        Button {
          text: "취소"
          onClicked: renameDialog.close()
        }
        Button {
          text: "변경"
          enabled: renameField.text.trim().length > 0
                   && !game.saveNameExists(renameField.text)
          onClicked: {
            if (game.renameSave(page.selectedSaveId, renameField.text))
              renameDialog.close()
          }
        }
      }
    }
  }

  Dialog {
    id: deleteDialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 440
    title: "저장 데이터 삭제"

    contentItem: ColumnLayout {
      spacing: 14

      Text {
        Layout.fillWidth: true
        text: page.selectedAutosave
              ? "선택한 자동 저장 복제본을 삭제할까요?"
              : "‘" + page.selectedSaveName + "’ 커리어와 연결된 자동 저장을 모두 삭제할까요?"
        color: "#c8d0d8"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
      }

      Text {
        text: "삭제한 저장 데이터는 게임 안에서 복구할 수 없습니다."
        color: "#d46b65"
        font.pixelSize: 11
      }

      RowLayout {
        Layout.fillWidth: true
        Item { Layout.fillWidth: true }
        Button {
          text: "취소"
          onClicked: deleteDialog.close()
        }
        Button {
          text: "삭제"
          onClicked: {
            if (game.deleteSave(page.selectedSaveId))
              deleteDialog.close()
          }
        }
      }
    }
  }
}
