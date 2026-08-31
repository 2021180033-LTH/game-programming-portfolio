import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
  id: page

  property int selectedSection: 0
  property var selectedContract: ({})
  property var selectedLoan: ({})
  property var selectedCandidate: ({})
  property string loanDialogMode: "incoming"
  property var availableLoanCandidates: game.transferCandidates.filter(function(candidate) {
    return candidate.canLoan
  })

  function openRenewal(rowData) {
    selectedContract = rowData
    renewalWage.text = String(rowData.recommendedWagePounds)
    renewalYears.value = rowData.recommendedYears
    renewalSigning.text = String(rowData.recommendedSigningPounds)
    renewalLoyalty.text = String(rowData.recommendedLoyaltyPounds)
    renewalAppearance.text = String(rowData.recommendedAppearancePounds)
    renewalGoal.text = String(rowData.recommendedGoalPounds)
    renewalAgent.text = String(rowData.recommendedAgentPounds)
    renewalRelease.text = String(rowData.recommendedReleasePounds)
    renewalRole.editText = rowData.recommendedRole
    renewalDialog.open()
  }

  function openIncomingLoan(rowData) {
    selectedCandidate = rowData
    loanDialogMode = "incoming"
    loanFee.text = String(rowData.recommendedLoanFeePounds)
    loanWage.value = rowData.recommendedLoanWagePercent
    loanRole.editText = rowData.recommendedLoanRole
    loanEnd.text = rowData.recommendedLoanEndDate
    loanRecall.checked = true
    loanRecallDays.value = 7
    loanFutureType.currentIndex = 1
    loanFutureFee.text = String(rowData.recommendedLoanOptionPounds)
    loanDialog.open()
  }

  function openLoanCounter(rowData) {
    selectedLoan = rowData
    loanDialogMode = "counter"
    loanFee.text = String(rowData.counterLoanFeePounds)
    loanWage.value = rowData.counterWagePercentage
    loanRole.editText = rowData.counterRole
    loanEnd.text = rowData.counterEndDateIso
    loanRecall.checked = rowData.counterCanRecall
    loanRecallDays.value = rowData.counterRecallDays
    loanFutureType.currentIndex =
        rowData.counterFutureFeeType === "mandatory" ? 2
        : (rowData.counterFutureFeeType === "option" ? 1 : 0)
    loanFutureFee.text = String(rowData.counterFutureFeePounds)
    loanDialog.open()
  }

  ColumnLayout {
    anchors.fill: parent
    anchors.margins: 24
    spacing: 14

    RowLayout {
      Layout.fillWidth: true

      ColumnLayout {
        Layout.fillWidth: true
        spacing: 3

        Text {
          text: "계약·임대"
          color: "#f1f4f6"
          font.pixelSize: 26
          font.weight: Font.DemiBold
        }

        Text {
          text: "재계약 협상과 들어오고 나가는 임대의 조건·등록·복귀를 관리합니다"
          color: "#7f8d9e"
          font.pixelSize: 12
        }
      }

      Rectangle {
        implicitWidth: 275
        implicitHeight: 34
        radius: 6
        color: "#242b25"
        border.color: "#5f6442"

        Text {
          anchors.centerIn: parent
          text: "1992 규정 · 현대식 이적시장 창 미적용 · 보스만 이전"
          color: "#d4d3ac"
          font.pixelSize: 9
        }
      }
    }

    RowLayout {
      Layout.fillWidth: true
      spacing: 10

      Repeater {
        model: [
          { label: "1년 이내 만료", value: game.expiringContractCount + "명", color: "#d2ab61" },
          { label: "진행 중 재계약", value: game.activeRenewalCount + "건", color: "#63a7d8" },
          { label: "활성 임대", value: game.activeLoanCount + "건", color: "#4db58b" },
          { label: "임대 결정", value: game.pendingLoanDecisionCount + "건", color: "#e2bd6b" }
        ]

        Panel {
          required property var modelData
          Layout.fillWidth: true
          Layout.preferredHeight: 70
          padding: 12

          Column {
            anchors.centerIn: parent
            spacing: 4

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: modelData.value
              color: modelData.color
              font.pixelSize: 18
              font.weight: Font.Bold
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: modelData.label
              color: "#8391a0"
              font.pixelSize: 10
            }
          }
        }
      }
    }

    RowLayout {
      Layout.fillWidth: true
      spacing: 8

      Repeater {
        model: [
          { label: "재계약", count: game.playerContracts.length },
          { label: "임대 안건", count: game.loanDeals.length },
          { label: "들어오는 임대 제의", count: page.availableLoanCandidates.length }
        ]

        Rectangle {
          required property var modelData
          required property int index
          property bool active: page.selectedSection === index

          Layout.preferredWidth: 190
          Layout.preferredHeight: 38
          radius: 7
          color: active ? "#263d45" : (sectionMouse.containsMouse ? "#1e2b36" : "#17212b")
          border.color: active ? "#4c8e88" : "#293642"

          Text {
            anchors.centerIn: parent
            text: modelData.label + "  " + modelData.count
            color: parent.active ? "#dff4ec" : "#8996a6"
            font.pixelSize: 11
            font.weight: parent.active ? Font.DemiBold : Font.Normal
          }

          MouseArea {
            id: sectionMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: page.selectedSection = index
          }
        }
      }

      Item { Layout.fillWidth: true }
    }

    StackLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      currentIndex: page.selectedSection

      Panel {
        padding: 0

        ListView {
          id: contractList
          anchors.fill: parent
          clip: true
          spacing: 8
          model: game.playerContracts
          boundsBehavior: Flickable.StopAtBounds
          ScrollBar.vertical: ScrollBar { }

          delegate: Rectangle {
            required property var modelData
            property var rowData: modelData

            width: contractList.width
            height: 132
            radius: 7
            color: "#17212b"
            border.color: rowData.daysRemaining <= 90 ? "#684449" : "#263542"

            ColumnLayout {
              anchors.fill: parent
              anchors.margins: 13
              spacing: 6

              RowLayout {
                Layout.fillWidth: true

                Text {
                  text: rowData.position
                  color: "#6fc29f"
                  font.pixelSize: 10
                  Layout.preferredWidth: 60
                }

                Text {
                  text: rowData.name
                  color: "#edf1f3"
                  font.pixelSize: 13
                  font.weight: Font.DemiBold
                  Layout.preferredWidth: 170
                }

                Text {
                  text: rowData.squadGroup + " · " + rowData.squadRole
                  color: "#8c9aaa"
                  font.pixelSize: 10
                  Layout.preferredWidth: 170
                }

                Text {
                  text: rowData.weeklyWage + " /주"
                  color: "#d8c27b"
                  font.pixelSize: 11
                  Layout.preferredWidth: 115
                }

                Text {
                  text: "만료 " + rowData.endDate + " · " + rowData.expiryRisk
                  color: rowData.daysRemaining <= 90 ? "#e58a84" : "#9aa8b6"
                  font.pixelSize: 10
                  Layout.fillWidth: true
                }

                Rectangle {
                  implicitWidth: 126
                  implicitHeight: 24
                  radius: 5
                  color: "#111922"
                  border.color: rowData.renewalStageColor

                  Text {
                    anchors.centerIn: parent
                    text: rowData.renewalStage
                    color: rowData.renewalStageColor
                    font.pixelSize: 9
                  }
                }
              }

              Text {
                Layout.fillWidth: true
                text: rowData.renewalNote.length > 0
                      ? rowData.renewalNote
                      : "현재 계약 · " + rowData.contractSummary
                color: "#7f8d9e"
                font.pixelSize: 10
                elide: Text.ElideRight
              }

              Text {
                visible: rowData.counterSummary.length > 0
                Layout.fillWidth: true
                text: "선수 측 역제안 · " + rowData.counterSummary
                color: "#d4b96e"
                font.pixelSize: 10
                elide: Text.ElideRight
              }

              RowLayout {
                Layout.fillWidth: true
                spacing: 7

                Item { Layout.fillWidth: true }

                Button {
                  visible: rowData.canOffer
                  text: rowData.renewalStageKey === "player_countered"
                        ? "조건 수정" : "재계약 제안"
                  onClicked: page.openRenewal(rowData)
                }

                Button {
                  visible: rowData.canAcceptCounter
                  text: "역제안 수락"
                  onClicked: game.acceptContractRenewalCounter(rowData.renewalId)
                }

                Button {
                  visible: rowData.canConfirm
                  text: "최종 서명"
                  onClicked: game.confirmContractRenewal(rowData.renewalId)
                }

                Button {
                  visible: rowData.canWithdraw
                  text: "협상 철회"
                  onClicked: game.withdrawContractRenewal(rowData.renewalId)
                }
              }
            }
          }
        }
      }

      Panel {
        padding: 0

        ListView {
          id: loanList
          anchors.fill: parent
          clip: true
          spacing: 8
          model: game.loanDeals
          boundsBehavior: Flickable.StopAtBounds
          ScrollBar.vertical: ScrollBar { }

          delegate: Rectangle {
            required property var modelData
            property var rowData: modelData

            width: loanList.width
            height: 142
            radius: 7
            color: "#17212b"
            border.color: "#293844"

            ColumnLayout {
              anchors.fill: parent
              anchors.margins: 13
              spacing: 6

              RowLayout {
                Layout.fillWidth: true

                Text {
                  text: rowData.direction
                  color: "#6fc29f"
                  font.pixelSize: 10
                  Layout.preferredWidth: 105
                }

                Text {
                  text: rowData.name
                  color: "#edf1f3"
                  font.pixelSize: 13
                  font.weight: Font.DemiBold
                  Layout.preferredWidth: 180
                }

                Text {
                  text: "상대 · " + rowData.counterparty
                  color: "#91a0ae"
                  font.pixelSize: 10
                  Layout.fillWidth: true
                }

                Text {
                  text: rowData.stage
                  color: rowData.stageColor
                  font.pixelSize: 10
                  font.weight: Font.DemiBold
                }
              }

              Text {
                Layout.fillWidth: true
                text: rowData.termsSummary
                color: "#c0c8cf"
                font.pixelSize: 10
                elide: Text.ElideRight
              }

              Text {
                Layout.fillWidth: true
                text: rowData.responseNote
                color: "#7f8d9e"
                font.pixelSize: 10
                elide: Text.ElideRight
              }

              Text {
                visible: rowData.stageKey === "active"
                         || rowData.stageKey === "recall_pending"
                         || rowData.stageKey === "completed"
                         || rowData.stageKey === "recalled"
                text: "출전 " + rowData.appearances + " · 선발 " + rowData.starts
                color: "#72b798"
                font.pixelSize: 10
              }

              RowLayout {
                Layout.fillWidth: true
                spacing: 7

                Item { Layout.fillWidth: true }

                Button {
                  visible: rowData.canReject
                  text: "거절"
                  onClicked: game.rejectLoanOffer(rowData.id)
                }

                Button {
                  visible: rowData.canCounter
                  text: "조건 재협상"
                  onClicked: page.openLoanCounter(rowData)
                }

                Button {
                  visible: rowData.canAcceptOffer
                  text: "임대 승인"
                  onClicked: game.acceptLoanOffer(rowData.id)
                }

                Button {
                  visible: rowData.canAcceptCounter
                  text: "역제안 수락"
                  onClicked: game.acceptLoanCounter(rowData.id)
                }

                Button {
                  visible: rowData.canRecall
                  text: "조기 복귀 요청"
                  onClicked: game.recallLoan(rowData.id)
                }
              }
            }
          }
        }
      }

      Panel {
        padding: 0

        ListView {
          id: incomingLoanList
          anchors.fill: parent
          clip: true
          spacing: 8
          model: page.availableLoanCandidates
          boundsBehavior: Flickable.StopAtBounds
          ScrollBar.vertical: ScrollBar { }

          delegate: Rectangle {
            required property var modelData
            property var rowData: modelData

            width: incomingLoanList.width
            height: 112
            radius: 7
            color: "#17212b"
            border.color: "#293844"

            ColumnLayout {
              anchors.fill: parent
              anchors.margins: 13
              spacing: 7

              RowLayout {
                Layout.fillWidth: true

                Text {
                  text: "#" + rowData.rank
                  color: "#6fc29f"
                  font.pixelSize: 10
                  Layout.preferredWidth: 38
                }

                Text {
                  text: rowData.name
                  color: "#edf1f3"
                  font.pixelSize: 13
                  font.weight: Font.DemiBold
                  Layout.preferredWidth: 180
                }

                Text {
                  text: rowData.position + " · " + rowData.club
                  color: "#91a0ae"
                  font.pixelSize: 10
                  Layout.preferredWidth: 210
                }

                Text {
                  text: "지식 " + rowData.knowledge + "/100"
                  color: "#d2b86d"
                  font.pixelSize: 10
                  Layout.fillWidth: true
                }

                Button {
                  text: "임대 제의"
                  onClicked: page.openIncomingLoan(rowData)
                }
              }

              Text {
                Layout.fillWidth: true
                text: rowData.roleFit + " · 예상 주급 " + rowData.expectedWage
                color: "#7f8d9e"
                font.pixelSize: 10
                elide: Text.ElideRight
              }
            }
          }
        }
      }
    }
  }

  Dialog {
    id: renewalDialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 620
    title: page.selectedContract.name + " 재계약 제안"
    standardButtons: Dialog.Ok | Dialog.Cancel

    onAccepted: {
      game.submitContractRenewal(
          page.selectedContract.playerId,
          Number(renewalWage.text),
          renewalYears.value,
          Number(renewalSigning.text),
          Number(renewalLoyalty.text),
          Number(renewalAppearance.text),
          Number(renewalGoal.text),
          Number(renewalAgent.text),
          Number(renewalRelease.text),
          renewalRole.editText)
    }

    contentItem: ColumnLayout {
      spacing: 10

      Text {
        Layout.fillWidth: true
        text: "금액 단위는 파운드입니다. 계약금·로열티 보너스·에이전트 비용은 가용 현금 한도에 포함됩니다."
        color: "#8996a4"
        font.pixelSize: 10
        wrapMode: Text.WordWrap
      }

      GridLayout {
        Layout.fillWidth: true
        columns: 4
        columnSpacing: 8
        rowSpacing: 8

        Label { text: "주급" }
        TextField {
          id: renewalWage
          validator: IntValidator { bottom: 1 }
          Layout.fillWidth: true
        }
        Label { text: "기간" }
        SpinBox { id: renewalYears; from: 1; to: 5; editable: true; Layout.fillWidth: true }

        Label { text: "계약금" }
        TextField {
          id: renewalSigning
          validator: IntValidator { bottom: 0 }
          Layout.fillWidth: true
        }
        Label { text: "로열티 보너스" }
        TextField {
          id: renewalLoyalty
          validator: IntValidator { bottom: 0 }
          Layout.fillWidth: true
        }

        Label { text: "출전 수당" }
        TextField {
          id: renewalAppearance
          validator: IntValidator { bottom: 0 }
          Layout.fillWidth: true
        }
        Label { text: "득점 수당" }
        TextField {
          id: renewalGoal
          validator: IntValidator { bottom: 0 }
          Layout.fillWidth: true
        }

        Label { text: "에이전트 비용" }
        TextField {
          id: renewalAgent
          validator: IntValidator { bottom: 0 }
          Layout.fillWidth: true
        }
        Label { text: "바이아웃" }
        TextField {
          id: renewalRelease
          validator: IntValidator { bottom: 0 }
          Layout.fillWidth: true
        }

        Label { text: "선수단 지위" }
        ComboBox {
          id: renewalRole
          editable: true
          model: ["Key Player", "First Team", "Rotation", "Squad Player", "Prospect", "Reserve"]
          Layout.columnSpan: 3
          Layout.fillWidth: true
        }
      }
    }
  }

  Dialog {
    id: loanDialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 620
    title: page.loanDialogMode === "incoming"
           ? page.selectedCandidate.name + " 임대 제의"
           : page.selectedLoan.name + " 임대 조건 재협상"
    standardButtons: Dialog.Ok | Dialog.Cancel

    onAccepted: {
      var futureKey = loanFutureType.currentIndex === 2 ? "mandatory"
                      : (loanFutureType.currentIndex === 1 ? "option" : "none")
      var futureAmount = futureKey === "none" ? 0 : Number(loanFutureFee.text)
      if (page.loanDialogMode === "incoming") {
        game.startIncomingLoan(
            page.selectedCandidate.playerId,
            Number(loanFee.text),
            loanWage.value,
            loanRole.editText,
            loanEnd.text,
            loanRecall.checked,
            loanRecallDays.value,
            futureKey,
            futureAmount)
      } else {
        game.submitLoanCounter(
            page.selectedLoan.id,
            Number(loanFee.text),
            loanWage.value,
            loanRole.editText,
            loanEnd.text,
            loanRecall.checked,
            loanRecallDays.value,
            futureKey,
            futureAmount)
      }
    }

    contentItem: ColumnLayout {
      spacing: 10

      Text {
        Layout.fillWidth: true
        text: "주급 부담률은 선수를 빌리는 구단이 부담하는 비율입니다. 종료일은 YYYY-MM-DD로 입력합니다."
        color: "#8996a4"
        font.pixelSize: 10
        wrapMode: Text.WordWrap
      }

      GridLayout {
        Layout.fillWidth: true
        columns: 4
        columnSpacing: 8
        rowSpacing: 8

        Label { text: "임대료" }
        TextField {
          id: loanFee
          validator: IntValidator { bottom: 0 }
          Layout.fillWidth: true
        }
        Label { text: "주급 부담 %" }
        SpinBox { id: loanWage; from: 0; to: 100; editable: true; Layout.fillWidth: true }

        Label { text: "약속 역할" }
        ComboBox {
          id: loanRole
          editable: true
          model: ["Regular Starter", "Squad Player", "Backup", "Development"]
          Layout.fillWidth: true
        }
        Label { text: "종료일" }
        TextField { id: loanEnd; placeholderText: "YYYY-MM-DD"; Layout.fillWidth: true }

        Label { text: "리콜" }
        CheckBox { id: loanRecall; text: "허용" }
        Label { text: "통지 일수" }
        SpinBox { id: loanRecallDays; from: 0; to: 30; editable: true; Layout.fillWidth: true }

        Label { text: "완전 영입" }
        ComboBox {
          id: loanFutureType
          model: ["없음", "선택", "의무"]
          Layout.fillWidth: true
        }
        Label { text: "완전 영입 금액" }
        TextField {
          id: loanFutureFee
          enabled: loanFutureType.currentIndex > 0
          validator: IntValidator { bottom: 0 }
          Layout.fillWidth: true
        }
      }
    }
  }
}

