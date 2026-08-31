import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Flickable {
  id: page

  property var cadenceOptions: [
    { "key": "off", "label": "개최 안 함" },
    { "key": "urgent_only", "label": "긴급 안건만" },
    { "key": "monthly", "label": "월간" },
    { "key": "quarterly", "label": "분기" },
    { "key": "transfer_windows", "label": "이적시장 전후" },
    { "key": "season_milestones", "label": "시즌 주요 시점" }
  ]
  property var agendaOptions: [
    { "key": "general", "label": "종합 운영" },
    { "key": "squad_planning", "label": "선수단 계획" },
    { "key": "transfer_market", "label": "이적시장" },
    { "key": "finance", "label": "재정" },
    { "key": "staff", "label": "스태프/권한" },
    { "key": "scouting", "label": "스카우팅" },
    { "key": "crisis", "label": "긴급 안건" }
  ]

  function optionIndex(options, key) {
    for (var i = 0; i < options.length; ++i) {
      if (options[i].key === key) {
        return i
      }
    }
    return 0
  }

  function optionKey(options, index, fallbackKey) {
    if (index < 0 || index >= options.length) {
      return fallbackKey
    }
    return options[index].key
  }

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

    ColumnLayout {
      Layout.fillWidth: true
      spacing: 3

      Text {
        text: "감독"
        color: "#f1f4f6"
        font.pixelSize: 26
        font.weight: Font.DemiBold
      }

      Text {
        text: "구단주는 감독을 평가하고 방향을 요구하지만 경기 중 전술과 교체는 직접 조작하지 않습니다"
        color: "#7f8d9e"
        font.pixelSize: 13
      }
    }

    RowLayout {
      Layout.fillWidth: true
      spacing: 14

      Panel {
        Layout.fillWidth: true
        Layout.preferredHeight: 260

        RowLayout {
          anchors.fill: parent
          spacing: 22

          Rectangle {
            Layout.preferredWidth: 112
            Layout.preferredHeight: 112
            radius: 56
            color: "#263443"
            border.color: "#4b6275"
            border.width: 2

            Text {
              anchors.centerIn: parent
              text: "AF"
              color: "#dce4e9"
              font.pixelSize: 34
              font.weight: Font.DemiBold
            }
          }

          ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
              text: game.managerName
              color: "#f2f5f7"
              font.pixelSize: 25
              font.weight: Font.DemiBold
            }

            Text {
              text: "Manager · Scotland"
              color: "#7f8d9e"
              font.pixelSize: 12
            }

            Rectangle {
              implicitWidth: confidenceText.implicitWidth + 18
              implicitHeight: 27
              radius: 13
              color: "#1e453a"

              Text {
                id: confidenceText
                anchors.centerIn: parent
                text: "이사회 신뢰 " + game.managerConfidence + "/100"
                color: "#7bc9aa"
                font.pixelSize: 11
                font.weight: Font.Medium
              }
            }

            Text {
              Layout.fillWidth: true
              text: game.managerIdentity
              color: "#a2adba"
              font.pixelSize: 12
              wrapMode: Text.WordWrap
            }
          }
        }
      }

      Panel {
        Layout.fillWidth: true
        Layout.preferredHeight: 260

        ColumnLayout {
          anchors.fill: parent
          spacing: 12

          Text {
            text: "현재 이사회 평가"
            color: "#f0f3f5"
            font.pixelSize: 16
            font.weight: Font.DemiBold
          }

          ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 100
            value: game.managerConfidence
            background: Rectangle { color: "#0f171f"; radius: 4 }
            contentItem: Item {
              Rectangle {
                width: parent.width * (game.managerConfidence / 100)
                height: parent.height
                radius: 4
                color: "#4db58b"
              }
            }
          }

          Text {
            Layout.fillWidth: true
            text: game.managerAssessment
            color: "#9da8b5"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
          }

          Item { Layout.fillHeight: true }

          Text {
            Layout.fillWidth: true
            text: "다음 구현: 업무 위임 설정 · 상세 협상 권한 · 감독 평가 기록"
            color: "#6f7d8e"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }
        }
      }
    }

    Panel {
      Layout.fillWidth: true
      Layout.preferredHeight: game.operatingMeetings.length > 0 ? 542 : 408

      ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
          Layout.fillWidth: true
          spacing: 12

          ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Text {
              text: "구단주 운영 회의"
              color: "#f0f3f5"
              font.pixelSize: 17
              font.weight: Font.DemiBold
            }

            Text {
              Layout.fillWidth: true
              text: "원하는 시점에 감독·단장·재정·스카우팅 담당을 소집하고, 회의 결과를 저장된 구단 운영 기록과 받은 편지함에 남깁니다."
              color: "#8f9bad"
              font.pixelSize: 11
              wrapMode: Text.WordWrap
            }
          }

          Rectangle {
            implicitWidth: cadenceBadge.implicitWidth + 18
            implicitHeight: 28
            radius: 14
            color: "#1c3145"

            Text {
              id: cadenceBadge
              anchors.centerIn: parent
              text: game.operatingMeetingCadenceLabel + " · 다음 " + game.nextOperatingMeetingDate
              color: "#9fc4df"
              font.pixelSize: 11
              font.weight: Font.Medium
            }
          }
        }

        GridLayout {
          Layout.fillWidth: true
          columns: 2
          columnSpacing: 18
          rowSpacing: 8

          ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
              text: "정기 회의 주기"
              color: "#aeb8c4"
              font.pixelSize: 11
            }

            ComboBox {
              id: cadenceBox
              Layout.fillWidth: true
              model: page.cadenceOptions
              textRole: "label"
              valueRole: "key"
              currentIndex: page.optionIndex(page.cadenceOptions, game.operatingMeetingCadence)
            }
          }

          ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
              text: "즉시 소집 안건"
              color: "#aeb8c4"
              font.pixelSize: 11
            }

            ComboBox {
              id: agendaBox
              Layout.fillWidth: true
              model: page.agendaOptions
              textRole: "label"
              valueRole: "key"
              currentIndex: 0
            }
          }
        }

        RowLayout {
          Layout.fillWidth: true
          spacing: 14

          CheckBox {
            id: autoAssignBox
            text: "참석자 자동 배정"
            checked: game.operatingMeetingAutoAssign
            palette.windowText: "#dce4e9"
          }

          Text {
            Layout.fillWidth: true
            text: autoAssignBox.checked
                  ? "안건에 맞춰 필요한 역할이 자동으로 들어옵니다."
                  : "구단주가 이번 회의 참석 역할을 직접 고릅니다."
            color: "#7f8d9e"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }
        }

        GridLayout {
          Layout.fillWidth: true
          columns: 5
          columnSpacing: 8
          rowSpacing: 4
          enabled: !autoAssignBox.checked
          opacity: autoAssignBox.checked ? 0.42 : 1

          CheckBox {
            id: managerCheck
            text: "감독"
            checked: game.operatingMeetingIncludeManager
            palette.windowText: "#dce4e9"
          }

          CheckBox {
            id: directorCheck
            text: "단장/운영"
            checked: game.operatingMeetingIncludeDirector
            palette.windowText: "#dce4e9"
          }

          CheckBox {
            id: financeCheck
            text: "재정"
            checked: game.operatingMeetingIncludeFinance
            palette.windowText: "#dce4e9"
          }

          CheckBox {
            id: scoutingCheck
            text: "스카우팅"
            checked: game.operatingMeetingIncludeScouting
            palette.windowText: "#dce4e9"
          }

          CheckBox {
            id: medicalCheck
            text: "의료"
            checked: game.operatingMeetingIncludeMedical
            palette.windowText: "#dce4e9"
          }
        }

        RowLayout {
          Layout.fillWidth: true
          spacing: 10

          Button {
            text: "주기 저장"
            onClicked: game.setOperatingMeetingCadence(
              page.optionKey(page.cadenceOptions, cadenceBox.currentIndex, "off"))
          }

          Button {
            text: "지금 소집"
            highlighted: true
            onClicked: game.callOperatingMeeting(
              page.optionKey(page.agendaOptions, agendaBox.currentIndex, "general"),
              autoAssignBox.checked,
              managerCheck.checked,
              directorCheck.checked,
              financeCheck.checked,
              scoutingCheck.checked,
              medicalCheck.checked)
          }

          Text {
            Layout.fillWidth: true
            text: "선수단 계획 안건은 감독·단장 포지션군 의견까지 자동 반영됩니다."
            color: "#7fbea6"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }
        }

        Rectangle {
          Layout.fillWidth: true
          Layout.preferredHeight: 1
          color: "#263444"
        }

        Text {
          text: "최근 회의"
          color: "#dfe6eb"
          font.pixelSize: 13
          font.weight: Font.DemiBold
        }

        Text {
          Layout.fillWidth: true
          visible: game.operatingMeetings.length === 0
          text: "아직 기록된 운영 회의가 없습니다. 회의를 소집하면 결과 요약과 참석자가 여기에 쌓입니다."
          color: "#7f8d9e"
          font.pixelSize: 11
          wrapMode: Text.WordWrap
        }

        Repeater {
          model: Math.min(3, game.operatingMeetings.length)

          Rectangle {
            required property int index
            property var rowData: game.operatingMeetings[index]

            Layout.fillWidth: true
            Layout.preferredHeight: 86
            radius: 8
            color: "#121a23"
            border.color: "#253445"
            border.width: 1

            ColumnLayout {
              anchors.fill: parent
              anchors.margins: 10
              spacing: 4

              RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                  implicitWidth: agendaText.implicitWidth + 14
                  implicitHeight: 22
                  radius: 11
                  color: rowData.agendaColor
                  opacity: 0.82

                  Text {
                    id: agendaText
                    anchors.centerIn: parent
                    text: rowData.agendaLabel
                    color: "#101820"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                  }
                }

                Text {
                  text: rowData.date + " · " + rowData.attendeeModeLabel
                  color: "#8f9bad"
                  font.pixelSize: 10
                }

                Item { Layout.fillWidth: true }

                Text {
                  text: rowData.statusLabel
                  color: "#7fbea6"
                  font.pixelSize: 10
                  font.weight: Font.DemiBold
                }
              }

              Text {
                Layout.fillWidth: true
                text: rowData.attendeeSummary
                color: "#aab5c1"
                font.pixelSize: 10
                elide: Text.ElideRight
              }

              Text {
                Layout.fillWidth: true
                text: rowData.summary
                color: "#d1d9df"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
              }
            }
          }
        }
      }
    }

    Text {
      text: "영입 계획과 신뢰 평가"
      color: "#e7ebee"
      font.pixelSize: 19
      font.weight: Font.DemiBold
    }

    Repeater {
      model: game.recruitmentPlans

      Panel {
        required property var modelData
        property var rowData: modelData

        Layout.fillWidth: true
        Layout.preferredHeight: 196

        ColumnLayout {
          anchors.fill: parent
          spacing: 9

          RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
              Layout.fillWidth: true
              spacing: 2
              Text {
                text: rowData.title
                color: "#f0f4f6"
                font.pixelSize: 17
                font.weight: Font.DemiBold
              }
              Text {
                text: rowData.requestedBy + " · 평가일 " + rowData.evaluationDate
                color: "#8290a0"
                font.pixelSize: 10
              }
            }
            Text {
              text: rowData.status
              color: rowData.statusColor
              font.pixelSize: 11
              font.weight: Font.DemiBold
            }
          }

          Text {
            Layout.fillWidth: true
            text: rowData.description
            color: "#a4afb9"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
          }

          RowLayout {
            Layout.fillWidth: true
            Text {
              text: rowData.completedSignings + "/" + rowData.targetSignings + "명"
              color: "#dce5ea"
              font.pixelSize: 12
              font.weight: Font.DemiBold
            }
            ProgressBar {
              Layout.fillWidth: true
              from: 0
              to: 100
              value: rowData.progress
            }
            Text {
              text: "우선순위 " + rowData.priorityScore + "점"
              color: "#9fc4df"
              font.pixelSize: 10
            }
          }

          Text {
            Layout.fillWidth: true
            text: "최소 기대 " + rowData.minimumAcceptableSignings
                  + "명 · 일부 후보를 놓쳐도 이 기준을 채우면 감독 신뢰는 하락하지 않습니다."
            color: "#7fbea6"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }
        }
      }
    }

    Panel {
      Layout.fillWidth: true
      Layout.preferredHeight: 225

      ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Text {
          text: "권한 경계 · MVP 적용 원칙"
          color: "#f0f3f5"
          font.pixelSize: 16
          font.weight: Font.DemiBold
        }

        GridLayout {
          Layout.fillWidth: true
          columns: 2
          columnSpacing: 22
          rowSpacing: 10

          Text { text: "✓ 감독 선임·해임과 계약"; color: "#93d1b8"; font.pixelSize: 12 }
          Text { text: "✓ 예산과 장기 축구 방향"; color: "#93d1b8"; font.pixelSize: 12 }
          Text { text: "✓ 영입·매각의 최종 승인"; color: "#93d1b8"; font.pixelSize: 12 }
          Text { text: "✓ 감독에게 요구와 압력 행사"; color: "#93d1b8"; font.pixelSize: 12 }
          Text { text: "— 경기 중 전술 직접 변경"; color: "#8a96a5"; font.pixelSize: 12 }
          Text { text: "— 선발과 교체 직접 조작"; color: "#8a96a5"; font.pixelSize: 12 }
        }
      }
    }
  }
}
