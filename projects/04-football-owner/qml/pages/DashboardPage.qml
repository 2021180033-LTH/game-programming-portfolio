import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Flickable {
  id: page

  clip: true
  contentWidth: width
  contentHeight: content.implicitHeight + 48
  boundsBehavior: Flickable.StopAtBounds
  ScrollBar.vertical: ScrollBar { }

  ColumnLayout {
    id: content
    x: 24
    y: 24
    width: page.width - 48
    spacing: 18

    RowLayout {
      Layout.fillWidth: true

      ColumnLayout {
        Layout.fillWidth: true
        spacing: 3

        Text {
          text: "구단주 홈"
          color: "#f1f4f6"
          font.pixelSize: 26
          font.weight: Font.DemiBold
        }

        Text {
          text: game.clubName + "의 취임 첫 100일"
          color: "#7f8d9e"
          font.pixelSize: 13
        }
      }

      Rectangle {
        implicitWidth: decisionLabel.implicitWidth + 24
        implicitHeight: 30
        radius: 15
        color: game.decisionCount > 0 ? "#5a4224" : "#1d3c35"
        border.color: game.decisionCount > 0 ? "#9a7137" : "#2e6657"

        Text {
          id: decisionLabel
          anchors.centerIn: parent
          text: game.decisionCount + "개 결정 대기"
          color: game.decisionCount > 0 ? "#f0cc83" : "#85cbb0"
          font.pixelSize: 12
          font.weight: Font.Medium
        }
      }
    }

    Rectangle {
      Layout.fillWidth: true
      Layout.preferredHeight: noticeText.implicitHeight + 24
      radius: 8
      color: "#242b25"
      border.color: "#5f6442"

      Text {
        id: noticeText
        anchors.fill: parent
        anchors.margins: 12
        text: game.dataNotice
        color: "#d4d3ac"
        font.pixelSize: 11
        wrapMode: Text.WordWrap
        verticalAlignment: Text.AlignVCenter
      }
    }

    GridLayout {
      Layout.fillWidth: true
      columns: 4
      columnSpacing: 12
      rowSpacing: 12

      StatCard {
        Layout.fillWidth: true
        label: "구단 현금"
        value: game.cash
        hint: "개발용 추정 기준값"
        accent: "#4db58b"
      }

      StatCard {
        Layout.fillWidth: true
        label: "이적 예산"
        value: game.transferBudget
        hint: "구단주 승인 가능"
        accent: "#63a7d8"
      }

      StatCard {
        Layout.fillWidth: true
        label: "주급 사용률"
        value: game.wageUsagePercent + "%"
        hint: game.weeklyWageCommitment + " / " + game.weeklyWageBudget
        accent: game.wageUsagePercent >= 95 ? "#d46b65" : "#c7a35a"
      }

      StatCard {
        Layout.fillWidth: true
        label: "감독 신뢰"
        value: game.managerConfidence + "/100"
        hint: game.managerName
        accent: "#aa83d4"
      }

      StatCard {
        Layout.fillWidth: true
        label: "구단주 평판"
        value: game.ownerReputationScore + "/100"
        hint: game.ownerReputationLabel
        accent: game.ownerReputationScore >= 60 ? "#6fc29f" : "#d0b86b"
      }

      StatCard {
        Layout.fillWidth: true
        label: "팬 신뢰"
        value: game.fanTrustScore + "/100"
        hint: game.fanTrustLabel + " · " + game.fanMoodLabel
        accent: game.fanTrustScore >= 60 ? "#63a7d8" : "#d78376"
      }
    }

    RowLayout {
      Layout.fillWidth: true
      spacing: 14

      Panel {
        Layout.fillWidth: true
        Layout.preferredHeight: 240

        ColumnLayout {
          anchors.fill: parent
          spacing: 12

          RowLayout {
            Layout.fillWidth: true

            Text {
              text: "다음 경기"
              color: "#f0f3f5"
              font.pixelSize: 16
              font.weight: Font.DemiBold
              Layout.fillWidth: true
            }

            Text {
              text: game.nextFixtureDate
              color: "#7f8d9e"
              font.pixelSize: 11
            }
          }

          Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 94
            radius: 8
            color: "#111923"
            border.color: "#263444"

            Column {
              anchors.centerIn: parent
              spacing: 6

              Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: game.nextFixtureTitle
                color: "#f2f5f7"
                font.pixelSize: 20
                font.weight: Font.DemiBold
              }

              Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: game.nextFixtureStatus
                color: "#748296"
                font.pixelSize: 11
              }
            }
          }

          RowLayout {
            Layout.fillWidth: true

            Text {
              Layout.fillWidth: true
              text: "최근 결과 · " + game.recentResult
              color: "#8f9bad"
              font.pixelSize: 11
              elide: Text.ElideRight
            }

            Button {
              id: dashboardAdvanceButton
              text: "다음 일정까지"
              onClicked: game.advanceToNextEvent()
              background: Rectangle {
                color: dashboardAdvanceButton.hovered ? "#2b725f" : "#225b4d"
                radius: 6
              }
              contentItem: Text {
                text: dashboardAdvanceButton.text
                color: "white"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }
          }
        }
      }

      Panel {
        Layout.fillWidth: true
        Layout.preferredHeight: 240

        ColumnLayout {
          anchors.fill: parent
          spacing: 10

          RowLayout {
            Layout.fillWidth: true

            Text {
              text: "감독 현황"
              color: "#f0f3f5"
              font.pixelSize: 16
              font.weight: Font.DemiBold
              Layout.fillWidth: true
            }

            Text {
              text: game.managerConfidence >= 75 ? "안정적" : "검토 필요"
              color: game.managerConfidence >= 75 ? "#66c29c" : "#e0ad65"
              font.pixelSize: 11
              font.weight: Font.Medium
            }
          }

          Text {
            text: game.managerName
            color: "#f2f5f7"
            font.pixelSize: 21
            font.weight: Font.DemiBold
          }

          Text {
            text: game.managerIdentity
            color: "#8d99a9"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
          }

          ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 100
            value: game.managerConfidence
            background: Rectangle { color: "#0f171f"; radius: 3 }
            contentItem: Item {
              Rectangle {
                width: parent.width * (game.managerConfidence / 100)
                height: parent.height
                radius: 3
                color: "#4db58b"
              }
            }
          }

          Text {
            text: game.managerAssessment
            color: "#748296"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
          }
        }
      }
    }

    Panel {
      Layout.fillWidth: true
      Layout.preferredHeight: 230

      ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
          Layout.fillWidth: true

          Text {
            text: "구단주 평판과 팬 반응"
            color: "#f0f3f5"
            font.pixelSize: 16
            font.weight: Font.DemiBold
            Layout.fillWidth: true
          }

          Text {
            text: game.dominantFanTendency
            color: "#7f8d9e"
            font.pixelSize: 11
          }
        }

        Text {
          Layout.fillWidth: true
          text: "영입 승인·거절, 선수단 의견, 운영 회의, 스카우팅 지시가 작은 평판 변화로 누적됩니다. 팬 성향은 고정값이 아니라 성공과 운영 방식에 따라 천천히 움직입니다."
          color: "#8d99a9"
          font.pixelSize: 11
          wrapMode: Text.WordWrap
        }

        Rectangle {
          Layout.fillWidth: true
          Layout.preferredHeight: 1
          color: "#223040"
        }

        Text {
          visible: game.ownerActionLog.length === 0
          Layout.fillWidth: true
          text: "아직 기록된 구단주 행동이 없습니다. 첫 승인, 회의, 스카우팅 지시부터 평판 기록이 쌓입니다."
          color: "#6f7d8e"
          font.pixelSize: 12
          wrapMode: Text.WordWrap
        }

        Repeater {
          model: Math.min(3, game.ownerActionLog.length)

          Rectangle {
            required property int index
            property var rowData: game.ownerActionLog[index]

            Layout.fillWidth: true
            Layout.preferredHeight: 42
            radius: 6
            color: "#111923"
            border.color: "#223040"

            RowLayout {
              anchors.fill: parent
              anchors.leftMargin: 10
              anchors.rightMargin: 10
              spacing: 10

              Rectangle {
                Layout.preferredWidth: 7
                Layout.preferredHeight: 7
                radius: 4
                color: rowData.fanTrustDelta < 0 ? "#d78376" : "#6fc29f"
              }

              ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Text {
                  Layout.fillWidth: true
                  text: rowData.typeLabel + " · " + rowData.summary
                  color: "#eef2f5"
                  font.pixelSize: 12
                  elide: Text.ElideRight
                }

                Text {
                  Layout.fillWidth: true
                  text: rowData.impact
                  color: "#7f8d9e"
                  font.pixelSize: 10
                  elide: Text.ElideRight
                }
              }

              Text {
                text: rowData.date
                color: "#6f7d8e"
                font.pixelSize: 10
              }
            }
          }
        }
      }
    }

    Panel {
      Layout.fillWidth: true
      Layout.preferredHeight: 310

      ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
          Layout.fillWidth: true

          Text {
            text: "최근 받은 편지"
            color: "#f0f3f5"
            font.pixelSize: 16
            font.weight: Font.DemiBold
            Layout.fillWidth: true
          }

          Text {
            text: game.unreadCount + "개 읽지 않음"
            color: "#7f8d9e"
            font.pixelSize: 11
          }
        }

        Repeater {
          model: Math.min(4, game.inbox.length)

          Rectangle {
            required property int index
            property var rowData: game.inbox[index]

            Layout.fillWidth: true
            Layout.preferredHeight: 50
            radius: 6
            color: rowData.unread ? "#1d2c38" : "#141d27"

            RowLayout {
              anchors.fill: parent
              anchors.leftMargin: 12
              anchors.rightMargin: 12
              spacing: 10

              Rectangle {
                Layout.preferredWidth: 7
                Layout.preferredHeight: 7
                radius: 4
                color: rowData.categoryColor
              }

              Text {
                text: rowData.title
                color: rowData.unread ? "#eff3f5" : "#9aa5b3"
                font.pixelSize: 12
                font.weight: rowData.unread ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
                Layout.fillWidth: true
              }

              Text {
                text: rowData.category + " · " + rowData.date
                color: "#6f7d8e"
                font.pixelSize: 10
              }
            }
          }
        }
      }
    }
  }
}
