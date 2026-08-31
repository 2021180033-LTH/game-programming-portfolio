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

  TransferTermsDialog {
    id: transferTermsDialog

    onTermsSubmitted: function(actionKind, targetId, upfrontPounds,
                                installmentPounds, installmentCount,
                                appearanceBonusPounds, appearanceThreshold,
                                sellOnPercentage) {
      if (actionKind === "start") {
        game.startDetailedTransferPursuit(
              targetId, upfrontPounds, installmentPounds, installmentCount,
              appearanceBonusPounds, appearanceThreshold, sellOnPercentage)
      } else if (actionKind === "revise") {
        game.submitDetailedClubOffer(
              targetId, upfrontPounds, installmentPounds, installmentCount,
              appearanceBonusPounds, appearanceThreshold, sellOnPercentage)
      } else if (actionKind === "outgoing") {
        game.submitOutgoingTransferCounter(
              targetId, upfrontPounds, installmentPounds, installmentCount,
              appearanceBonusPounds, appearanceThreshold, sellOnPercentage)
      }
    }
  }

  ContractTermsDialog {
    id: contractTermsDialog

    onTermsSubmitted: function(pursuitId, weeklyWagePounds, contractYears,
                               signingBonusPounds, loyaltyBonusPounds,
                               appearanceFeePounds, goalBonusPounds,
                               agentFeePounds, releaseClausePounds, squadRole) {
      game.submitPlayerContractOffer(
            pursuitId, weeklyWagePounds, contractYears, signingBonusPounds,
            loyaltyBonusPounds, appearanceFeePounds, goalBonusPounds,
            agentFeePounds, releaseClausePounds, squadRole)
    }
  }

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
          text: "이적 시장"
          color: "#f1f4f6"
          font.pixelSize: 26
          font.weight: Font.DemiBold
        }

        Text {
          text: "즉시 지급·할부·보너스·재판매 조항과 선수 계약을 직접 협상하고 최종 승인합니다"
          color: "#7f8d9e"
          font.pixelSize: 13
        }
      }

      Rectangle {
        implicitWidth: marketBadge.implicitWidth + 24
        implicitHeight: 30
        radius: 15
        color: game.decisionCount > 0 ? "#49391f" : "#1d3c35"
        border.color: game.decisionCount > 0 ? "#8f6a35" : "#2e6657"

        Text {
          id: marketBadge
          anchors.centerIn: parent
          text: game.decisionCount + "건 결정 필요"
          color: game.decisionCount > 0 ? "#e6c277" : "#85cbb0"
          font.pixelSize: 11
          font.weight: Font.Medium
        }
      }
    }

    GridLayout {
      Layout.fillWidth: true
      columns: 4
      columnSpacing: 12

      StatCard {
        Layout.fillWidth: true
        label: "이적 가용 현금"
        value: game.availableTransferCash
        hint: "기존 지급·진행 약정 제외"
        accent: "#63a7d8"
      }

      StatCard {
        Layout.fillWidth: true
        label: "진행 중 협상"
        value: game.activeNegotiationCount + "건"
        hint: "구단·선수·합류 단계"
        accent: "#aa83d4"
      }

      StatCard {
        Layout.fillWidth: true
        label: "최종 승인"
        value: game.pendingTransferCount + "건"
        hint: "합의된 영입 조건"
        accent: "#c7a35a"
      }

      StatCard {
        Layout.fillWidth: true
        label: "판매 제안"
        value: game.pendingOutgoingOfferCount + "건"
        hint: "선수단 이탈 승인"
        accent: "#d87870"
      }
    }

    Rectangle {
      Layout.fillWidth: true
      Layout.preferredHeight: 58
      radius: 8
      color: "#242b25"
      border.color: "#5f6442"

      Text {
        anchors.fill: parent
        anchors.margins: 13
        text: "데이터 안내 · 선수 신원과 1992년 당시 소속은 역사 데이터로 관리하지만 요구액·주급·보너스와 협상 반응은 기능 검증용 추정치입니다. 제안의 최대 지출은 미정산·진행 중 약정을 뺀 보유 현금을 넘을 수 없으며, 승인 뒤 즉시 지급액과 미래 할부를 분리해 정산합니다."
        color: "#d4d3ac"
        font.pixelSize: 11
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
      }
    }

    Text {
      text: "최종 승인"
      color: "#e7ebee"
      font.pixelSize: 19
      font.weight: Font.DemiBold
    }

    Text {
      visible: game.transferProposals.length === 0
      text: "현재 최종 승인을 기다리는 영입 안건이 없습니다."
      color: "#748296"
      font.pixelSize: 12
    }

    Repeater {
      model: game.transferProposals

      Panel {
        required property var modelData
        property var rowData: modelData

        Layout.fillWidth: true
        Layout.preferredHeight: 390

        ColumnLayout {
          anchors.fill: parent
          spacing: 11

          RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
              Layout.fillWidth: true
              spacing: 3

              Text {
                text: rowData.name
                color: "#f2f5f7"
                font.pixelSize: 21
                font.weight: Font.DemiBold
              }

              Text {
                text: rowData.position + " · "
                      + (rowData.age === "미상" ? "나이 미상" : rowData.age + "세")
                      + " · " + rowData.fromClub
                color: "#8b98a8"
                font.pixelSize: 11
              }
            }

            Rectangle {
              implicitWidth: approvalStatus.implicitWidth + 20
              implicitHeight: 28
              radius: 14
              color: "#19232d"
              border.color: rowData.statusColor

              Text {
                id: approvalStatus
                anchors.centerIn: parent
                text: rowData.status
                color: rowData.statusColor
                font.pixelSize: 11
                font.weight: Font.DemiBold
              }
            }
          }

          GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 18
            rowSpacing: 7

            Text { text: "보장 이적료"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.fee; color: "#e5e9ec"; font.pixelSize: 13; font.weight: Font.DemiBold }
            Text { text: "최대 이적료"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.maximumFee; color: "#e0bd76"; font.pixelSize: 13; font.weight: Font.DemiBold }

            Text { text: "승인 즉시 현금"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.immediateCash; color: "#d78376"; font.pixelSize: 12 }
            Text { text: "최대 예산 약정"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.maximumCommitment; color: "#d6c08b"; font.pixelSize: 12 }

            Text { text: "합의 주급"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.weeklyWage; color: "#e5e9ec"; font.pixelSize: 13; font.weight: Font.DemiBold }
            Text { text: "계약 기간"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.contractYears + "년"; color: "#c7d0d9"; font.pixelSize: 12 }

            Text { text: "결정 기한"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.decisionDeadline; color: "#e1bb70"; font.pixelSize: 12 }
            Text { text: "메디컬·등록"; color: "#748296"; font.pixelSize: 10 }
            Text { text: "통과 · 확인"; color: "#6fc29f"; font.pixelSize: 12 }

            Text { text: "승인 후 가용 현금"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.cashAfter; color: "#8ec5e7"; font.pixelSize: 12 }
            Text { text: "승인 후 예산"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.budgetAfter; color: "#d6c08b"; font.pixelSize: 12 }
          }

          Text {
            Layout.fillWidth: true
            text: "구단 조건 · " + rowData.feeSummary
            color: "#91a9bc"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }

          Text {
            Layout.fillWidth: true
            text: "선수 계약 · " + rowData.contractSummary
                  + " · 출전 " + rowData.appearanceFee
                  + " · 득점 " + rowData.goalBonus
                  + " · 바이아웃 " + rowData.releaseClause
            color: "#a99aba"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }

          Text {
            Layout.fillWidth: true
            text: rowData.requestedBy + " · " + rowData.rationale
            color: "#9aa6b4"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
          }

          RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
              Layout.fillWidth: true
              text: rowData.pending
                    ? (rowData.affordable ? "현재 예산 안에서 승인 가능" : "현재 예산 조건을 충족하지 못함")
                    : (rowData.statusKey === "approved"
                       ? "합류 예정 · " + rowData.joinDate
                       : (rowData.statusKey === "completed"
                          ? "합류 완료 · " + rowData.joinDate
                          : "거래 종료"))
              color: rowData.pending
                     ? (rowData.affordable ? "#6fc29f" : "#d97770")
                     : "#7f8d9e"
              font.pixelSize: 11
            }

            Button {
              id: approvalReject
              visible: rowData.pending
              text: "거절"
              onClicked: game.rejectTransfer(rowData.id)
              background: Rectangle {
                color: approvalReject.hovered ? "#5e3435" : "#492a2c"
                radius: 7
                border.color: "#704044"
              }
              contentItem: Text {
                text: approvalReject.text
                color: "#f1c4c1"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }

            Button {
              id: approvalAccept
              visible: rowData.pending
              text: "영입 승인"
              onClicked: game.approveTransfer(rowData.id)
              background: Rectangle {
                color: approvalAccept.hovered ? "#2c7a65" : "#256854"
                radius: 7
                border.color: "#3a8b73"
              }
              contentItem: Text {
                text: approvalAccept.text
                color: "white"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }
          }
        }
      }
    }

    Text {
      text: "진행 중·완료 협상"
      color: "#e7ebee"
      font.pixelSize: 19
      font.weight: Font.DemiBold
    }

    Repeater {
      model: game.transferPursuits

      Panel {
        required property var modelData
        property var rowData: modelData

        Layout.fillWidth: true
        Layout.preferredHeight: 344

        ColumnLayout {
          anchors.fill: parent
          spacing: 10

          RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
              Layout.fillWidth: true
              spacing: 2

              Text {
                text: rowData.name
                color: "#f0f4f6"
                font.pixelSize: 18
                font.weight: Font.DemiBold
              }

              Text {
                text: rowData.club + " · 협상 " + rowData.round + "차"
                color: "#7e8c9d"
                font.pixelSize: 11
              }
            }

            Text {
              text: rowData.stage
              color: rowData.stageColor
              font.pixelSize: 11
              font.weight: Font.DemiBold
            }
          }

          GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 16

            Text { text: "현재 제안"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.currentOffer; color: "#dce4e9"; font.pixelSize: 12 }
            Text { text: "상대 역제안"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.clubCounter; color: "#e0bd76"; font.pixelSize: 12 }

            Text { text: "다음 처리"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.nextActionDate; color: "#8ec5e7"; font.pixelSize: 11 }
            Text { text: "협상 기한"; color: "#748296"; font.pixelSize: 10 }
            Text { text: rowData.decisionDeadline; color: "#d7a36d"; font.pixelSize: 11 }
          }

          Text {
            Layout.fillWidth: true
            text: "우리 제안 · " + rowData.offerSummary
            color: "#91a9bc"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }

          Text {
            visible: rowData.canAcceptCounter
            Layout.fillWidth: true
            text: "상대 구단 요구 · " + rowData.clubCounterSummary
            color: "#d4b778"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }

          Text {
            visible: rowData.canSubmitPlayerTerms
                     || rowData.stageKey === "awaiting_player_response"
                     || rowData.stageKey === "awaiting_medical"
                     || rowData.stageKey === "awaiting_registration"
            Layout.fillWidth: true
            text: "선수 계약 · "
                  + (rowData.canAcceptPlayerCounter
                     ? rowData.playerCounterSummary
                     : rowData.playerTermsSummary)
            color: "#aa97c3"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }

          Text {
            Layout.fillWidth: true
            text: rowData.responseNote
            color: "#9ba8b5"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
          }

          Text {
            visible: rowData.rivalActive && rowData.rivalClub.length > 0
            text: "경쟁 제안 · " + rowData.rivalClub + " " + rowData.rivalBid
            color: "#d78376"
            font.pixelSize: 10
          }

          RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            Button {
              id: withdrawButton
              visible: rowData.canWithdraw
              text: "협상 철회"
              onClicked: game.withdrawTransferPursuit(rowData.id)
              background: Rectangle {
                color: withdrawButton.hovered ? "#523239" : "#3d2930"
                radius: 7
                border.color: "#66404a"
              }
              contentItem: Text {
                text: withdrawButton.text
                color: "#e6bfc4"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }

            Button {
              id: improveButton
              visible: rowData.canImproveOffer
              text: "빠른 금액 개선"
              onClicked: game.improveClubOffer(rowData.id)
              background: Rectangle {
                color: improveButton.hovered ? "#3b5167" : "#2e4255"
                radius: 7
                border.color: "#47627b"
              }
              contentItem: Text {
                text: improveButton.text
                color: "#c9dcec"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }

            Button {
              id: detailedOfferButton
              visible: rowData.canImproveOffer
              text: "세부 조건 수정"
              onClicked: transferTermsDialog.openFor(
                  "revise", rowData.id, rowData.name,
                  rowData.counterUpfrontPounds,
                  rowData.counterInstallmentPounds,
                  rowData.counterInstallmentCount,
                  rowData.counterAppearanceBonusPounds,
                  rowData.counterAppearanceThreshold,
                  rowData.counterSellOnPercentage)
              background: Rectangle {
                color: detailedOfferButton.hovered ? "#4d4667" : "#3d3854"
                radius: 7
                border.color: "#5e557e"
              }
              contentItem: Text {
                text: detailedOfferButton.text
                color: "#ded6f1"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }

            Button {
              id: counterButton
              visible: rowData.canAcceptCounter
              text: "역제안 수락"
              onClicked: game.acceptClubCounter(rowData.id)
              background: Rectangle {
                color: counterButton.hovered ? "#2c7a65" : "#256854"
                radius: 7
                border.color: "#3a8b73"
              }
              contentItem: Text {
                text: counterButton.text
                color: "white"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }

            Button {
              id: contractOfferButton
              visible: rowData.canSubmitPlayerTerms
              text: rowData.canAcceptPlayerCounter ? "계약 재제안" : "계약 조건 제안"
              onClicked: contractTermsDialog.openFor(
                  rowData.id, rowData.name,
                  rowData.canAcceptPlayerCounter
                    ? rowData.counterWagePounds : rowData.playerWagePounds,
                  rowData.canAcceptPlayerCounter
                    ? rowData.counterContractYears : rowData.playerContractYears,
                  rowData.canAcceptPlayerCounter
                    ? rowData.counterSigningPounds : rowData.playerSigningPounds,
                  rowData.canAcceptPlayerCounter
                    ? rowData.counterLoyaltyPounds : rowData.playerLoyaltyPounds,
                  rowData.canAcceptPlayerCounter
                    ? rowData.counterAppearancePounds : rowData.playerAppearancePounds,
                  rowData.canAcceptPlayerCounter
                    ? rowData.counterGoalPounds : rowData.playerGoalPounds,
                  rowData.canAcceptPlayerCounter
                    ? rowData.counterAgentPounds : rowData.playerAgentPounds,
                  rowData.canAcceptPlayerCounter
                    ? rowData.counterReleasePounds : rowData.playerReleasePounds,
                  rowData.canAcceptPlayerCounter
                    ? rowData.counterRole : rowData.playerRole)
              background: Rectangle {
                color: contractOfferButton.hovered ? "#5d4d71" : "#493d59"
                radius: 7
                border.color: "#715d87"
              }
              contentItem: Text {
                text: contractOfferButton.text
                color: "#eadff4"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }

            Button {
              id: playerCounterButton
              visible: rowData.canAcceptPlayerCounter
              text: "선수 역제안 수락"
              onClicked: game.acceptPlayerCounter(rowData.id)
              background: Rectangle {
                color: playerCounterButton.hovered ? "#2c7a65" : "#256854"
                radius: 7
                border.color: "#3a8b73"
              }
              contentItem: Text {
                text: playerCounterButton.text
                color: "white"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }
          }
        }
      }
    }

    Text {
      text: "감독·스카우트 추천 후보"
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
        Layout.preferredHeight: 286

        ColumnLayout {
          anchors.fill: parent
          spacing: 10

          RowLayout {
            Layout.fillWidth: true

            Rectangle {
              implicitWidth: 34
              implicitHeight: 34
              radius: 17
              color: "#20364a"
              border.color: "#36546e"

              Text {
                anchors.centerIn: parent
                text: rowData.rank
                color: "#9fc4df"
                font.pixelSize: 13
                font.weight: Font.Bold
              }
            }

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
                text: rowData.position + " · " + rowData.age + "세 · "
                      + rowData.nationality + " · " + rowData.club
                color: "#8391a1"
                font.pixelSize: 10
              }
            }

            Text {
              text: rowData.status
              color: rowData.statusColor
              font.pixelSize: 10
              font.weight: Font.DemiBold
            }
          }

          Text {
            Layout.fillWidth: true
            text: rowData.roleFit
            color: "#78bea4"
            font.pixelSize: 12
            font.weight: Font.DemiBold
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
            Text { text: "첫 제안"; color: "#748296"; font.pixelSize: 9 }
            Text { text: rowData.openingOffer; color: "#8ec5e7"; font.pixelSize: 11 }
            Text { text: "예상 요구"; color: "#748296"; font.pixelSize: 9 }
            Text { text: rowData.askingPrice; color: "#dfb875"; font.pixelSize: 11 }
          }

          Text {
            Layout.fillWidth: true
            text: rowData.scoutSummary
            color: "#a1adb9"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
          }

          Text {
            Layout.fillWidth: true
            text: rowData.availability
            color: "#788696"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }

          RowLayout {
            Layout.fillWidth: true

            Text {
              Layout.fillWidth: true
              text: rowData.expectedWage + " 예상 주급 · "
                    + rowData.expectedYears + "년 · " + rowData.confidenceLabel
              color: "#7f8d9e"
              font.pixelSize: 10
            }

            Button {
              id: startButton
              visible: rowData.canStart
              text: rowData.retry ? "세부 조건으로 재협상" : "세부 제안 작성"
              onClicked: transferTermsDialog.openFor(
                  "start", rowData.playerId, rowData.name,
                  rowData.openingUpfrontPounds,
                  rowData.openingInstallmentPounds,
                  rowData.openingInstallmentCount,
                  rowData.openingAppearanceBonusPounds,
                  rowData.openingAppearanceThreshold,
                  rowData.openingSellOnPercentage)
              background: Rectangle {
                color: startButton.hovered ? "#2e607c" : "#274f67"
                radius: 7
                border.color: "#39738f"
              }
              contentItem: Text {
                text: startButton.text
                color: "#e7f2f8"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }
          }
        }
      }
    }

    Text {
      text: "받은 선수 판매 제안"
      color: "#e7ebee"
      font.pixelSize: 19
      font.weight: Font.DemiBold
    }

    Repeater {
      model: game.outgoingTransferOffers

      Panel {
        required property var modelData
        property var rowData: modelData

        Layout.fillWidth: true
        Layout.preferredHeight: 294

        ColumnLayout {
          anchors.fill: parent
          spacing: 10

          RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
              Layout.fillWidth: true
              spacing: 2

              Text {
                text: rowData.name + " → " + rowData.buyerClub
                color: "#f0f4f6"
                font.pixelSize: 18
                font.weight: Font.DemiBold
              }

              Text {
                text: "보장 " + rowData.fee + " · 최대 " + rowData.maximumFee
                      + " · 주급 절감 "
                      + rowData.wageRelief + " · 결정 기한 " + rowData.decisionDeadline
                color: "#8a98a8"
                font.pixelSize: 11
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
            text: "구매 구단 조건 · " + rowData.feeSummary
            color: "#91a9bc"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }

          Text {
            visible: rowData.canAcceptBuyerCounter
            Layout.fillWidth: true
            text: "구매 구단 역제안 · " + rowData.buyerCounterSummary
            color: "#d4b778"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }

          Text {
            Layout.fillWidth: true
            text: rowData.rationale
            color: "#9ba8b5"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
          }

          Text {
            text: rowData.pending
                  ? rowData.negotiationStage + " · 즉시 수령액은 완료일 현금에, 보장액은 이적 예산에 반영됩니다."
                  : (rowData.statusKey === "accepted"
                     ? "이적 완료 예정 · " + rowData.completionDate
                     : (rowData.statusKey === "completed"
                        ? "이적 완료 · " + rowData.completionDate
                        : "선수단과 재정 변화 없음"))
            color: rowData.pending ? "#d0b777" : "#7f8d9e"
            font.pixelSize: 10
          }

          RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            Button {
              id: saleReject
              visible: rowData.pending
              text: "판매 거절"
              onClicked: game.rejectOutgoingTransfer(rowData.id)
              background: Rectangle {
                color: saleReject.hovered ? "#5e3435" : "#492a2c"
                radius: 7
                border.color: "#704044"
              }
              contentItem: Text {
                text: saleReject.text
                color: "#f1c4c1"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }

            Button {
              id: saleCounterEdit
              visible: rowData.canCounter
              text: rowData.canAcceptBuyerCounter ? "다시 역제안" : "조건 재협상"
              onClicked: transferTermsDialog.openFor(
                  "outgoing", rowData.id, rowData.name + " 판매",
                  rowData.canAcceptBuyerCounter
                    ? rowData.buyerCounterUpfrontPounds : rowData.upfrontPounds,
                  rowData.canAcceptBuyerCounter
                    ? rowData.buyerCounterInstallmentPounds : rowData.installmentPounds,
                  rowData.canAcceptBuyerCounter
                    ? rowData.buyerCounterInstallmentCount : rowData.installmentCount,
                  rowData.canAcceptBuyerCounter
                    ? rowData.buyerCounterAppearanceBonusPounds : rowData.appearanceBonusPounds,
                  rowData.canAcceptBuyerCounter
                    ? rowData.buyerCounterAppearanceThreshold : rowData.appearanceThreshold,
                  rowData.canAcceptBuyerCounter
                    ? rowData.buyerCounterSellOnPercentage : rowData.sellOnPercentage)
              background: Rectangle {
                color: saleCounterEdit.hovered ? "#4d4667" : "#3d3854"
                radius: 7
                border.color: "#5e557e"
              }
              contentItem: Text {
                text: saleCounterEdit.text
                color: "#ded6f1"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }

            Button {
              id: saleCounterAccept
              visible: rowData.canAcceptBuyerCounter
              text: "역제안 수락"
              onClicked: game.acceptOutgoingTransferCounter(rowData.id)
              background: Rectangle {
                color: saleCounterAccept.hovered ? "#3b5167" : "#2e4255"
                radius: 7
                border.color: "#47627b"
              }
              contentItem: Text {
                text: saleCounterAccept.text
                color: "#c9dcec"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }

            Button {
              id: saleAccept
              visible: rowData.canDecide
              text: "판매 승인"
              onClicked: game.acceptOutgoingTransfer(rowData.id)
              background: Rectangle {
                color: saleAccept.hovered ? "#2c7a65" : "#256854"
                radius: 7
                border.color: "#3a8b73"
              }
              contentItem: Text {
                text: saleAccept.text
                color: "white"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
            }
          }
        }
      }
    }

    Text {
      text: "AI 이적시장"
      color: "#e7ebee"
      font.pixelSize: 19
      font.weight: Font.DemiBold
    }

    Text {
      visible: game.aiMarketDeals.length === 0
      text: "아직 AI 구단 간 독립 거래가 없습니다. 다음 이벤트 진행 시 각 구단이 예산과 주급 한도를 검사해 협상을 시작합니다."
      color: "#748296"
      font.pixelSize: 12
      wrapMode: Text.WordWrap
      Layout.fillWidth: true
    }

    Repeater {
      model: game.aiMarketDeals

      Panel {
        required property var modelData
        property var rowData: modelData

        Layout.fillWidth: true
        Layout.preferredHeight: 152

        ColumnLayout {
          anchors.fill: parent
          spacing: 9

          RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
              Layout.fillWidth: true
              spacing: 2

              Text {
                text: rowData.player + " · " + rowData.type
                color: "#f0f4f6"
                font.pixelSize: 16
                font.weight: Font.DemiBold
              }

              Text {
                text: rowData.route + " · 제안 " + rowData.proposedDate
                color: "#8a98a8"
                font.pixelSize: 11
              }
            }

            Rectangle {
              implicitWidth: 96
              implicitHeight: 26
              radius: 13
              color: "#202a34"
              border.color: rowData.stageColor

              Text {
                anchors.centerIn: parent
                text: rowData.stage
                color: rowData.stageColor
                font.pixelSize: 10
                font.weight: Font.DemiBold
              }
            }
          }

          Text {
            Layout.fillWidth: true
            text: "보장 " + rowData.guaranteedFee + " · 최대/예약 " + rowData.maximumFee
                  + " · 다음 처리 " + rowData.nextActionDate
                  + " · 완료 " + rowData.completionDate
            color: "#91a9bc"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }

          Text {
            Layout.fillWidth: true
            text: rowData.feeSummary
            color: "#a8b7c3"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }

          Text {
            Layout.fillWidth: true
            text: rowData.rationale + " · " + (rowData.completionApplied
                  ? "완료일 반영 완료"
                  : "완료 전까지 재정·선수단 미반영")
            color: rowData.completionApplied ? "#76c49c" : "#d0b777"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
          }
        }
      }
    }

    Text {
      text: "이적료 지급·수령 일정"
      color: "#e7ebee"
      font.pixelSize: 19
      font.weight: Font.DemiBold
    }

    Text {
      visible: game.transferPayments.length === 0
      text: "아직 확정된 이적료 지급 또는 수령 일정이 없습니다."
      color: "#748296"
      font.pixelSize: 12
    }

    Repeater {
      model: game.transferPayments

      Panel {
        required property var modelData
        property var rowData: modelData

        Layout.fillWidth: true
        Layout.preferredHeight: 94

        RowLayout {
          anchors.fill: parent
          spacing: 16

          Rectangle {
            implicitWidth: 48
            implicitHeight: 28
            radius: 14
            color: "#202a34"
            border.color: rowData.directionColor

            Text {
              anchors.centerIn: parent
              text: rowData.direction
              color: rowData.directionColor
              font.pixelSize: 10
              font.weight: Font.DemiBold
            }
          }

          ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Text {
              text: rowData.counterparty + " · " + rowData.category
              color: "#e1e7eb"
              font.pixelSize: 13
              font.weight: Font.DemiBold
            }

            Text {
              text: rowData.description
              color: "#8492a2"
              font.pixelSize: 10
            }
          }

          Text {
            text: rowData.dueDate
            color: "#9ba8b5"
            font.pixelSize: 11
          }

          Text {
            text: rowData.amount
            color: rowData.directionColor
            font.pixelSize: 13
            font.weight: Font.DemiBold
          }

          Text {
            text: rowData.status
            color: rowData.statusColor
            font.pixelSize: 10
            font.weight: Font.DemiBold
          }
        }
      }
    }
  }
}
