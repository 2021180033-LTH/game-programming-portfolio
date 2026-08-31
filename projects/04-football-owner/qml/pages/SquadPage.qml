import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
  id: page

  property string selectedGroup: "First Team"
  property string selectedGroupLabel: selectedGroup === "First Team"
                                      ? "1군"
                                      : (selectedGroup === "Reserve" ? "리저브" : "유스")
  property int selectedGroupCount: selectedGroup === "First Team"
                                   ? game.firstTeamCount
                                   : (selectedGroup === "Reserve" ? game.reserveCount : game.youthCount)
  property var filteredSquad: game.squad.filter(function(player) {
    return player.squadGroup === page.selectedGroup
  })

  ColumnLayout {
    anchors.fill: parent
    anchors.margins: 24
    spacing: 16

    RowLayout {
      Layout.fillWidth: true

      ColumnLayout {
        Layout.fillWidth: true
        spacing: 3

        Text {
          text: "선수단"
          color: "#f1f4f6"
          font.pixelSize: 26
          font.weight: Font.DemiBold
        }

        Text {
          text: "1992년 당시 조직에 맞춘 1군·리저브·유스 구분 · 전체 "
                + game.squad.length + "명"
          color: "#7f8d9e"
          font.pixelSize: 13
        }
      }

      Rectangle {
        implicitWidth: 190
        implicitHeight: 32
        radius: 6
        color: "#242b25"
        border.color: "#5f6442"

        Text {
          anchors.centerIn: parent
          text: "확인값과 잠정값을 함께 표시"
          color: "#d4d3ac"
          font.pixelSize: 10
        }
      }
    }

    RowLayout {
      Layout.fillWidth: true
      spacing: 8

      Repeater {
        model: [
          { key: "First Team", label: "1군", count: game.firstTeamCount },
          { key: "Reserve", label: "리저브", count: game.reserveCount },
          { key: "Youth", label: "유스", count: game.youthCount }
        ]

        Rectangle {
          required property var modelData
          property bool active: page.selectedGroup === modelData.key

          Layout.preferredWidth: 132
          Layout.preferredHeight: 38
          radius: 7
          color: active ? "#263d45" : (tabMouse.containsMouse ? "#1e2b36" : "#17212b")
          border.color: active ? "#4c8e88" : "#293642"

          Text {
            anchors.centerIn: parent
            text: modelData.label + "  " + modelData.count
            color: parent.active ? "#dff4ec" : "#8996a6"
            font.pixelSize: 12
            font.weight: parent.active ? Font.DemiBold : Font.Normal
          }

          MouseArea {
            id: tabMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: page.selectedGroup = modelData.key
          }
        }
      }

      Item { Layout.fillWidth: true }

      Text {
        text: page.selectedGroupLabel + " " + page.selectedGroupCount + "명"
        color: "#8492a2"
        font.pixelSize: 11
      }
    }

    Panel {
      Layout.fillWidth: true
      Layout.preferredHeight: 194
      padding: 14

      ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
          Layout.fillWidth: true

          ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
              text: "선수단 구조 진단"
              color: "#f1f4f6"
              font.pixelSize: 16
              font.weight: Font.DemiBold
            }

            Text {
              text: "구단주 관점에서 포지션·계약·임대·나이 균형을 자동 진단하고, 저장된 구단주·감독·단장 의견을 함께 보여줍니다."
              color: "#7f8d9e"
              font.pixelSize: 11
              wrapMode: Text.WordWrap
              Layout.fillWidth: true
            }
          }

          Rectangle {
            implicitWidth: 114
            implicitHeight: 28
            radius: 6
            color: "#1d2935"
            border.color: "#34495b"

            Text {
              anchors.centerIn: parent
              text: "자동 + 의견"
              color: "#b8c5d0"
              font.pixelSize: 10
            }
          }
        }

        ListView {
          id: diagnosticList
          Layout.fillWidth: true
          Layout.fillHeight: true
          orientation: ListView.Horizontal
          spacing: 10
          clip: true
          model: game.squadDiagnostics
          boundsBehavior: Flickable.StopAtBounds
          ScrollBar.horizontal: ScrollBar { }

          delegate: Rectangle {
            required property var modelData
            property var rowData: modelData

            width: 274
            height: diagnosticList.height - 8
            radius: 10
            color: "#151f29"
            border.color: "#253341"

            ColumnLayout {
              anchors.fill: parent
              anchors.margins: 12
              spacing: 8

              RowLayout {
                Layout.fillWidth: true

                Rectangle {
                  width: 8
                  height: 30
                  radius: 4
                  color: rowData.color
                }

                ColumnLayout {
                  Layout.fillWidth: true
                  spacing: 1

                  Text {
                    text: rowData.label
                    color: "#edf1f3"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                  }

                  Text {
                    text: "총 " + rowData.total + "명 · 주급 " + rowData.weeklyWage
                    color: "#8795a5"
                    font.pixelSize: 10
                  }
                }

                Rectangle {
                  implicitWidth: needLabel.implicitWidth + 18
                  implicitHeight: 24
                  radius: 12
                  color: "#101821"
                  border.color: rowData.needColor

                  Text {
                    id: needLabel
                    anchors.centerIn: parent
                    text: rowData.needLabel
                    color: rowData.needColor
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                  }
                }
              }

              GridLayout {
                Layout.fillWidth: true
                columns: 4
                rowSpacing: 4
                columnSpacing: 6

                Text { text: "1군"; color: "#748296"; font.pixelSize: 9 }
                Text { text: rowData.firstTeam; color: "#dce4e9"; font.pixelSize: 11; font.weight: Font.DemiBold }
                Text { text: "리저브"; color: "#748296"; font.pixelSize: 9 }
                Text { text: rowData.reserve; color: "#dce4e9"; font.pixelSize: 11; font.weight: Font.DemiBold }
                Text { text: "유스"; color: "#748296"; font.pixelSize: 9 }
                Text { text: rowData.youth; color: "#dce4e9"; font.pixelSize: 11; font.weight: Font.DemiBold }
                Text { text: "임대"; color: "#748296"; font.pixelSize: 9 }
                Text { text: rowData.outgoingLoan + "/" + rowData.incomingLoan; color: "#dce4e9"; font.pixelSize: 11; font.weight: Font.DemiBold }
                Text { text: "U21"; color: "#748296"; font.pixelSize: 9 }
                Text { text: rowData.under21; color: "#dce4e9"; font.pixelSize: 11; font.weight: Font.DemiBold }
                Text { text: "30+"; color: "#748296"; font.pixelSize: 9 }
                Text { text: rowData.over30; color: "#dce4e9"; font.pixelSize: 11; font.weight: Font.DemiBold }
              }

              Text {
                text: rowData.summary
                color: "#aab5c0"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
              }

              Text {
                text: "감독: " + rowData.managerSignal
                color: "#8ec5e7"
                font.pixelSize: 10
                elide: Text.ElideRight
                Layout.fillWidth: true
              }

              Text {
                text: "단장: " + rowData.directorSignal
                color: "#d7bc76"
                font.pixelSize: 10
                elide: Text.ElideRight
                Layout.fillWidth: true
              }

              Text {
                text: "저장 의견  구단주 " + rowData.ownerOpinionCount
                      + " · 감독 " + rowData.managerOpinionCount
                      + " · 단장 " + rowData.directorOpinionCount
                color: "#9aa6b2"
                font.pixelSize: 9
                elide: Text.ElideRight
                Layout.fillWidth: true
              }
            }
          }
        }
      }
    }

    Panel {
      Layout.fillWidth: true
      Layout.fillHeight: true
      padding: 0

      ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
          Layout.fillWidth: true
          Layout.preferredHeight: 42
          color: "#101821"

          RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 8

            Text { text: "포지션"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 68 }
            Text { text: "선수 / 의견"; color: "#7e8b9a"; font.pixelSize: 10; Layout.fillWidth: true }
            Text { text: "나이"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 54; horizontalAlignment: Text.AlignHCenter }
            Text { text: "국적"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 132 }
            Text { text: "상태"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 90 }
            Text { text: "합류 기록"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 112 }
            Text { text: "자료 상태"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 158 }
            Text { text: "구단주 의견"; color: "#7e8b9a"; font.pixelSize: 10; Layout.preferredWidth: 236 }
          }
        }

        ListView {
          id: squadList
          Layout.fillWidth: true
          Layout.fillHeight: true
          clip: true
          model: page.filteredSquad
          boundsBehavior: Flickable.StopAtBounds
          ScrollBar.vertical: ScrollBar { }

          delegate: Rectangle {
            required property var modelData
            required property int index
            property var rowData: modelData

            width: squadList.width
            height: 76
            color: index % 2 === 0 ? "#18222d" : "#151f29"

            RowLayout {
              anchors.fill: parent
              anchors.leftMargin: 16
              anchors.rightMargin: 16
              spacing: 8

              Text {
                text: rowData.position
                color: "#6fc29f"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                Layout.preferredWidth: 68
              }

              ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Text {
                  text: rowData.name
                  color: "#edf1f3"
                  font.pixelSize: 12
                  font.weight: Font.Medium
                  elide: Text.ElideRight
                  Layout.fillWidth: true
                }

                RowLayout {
                  Layout.fillWidth: true
                  spacing: 5

                  Rectangle {
                    implicitWidth: ownerOpinionText.implicitWidth + 14
                    implicitHeight: 18
                    radius: 9
                    color: "#101821"
                    border.color: rowData.ownerOpinionColor

                    Text {
                      id: ownerOpinionText
                      anchors.centerIn: parent
                      text: "구단주 " + rowData.ownerOpinionLabel
                      color: rowData.ownerOpinionColor
                      font.pixelSize: 9
                    }
                  }

                  Rectangle {
                    implicitWidth: managerOpinionText.implicitWidth + 14
                    implicitHeight: 18
                    radius: 9
                    color: "#101821"
                    border.color: rowData.managerOpinionColor

                    Text {
                      id: managerOpinionText
                      anchors.centerIn: parent
                      text: "감독 " + rowData.managerOpinionLabel
                      color: rowData.managerOpinionColor
                      font.pixelSize: 9
                    }
                  }

                  Rectangle {
                    implicitWidth: directorOpinionText.implicitWidth + 14
                    implicitHeight: 18
                    radius: 9
                    color: "#101821"
                    border.color: rowData.directorOpinionColor

                    Text {
                      id: directorOpinionText
                      anchors.centerIn: parent
                      text: "단장 " + rowData.directorOpinionLabel
                      color: rowData.directorOpinionColor
                      font.pixelSize: 9
                    }
                  }

                  Item { Layout.fillWidth: true }
                }
              }

              Text {
                text: rowData.age
                color: "#9aa5b3"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                Layout.preferredWidth: 54
              }

              Text {
                text: rowData.nationality
                color: "#9aa5b3"
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.preferredWidth: 132
              }

              Text {
                text: rowData.status
                color: rowData.status === "재활 중" ? "#d7a16b" : "#74bea0"
                font.pixelSize: 10
                Layout.preferredWidth: 90
              }

              Text {
                text: rowData.joinedDate
                color: "#7f8d9e"
                font.pixelSize: 10
                elide: Text.ElideRight
                Layout.preferredWidth: 112
              }

              Text {
                text: rowData.confidenceLabel
                color: rowData.confidenceLabel.indexOf("잠정") >= 0 ? "#cfb276" : "#8296aa"
                font.pixelSize: 10
                elide: Text.ElideRight
                Layout.preferredWidth: 158
              }

              RowLayout {
                Layout.preferredWidth: 236
                spacing: 4

                Button {
                  text: "유지"
                  Layout.preferredWidth: 42
                  Layout.preferredHeight: 26
                  font.pixelSize: 9
                  onClicked: game.setOwnerSquadOpinion(
                               rowData.id,
                               "keep",
                               3,
                               "구단주가 현재 전력 유지 의견을 표시")
                }

                Button {
                  text: "임대"
                  Layout.preferredWidth: 42
                  Layout.preferredHeight: 26
                  font.pixelSize: 9
                  onClicked: game.setOwnerSquadOpinion(
                               rowData.id,
                               "loan",
                               3,
                               "구단주가 임대 활용 검토 의견을 표시")
                }

                Button {
                  text: "매각"
                  Layout.preferredWidth: 42
                  Layout.preferredHeight: 26
                  font.pixelSize: 9
                  onClicked: game.setOwnerSquadOpinion(
                               rowData.id,
                               "sale",
                               4,
                               "구단주가 매각 후보 의견을 표시")
                }

                Button {
                  text: "방출"
                  Layout.preferredWidth: 46
                  Layout.preferredHeight: 26
                  font.pixelSize: 9
                  onClicked: game.setOwnerSquadOpinion(
                               rowData.id,
                               "release",
                               4,
                               "구단주가 방출 후보 의견을 표시")
                }

                Button {
                  text: "해제"
                  enabled: rowData.ownerOpinionType !== ""
                  Layout.preferredWidth: 46
                  Layout.preferredHeight: 26
                  font.pixelSize: 9
                  onClicked: game.clearOwnerSquadOpinion(rowData.id)
                }
              }
            }
          }
        }
      }
    }
  }
}
