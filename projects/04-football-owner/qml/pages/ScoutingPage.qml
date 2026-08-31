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

  Dialog {
    id: assignmentDialog

    property string playerId: ""
    property string playerName: ""

    function openFor(playerIdValue, playerNameValue) {
      playerId = playerIdValue
      playerName = playerNameValue
      scoutCombo.currentIndex = 0
      depthCombo.currentIndex = 1
      focusCombo.currentIndex = 0
      open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 520
    title: playerName + " 스카우팅 배정"

    ListModel {
      id: depthModel
      ListElement { label: "빠른 관찰 · 3일 · 지식 +20"; key: "quick" }
      ListElement { label: "표준 관찰 · 7일 · 지식 +45"; key: "standard" }
      ListElement { label: "종합 관찰 · 14일 · 지식 +70"; key: "comprehensive" }
    }

    ListModel {
      id: focusModel
      ListElement { label: "종합 평가"; key: "overall" }
      ListElement { label: "현재 능력"; key: "current_ability" }
      ListElement { label: "잠재력"; key: "potential" }
      ListElement { label: "이적 가능성·요구 조건"; key: "availability" }
    }

    contentItem: ColumnLayout {
      spacing: 12

      Text {
        Layout.fillWidth: true
        text: "관찰 기간 중간과 종료일에 보고서가 도착합니다. 담당자의 평가 능력과 중점 항목에 따라 오차 범위가 달라집니다."
        color: "#8896a5"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
      }

      Label { text: "담당자"; color: "#c5ced7" }
      ComboBox {
        id: scoutCombo
        Layout.fillWidth: true
        model: game.scouts
        textRole: "name"
        valueRole: "id"
      }

      Label { text: "관찰 깊이"; color: "#c5ced7" }
      ComboBox {
        id: depthCombo
        Layout.fillWidth: true
        model: depthModel
        textRole: "label"
        valueRole: "key"
      }

      Label { text: "중점 항목"; color: "#c5ced7" }
      ComboBox {
        id: focusCombo
        Layout.fillWidth: true
        model: focusModel
        textRole: "label"
        valueRole: "key"
      }

      RowLayout {
        Layout.fillWidth: true
        Item { Layout.fillWidth: true }
        Button {
          text: "취소"
          onClicked: assignmentDialog.close()
        }
        Button {
          text: "배정"
          enabled: game.scouts.length > 0
          onClicked: {
            game.startScoutingAssignment(
                  assignmentDialog.playerId,
                  scoutCombo.currentValue,
                  depthCombo.currentValue,
                  focusCombo.currentValue)
            assignmentDialog.close()
          }
        }
      }
    }
  }

  Dialog {
    id: directiveDialog

    function openDirective() {
      directiveTypeCombo.currentIndex = 0
      directiveScoutCombo.currentIndex = 0
      directiveTargetCombo.currentIndex = 0
      directiveDepthCombo.currentIndex = 1
      directiveFocusCombo.currentIndex = 0
      directiveRegionCombo.currentIndex = 0
      directivePositionCombo.currentIndex = 0
      directiveStyleCombo.currentIndex = 0
      directiveCountryField.text = ""
      directiveLeagueField.text = ""
      directiveMinAge.value = 0
      directiveMaxAge.value = 0
      directiveHomegrown.checked = false
      directiveBudget.value = 0
      open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 620
    title: "스카우팅 지시 생성"

    ListModel {
      id: directiveTypeModel
      ListElement { label: "범위형 지시"; key: "range" }
      ListElement { label: "특정 선수 지시"; key: "target" }
    }

    ListModel {
      id: directiveDepthModel
      ListElement { label: "빠른 관찰 · 3일"; key: "quick" }
      ListElement { label: "표준 관찰 · 7일"; key: "standard" }
      ListElement { label: "종합 관찰 · 14일"; key: "comprehensive" }
    }

    ListModel {
      id: directiveFocusModel
      ListElement { label: "종합 평가"; key: "overall" }
      ListElement { label: "현재 능력"; key: "current_ability" }
      ListElement { label: "잠재력"; key: "potential" }
      ListElement { label: "이적 가능성·요구 조건"; key: "availability" }
    }

    ListModel {
      id: directiveRegionModel
      ListElement { label: "전체 지역"; key: "any" }
      ListElement { label: "잉글랜드"; key: "England" }
      ListElement { label: "영국·아일랜드"; key: "United Kingdom & Ireland" }
      ListElement { label: "프랑스"; key: "France" }
    }

    ListModel {
      id: directivePositionModel
      ListElement { label: "전체 포지션"; key: "any" }
      ListElement { label: "공격"; key: "ATT" }
      ListElement { label: "미드필드"; key: "MID" }
      ListElement { label: "수비"; key: "DEF" }
      ListElement { label: "골키퍼"; key: "GK" }
    }

    ListModel {
      id: directiveStyleModel
      ListElement { label: "전체 스타일"; key: "any" }
      ListElement { label: "높은 잠재력"; key: "potential" }
      ListElement { label: "리더십"; key: "leadership" }
      ListElement { label: "피지컬"; key: "physical" }
      ListElement { label: "창조성"; key: "creative" }
      ListElement { label: "다목적"; key: "versatile" }
    }

    contentItem: ColumnLayout {
      spacing: 10

      Text {
        Layout.fillWidth: true
        text: "범위형 지시는 현재 후보 풀에서 조건에 맞는 선수를 찾아 우선 관찰 대상으로 연결합니다. 전체 로스터가 들어오기 전까지는 후보 8명을 기준으로 검증합니다."
        color: "#8896a5"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
      }

      GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 12
        rowSpacing: 8

        Label { text: "지시 유형"; color: "#c5ced7" }
        ComboBox {
          id: directiveTypeCombo
          Layout.fillWidth: true
          model: directiveTypeModel
          textRole: "label"
          valueRole: "key"
        }

        Label { text: "담당자"; color: "#c5ced7" }
        ComboBox {
          id: directiveScoutCombo
          Layout.fillWidth: true
          model: game.scouts
          textRole: "name"
          valueRole: "id"
        }

        Label {
          text: "대상 선수"
          color: "#c5ced7"
          visible: directiveTypeCombo.currentValue === "target"
        }
        ComboBox {
          id: directiveTargetCombo
          Layout.fillWidth: true
          visible: directiveTypeCombo.currentValue === "target"
          model: game.transferCandidates
          textRole: "name"
          valueRole: "playerId"
        }

        Label { text: "관찰 깊이"; color: "#c5ced7" }
        ComboBox {
          id: directiveDepthCombo
          Layout.fillWidth: true
          model: directiveDepthModel
          textRole: "label"
          valueRole: "key"
        }

        Label { text: "중점"; color: "#c5ced7" }
        ComboBox {
          id: directiveFocusCombo
          Layout.fillWidth: true
          model: directiveFocusModel
          textRole: "label"
          valueRole: "key"
        }

        Label {
          text: "지역"
          color: "#c5ced7"
          visible: directiveTypeCombo.currentValue === "range"
        }
        ComboBox {
          id: directiveRegionCombo
          Layout.fillWidth: true
          visible: directiveTypeCombo.currentValue === "range"
          model: directiveRegionModel
          textRole: "label"
          valueRole: "key"
        }

        Label {
          text: "국가 직접 입력"
          color: "#c5ced7"
          visible: directiveTypeCombo.currentValue === "range"
        }
        TextField {
          id: directiveCountryField
          Layout.fillWidth: true
          visible: directiveTypeCombo.currentValue === "range"
          placeholderText: "예: England, France"
        }

        Label {
          text: "리그/소속 힌트"
          color: "#c5ced7"
          visible: directiveTypeCombo.currentValue === "range"
        }
        TextField {
          id: directiveLeagueField
          Layout.fillWidth: true
          visible: directiveTypeCombo.currentValue === "range"
          placeholderText: "예: Nottingham, Sheffield"
        }

        Label {
          text: "포지션"
          color: "#c5ced7"
          visible: directiveTypeCombo.currentValue === "range"
        }
        ComboBox {
          id: directivePositionCombo
          Layout.fillWidth: true
          visible: directiveTypeCombo.currentValue === "range"
          model: directivePositionModel
          textRole: "label"
          valueRole: "key"
        }

        Label {
          text: "스타일"
          color: "#c5ced7"
          visible: directiveTypeCombo.currentValue === "range"
        }
        ComboBox {
          id: directiveStyleCombo
          Layout.fillWidth: true
          visible: directiveTypeCombo.currentValue === "range"
          model: directiveStyleModel
          textRole: "label"
          valueRole: "key"
        }

        Label {
          text: "나이 범위"
          color: "#c5ced7"
          visible: directiveTypeCombo.currentValue === "range"
        }
        RowLayout {
          Layout.fillWidth: true
          visible: directiveTypeCombo.currentValue === "range"

          SpinBox {
            id: directiveMinAge
            from: 0
            to: 45
            value: 0
          }

          Text {
            text: "–"
            color: "#8b98a6"
          }

          SpinBox {
            id: directiveMaxAge
            from: 0
            to: 45
            value: 0
          }

          Text {
            text: "0은 제한 없음"
            color: "#748296"
            font.pixelSize: 10
          }
        }

        Label {
          text: "예산 상한"
          color: "#c5ced7"
          visible: directiveTypeCombo.currentValue === "range"
        }
        SpinBox {
          id: directiveBudget
          Layout.fillWidth: true
          visible: directiveTypeCombo.currentValue === "range"
          from: 0
          to: 5000000
          stepSize: 250000
          value: 0
          textFromValue: function(value) {
            return value === 0 ? "제한 없음" : "£" + value
          }
          valueFromText: function(text) {
            var numberText = text.replace("£", "")
            if (numberText === "제한 없음") {
              return 0
            }
            return Number(numberText)
          }
        }
      }

      CheckBox {
        id: directiveHomegrown
        visible: directiveTypeCombo.currentValue === "range"
        text: "홈그로운 후보 우선"
        palette.windowText: "#dce4e9"
      }

      RowLayout {
        Layout.fillWidth: true
        Item { Layout.fillWidth: true }
        Button {
          text: "취소"
          onClicked: directiveDialog.close()
        }
        Button {
          text: "지시 생성"
          highlighted: true
          enabled: game.scouts.length > 0
          onClicked: {
            if (directiveTypeCombo.currentValue === "target") {
              game.startTargetScoutingDirective(
                    directiveTargetCombo.currentValue,
                    directiveScoutCombo.currentValue,
                    directiveDepthCombo.currentValue,
                    directiveFocusCombo.currentValue)
            } else {
              game.startRangeScoutingDirective(
                    directiveScoutCombo.currentValue,
                    directiveDepthCombo.currentValue,
                    directiveFocusCombo.currentValue,
                    directiveRegionCombo.currentValue,
                    directiveCountryField.text,
                    directiveLeagueField.text,
                    directivePositionCombo.currentValue,
                    directiveStyleCombo.currentValue,
                    directiveMinAge.value,
                    directiveMaxAge.value,
                    directiveHomegrown.checked,
                    directiveBudget.value)
            }
            directiveDialog.close()
          }
        }
      }
    }
  }

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
        text: "스카우팅"
        color: "#f1f4f6"
        font.pixelSize: 26
        font.weight: Font.DemiBold
      }

      Text {
        text: "담당자를 배정해 시간이 지날수록 선수 평가와 이적 조건의 오차 범위를 줄입니다"
        color: "#7f8d9e"
        font.pixelSize: 13
      }
    }

    GridLayout {
      Layout.fillWidth: true
      columns: 4
      columnSpacing: 12

      StatCard {
        Layout.fillWidth: true
        label: "진행 중"
        value: game.activeScoutingAssignmentCount + "건"
        hint: "중간·최종 보고 예정"
        accent: "#63a7d8"
      }

      StatCard {
        Layout.fillWidth: true
        label: "전체 배정 한도"
        value: game.scoutingCapacity + "건"
        hint: "담당자 동시 관찰"
        accent: "#aa83d4"
      }

      StatCard {
        Layout.fillWidth: true
        label: "상세 보고서"
        value: game.completedScoutingReportCount + "건"
        hint: "지식 70 이상"
        accent: "#4db58b"
      }

      StatCard {
        Layout.fillWidth: true
        label: "이적 가용 현금"
        value: game.availableTransferCash
        hint: "미정산·진행 약정 제외"
        accent: "#d2ab61"
      }
    }

    Rectangle {
      Layout.fillWidth: true
      Layout.preferredHeight: 66
      radius: 8
      color: "#242b25"
      border.color: "#5f6442"

      Text {
        anchors.fill: parent
        anchors.margins: 13
        text: "보고서 규칙 · 빠른 3일, 표준 7일, 종합 14일 관찰을 선택할 수 있습니다. 보고서는 마지막 관찰 30일 뒤 오래된 정보로 표시되며 다시 배정해 갱신할 수 있습니다. 숨겨진 기준 능력과 기능 검증용 금액 자체는 바뀌지 않고 표시 오차만 줄어듭니다."
        color: "#d4d3ac"
        font.pixelSize: 11
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
      }
    }

    Panel {
      Layout.fillWidth: true
      Layout.preferredHeight: game.scoutingDirectives.length > 0 ? 310 : 156

      ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
          Layout.fillWidth: true
          spacing: 12

          ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Text {
              text: "스카우팅 지시 v2"
              color: "#f0f4f6"
              font.pixelSize: 17
              font.weight: Font.DemiBold
            }

            Text {
              Layout.fillWidth: true
              text: "특정 선수 관찰뿐 아니라 지역·국가·나이·포지션·스타일·예산 범위를 지시할 수 있습니다. 현재는 후보 8명 풀에서 먼저 검증합니다."
              color: "#8b98a6"
              font.pixelSize: 11
              wrapMode: Text.WordWrap
            }
          }

          Button {
            text: "지시 생성"
            highlighted: true
            onClicked: directiveDialog.openDirective()
          }
        }

        Text {
          Layout.fillWidth: true
          visible: game.scoutingDirectives.length === 0
          text: "아직 저장된 스카우팅 지시가 없습니다. 지시를 만들면 조건, 추천 점수, 연결 배정과 완료 상태가 여기에 남습니다."
          color: "#748296"
          font.pixelSize: 11
          wrapMode: Text.WordWrap
        }

        Repeater {
          model: Math.min(3, game.scoutingDirectives.length)

          Rectangle {
            required property int index
            property var rowData: game.scoutingDirectives[index]

            Layout.fillWidth: true
            Layout.preferredHeight: 72
            radius: 8
            color: "#121a23"
            border.color: "#253445"
            border.width: 1

            RowLayout {
              anchors.fill: parent
              anchors.margins: 10
              spacing: 10

              ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                RowLayout {
                  Layout.fillWidth: true
                  Text {
                    text: rowData.typeLabel + " · " + rowData.scoutName
                    color: "#eef2f4"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                  }
                  Text {
                    text: rowData.statusLabel
                    color: rowData.statusColor
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                  }
                }

                Text {
                  Layout.fillWidth: true
                  text: rowData.filterSummary + " · 추천 " + rowData.recommendationScore
                        + " · 완료 " + rowData.dueDate
                  color: "#9aa7b4"
                  font.pixelSize: 10
                  elide: Text.ElideRight
                }

                Text {
                  Layout.fillWidth: true
                  text: rowData.summary
                  color: "#c7d0d8"
                  font.pixelSize: 10
                  elide: Text.ElideRight
                }
              }

              Button {
                visible: rowData.canCancel
                text: "취소"
                onClicked: game.cancelScoutingDirective(rowData.id)
              }
            }
          }
        }
      }
    }

    Text {
      text: "스카우팅 조직"
      color: "#e7ebee"
      font.pixelSize: 19
      font.weight: Font.DemiBold
    }

    GridLayout {
      Layout.fillWidth: true
      columns: 2
      columnSpacing: 12
      rowSpacing: 12

      Repeater {
        model: game.scouts

        Panel {
          required property var modelData
          property var rowData: modelData

          Layout.fillWidth: true
          Layout.preferredHeight: 176

          ColumnLayout {
            anchors.fill: parent
            spacing: 7

            RowLayout {
              Layout.fillWidth: true
              ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                  text: rowData.name
                  color: "#f0f4f6"
                  font.pixelSize: 17
                  font.weight: Font.DemiBold
                }
                Text {
                  text: rowData.role + " · " + rowData.region
                  color: "#8290a0"
                  font.pixelSize: 10
                }
              }
              Text {
                text: rowData.activeAssignments + "/" + rowData.capacity + " 배정"
                color: rowData.availableSlots > 0 ? "#72c19f" : "#d2ab61"
                font.pixelSize: 11
                font.weight: Font.DemiBold
              }
            }

            Text {
              text: "현재 능력 판단 " + rowData.currentAbility
                    + " · 잠재력 판단 " + rowData.potential
                    + " · 적응력 " + rowData.adaptability
              color: "#aeb8c2"
              font.pixelSize: 11
            }

            Text {
              text: rowData.confidenceLabel
              color: "#6f7d8d"
              font.pixelSize: 10
            }

            Text {
              Layout.fillWidth: true
              text: "친숙도 · " + rowData.familiaritySummary
              color: "#9fc4df"
              font.pixelSize: 10
              wrapMode: Text.WordWrap
            }
          }
        }
      }
    }

    Text {
      text: "배정 현황"
      color: "#e7ebee"
      font.pixelSize: 19
      font.weight: Font.DemiBold
    }

    Text {
      visible: game.scoutingAssignments.length === 0
      text: "아직 배정된 스카우팅이 없습니다."
      color: "#748296"
      font.pixelSize: 12
    }

    Repeater {
      model: game.scoutingAssignments

      Panel {
        required property var modelData
        property var rowData: modelData

        Layout.fillWidth: true
        Layout.preferredHeight: 126

        ColumnLayout {
          anchors.fill: parent
          spacing: 7

          RowLayout {
            Layout.fillWidth: true
            Text {
              Layout.fillWidth: true
              text: rowData.playerName + " · " + rowData.scoutName
              color: "#eef2f4"
              font.pixelSize: 15
              font.weight: Font.DemiBold
            }
            Text {
              text: rowData.status
              color: rowData.statusColor
              font.pixelSize: 11
              font.weight: Font.DemiBold
            }
          }

          Text {
            text: rowData.depth + " · " + rowData.focus
                  + " · 완료 " + rowData.dueDate
            color: "#8290a0"
            font.pixelSize: 10
          }

          RowLayout {
            Layout.fillWidth: true
            ProgressBar {
              Layout.fillWidth: true
              from: 0
              to: 100
              value: rowData.progress
            }
            Text {
              text: rowData.progress + "%"
              color: "#9aa7b4"
              font.pixelSize: 10
            }
            Button {
              visible: rowData.canCancel
              text: "중단"
              onClicked: game.cancelScoutingAssignment(rowData.id)
            }
          }
        }
      }
    }

    Text {
      text: "후보 보고서"
      color: "#e7ebee"
      font.pixelSize: 19
      font.weight: Font.DemiBold
    }

    Repeater {
      model: game.transferCandidates

      Panel {
        required property var modelData
        property var rowData: modelData

        Layout.fillWidth: true
        Layout.preferredHeight: 270

        ColumnLayout {
          anchors.fill: parent
          spacing: 9

          RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
              Layout.fillWidth: true
              spacing: 2
              Text {
                text: rowData.name
                color: "#f1f4f6"
                font.pixelSize: 18
                font.weight: Font.DemiBold
              }
              Text {
                text: rowData.position + " · " + rowData.club
                      + " · 담당 " + rowData.reportScout
                color: "#8391a1"
                font.pixelSize: 10
              }
            }
            Text {
              text: rowData.reportFreshness
              color: rowData.reportStatusColor
              font.pixelSize: 10
              font.weight: Font.DemiBold
            }
          }

          RowLayout {
            Layout.fillWidth: true
            Text {
              text: "지식 " + rowData.knowledge + "/100 · " + rowData.knowledgeLabel
              color: "#9fc4df"
              font.pixelSize: 11
              font.weight: Font.DemiBold
            }
            ProgressBar {
              Layout.fillWidth: true
              from: 0
              to: 100
              value: rowData.knowledge
            }
          }

          GridLayout {
            Layout.fillWidth: true
            columns: 6
            columnSpacing: 12
            rowSpacing: 5

            Text { text: "현재 능력"; color: "#748296"; font.pixelSize: 9 }
            Text { text: rowData.currentAbility; color: "#d7e0e6"; font.pixelSize: 11 }
            Text { text: "잠재력"; color: "#748296"; font.pixelSize: 9 }
            Text { text: rowData.potential; color: "#b8a3de"; font.pixelSize: 11 }
            Text { text: "선수 관심"; color: "#748296"; font.pixelSize: 9 }
            Text { text: rowData.playerInterestDisplay; color: "#8fc7ac"; font.pixelSize: 11 }

            Text { text: "시장 가치"; color: "#748296"; font.pixelSize: 9 }
            Text { text: rowData.marketValue; color: "#d7e0e6"; font.pixelSize: 11 }
            Text { text: "예상 요구"; color: "#748296"; font.pixelSize: 9 }
            Text { text: rowData.askingPrice; color: "#dfb875"; font.pixelSize: 11 }
            Text { text: "예상 주급"; color: "#748296"; font.pixelSize: 9 }
            Text { text: rowData.expectedWage; color: "#d7e0e6"; font.pixelSize: 11 }
          }

          Text {
            Layout.fillWidth: true
            text: rowData.scoutSummary
            color: "#a1adb9"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
          }

          RowLayout {
            Layout.fillWidth: true
            Text {
              Layout.fillWidth: true
              text: "마지막 관찰 " + rowData.lastObserved
              color: "#748296"
              font.pixelSize: 10
            }
            Button {
              visible: rowData.canScout
              text: rowData.reportStale ? "보고서 갱신" : "스카우트 배정"
              onClicked: assignmentDialog.openFor(rowData.playerId, rowData.name)
            }
            Button {
              visible: rowData.isScouting
              text: "관찰 중단"
              onClicked: game.cancelScoutingAssignment(rowData.scoutingAssignmentId)
            }
          }
        }
      }
    }
  }
}
