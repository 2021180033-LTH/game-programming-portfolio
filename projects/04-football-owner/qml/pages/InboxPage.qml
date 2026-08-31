import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
  id: page

  function approvalLabel(decisionType) {
    if (decisionType === "clubCounter")
      return "역제안 수락"
    if (decisionType === "playerCounter")
      return "선수 역제안 수락"
    if (decisionType === "loanCounter")
      return "임대 역제안 수락"
    if (decisionType === "loanOffer")
      return "임대 승인"
    if (decisionType === "renewalCounter")
      return "재계약 역제안 수락"
    if (decisionType === "renewalFinal")
      return "재계약 최종 서명"
    if (decisionType === "outgoingCounter")
      return "판매 역제안 수락"
    if (decisionType === "outgoingOffer")
      return "판매 승인"
    return "영입 승인"
  }

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
          text: "받은 편지함"
          color: "#f1f4f6"
          font.pixelSize: 26
          font.weight: Font.DemiBold
        }

        Text {
          text: "보고, 승인 요청, 경기 결과가 하나의 시간순 큐로 도착합니다"
          color: "#7f8d9e"
          font.pixelSize: 13
        }
      }

      Button {
        id: markAllReadButton
        text: "모두 읽음"
        enabled: game.unreadCount > 0
        onClicked: game.markAllInboxRead()
        background: Rectangle {
          color: markAllReadButton.enabled ? (markAllReadButton.hovered ? "#263746" : "#1d2b37") : "#171e26"
          radius: 7
          border.color: markAllReadButton.enabled ? "#34495b" : "#222a33"
        }
        contentItem: Text {
          text: markAllReadButton.text
          color: markAllReadButton.enabled ? "#cdd5de" : "#626d79"
          font.pixelSize: 12
          horizontalAlignment: Text.AlignHCenter
          verticalAlignment: Text.AlignVCenter
        }
      }
    }

    RowLayout {
      Layout.fillWidth: true
      spacing: 10

      Rectangle {
        implicitWidth: unreadText.implicitWidth + 22
        implicitHeight: 28
        radius: 14
        color: "#243643"

        Text {
          id: unreadText
          anchors.centerIn: parent
          text: "읽지 않음 " + game.unreadCount
          color: "#91b6cb"
          font.pixelSize: 11
        }
      }

      Rectangle {
        implicitWidth: decisionText.implicitWidth + 22
        implicitHeight: 28
        radius: 14
        color: "#49391f"

        Text {
          id: decisionText
          anchors.centerIn: parent
          text: "결정 필요 " + game.decisionCount
          color: "#e6c277"
          font.pixelSize: 11
        }
      }

      Item { Layout.fillWidth: true }
    }

    Panel {
      Layout.fillWidth: true
      Layout.fillHeight: true
      padding: 0

      ListView {
        id: inboxList
        anchors.fill: parent
        anchors.margins: 1
        clip: true
        spacing: 1
        model: game.inbox
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }

        delegate: Rectangle {
          required property var modelData
          required property int index
          property var rowData: modelData

          width: inboxList.width
          height: rowData.decisionType.length > 0 ? 154 : 112
          color: rowData.unread ? "#1b2a36" : (mouse.containsMouse ? "#192530" : "#151e28")

          Rectangle {
            width: 4
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            color: rowData.unread ? rowData.categoryColor : "transparent"
          }

          ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            anchors.topMargin: 13
            anchors.bottomMargin: 12
            spacing: 6

            RowLayout {
              Layout.fillWidth: true

              Rectangle {
                implicitWidth: categoryText.implicitWidth + 14
                implicitHeight: 22
                radius: 4
                color: Qt.rgba(0.2, 0.25, 0.3, 0.7)
                border.color: rowData.categoryColor

                Text {
                  id: categoryText
                  anchors.centerIn: parent
                  text: rowData.category
                  color: rowData.categoryColor
                  font.pixelSize: 10
                  font.weight: Font.Medium
                }
              }

              Text {
                text: rowData.requiresDecision ? "결정 필요" : ""
                visible: rowData.requiresDecision
                color: "#e4b968"
                font.pixelSize: 10
                font.weight: Font.DemiBold
              }

              Item { Layout.fillWidth: true }

              Text {
                text: rowData.date
                color: "#6f7d8e"
                font.pixelSize: 10
              }
            }

            Text {
              Layout.fillWidth: true
              text: rowData.title
              color: rowData.unread ? "#f2f5f7" : "#adb7c2"
              font.pixelSize: 14
              font.weight: rowData.unread ? Font.DemiBold : Font.Normal
              elide: Text.ElideRight
            }

            Text {
              Layout.fillWidth: true
              text: rowData.body
              color: "#8491a1"
              font.pixelSize: 11
              elide: Text.ElideRight
            }

            RowLayout {
              visible: rowData.decisionType.length > 0
              Layout.fillWidth: true
              Layout.preferredHeight: visible ? 30 : 0
              spacing: 8

              Text {
                Layout.fillWidth: true
                text: rowData.decisionStatus
                color: rowData.decisionStatusColor
                font.pixelSize: 10
                font.weight: Font.DemiBold
              }

              Button {
                id: inboxRejectButton
                visible: rowData.requiresDecision
                         && rowData.decisionType !== "transferDetails"
                text: rowData.decisionType === "clubCounter"
                      || rowData.decisionType === "playerCounter"
                      || rowData.decisionType === "renewalCounter"
                      || rowData.decisionType === "renewalFinal"
                      ? "협상 철회"
                      : "거절"
                onClicked: {
                  if (rowData.decisionType === "transferApproval")
                    game.rejectTransfer(rowData.transferProposalId)
                  else if (rowData.decisionType === "clubCounter")
                    game.withdrawTransferPursuit(rowData.transferPursuitId)
                  else if (rowData.decisionType === "playerCounter")
                    game.withdrawTransferPursuit(rowData.transferPursuitId)
                  else if (rowData.decisionType === "outgoingOffer")
                    game.rejectOutgoingTransfer(rowData.outgoingOfferId)
                  else if (rowData.decisionType === "outgoingCounter")
                    game.rejectOutgoingTransfer(rowData.outgoingOfferId)
                  else if (rowData.decisionType === "loanOffer")
                    game.rejectLoanOffer(rowData.loanId)
                  else if (rowData.decisionType === "loanCounter")
                    game.rejectLoanOffer(rowData.loanId)
                  else if (rowData.decisionType === "renewalCounter")
                    game.withdrawContractRenewal(rowData.renewalId)
                  else if (rowData.decisionType === "renewalFinal")
                    game.withdrawContractRenewal(rowData.renewalId)
                }
                background: Rectangle {
                  color: inboxRejectButton.hovered ? "#5d3335" : "#47292b"
                  radius: 6
                  border.color: "#704044"
                }
                contentItem: Text {
                  text: inboxRejectButton.text
                  color: "#efc2bf"
                  font.pixelSize: 11
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                }
              }

              Button {
                id: inboxApproveButton
                visible: rowData.requiresDecision
                         && rowData.decisionType !== "transferDetails"
                text: page.approvalLabel(rowData.decisionType)
                onClicked: {
                  if (rowData.decisionType === "transferApproval")
                    game.approveTransfer(rowData.transferProposalId)
                  else if (rowData.decisionType === "clubCounter")
                    game.acceptClubCounter(rowData.transferPursuitId)
                  else if (rowData.decisionType === "playerCounter")
                    game.acceptPlayerCounter(rowData.transferPursuitId)
                  else if (rowData.decisionType === "outgoingOffer")
                    game.acceptOutgoingTransfer(rowData.outgoingOfferId)
                  else if (rowData.decisionType === "outgoingCounter")
                    game.acceptOutgoingTransferCounter(rowData.outgoingOfferId)
                  else if (rowData.decisionType === "loanOffer")
                    game.acceptLoanOffer(rowData.loanId)
                  else if (rowData.decisionType === "loanCounter")
                    game.acceptLoanCounter(rowData.loanId)
                  else if (rowData.decisionType === "renewalCounter")
                    game.acceptContractRenewalCounter(rowData.renewalId)
                  else if (rowData.decisionType === "renewalFinal")
                    game.confirmContractRenewal(rowData.renewalId)
                }
                background: Rectangle {
                  color: inboxApproveButton.hovered ? "#2c7a65" : "#256854"
                  radius: 6
                  border.color: "#3a8b73"
                }
                contentItem: Text {
                  text: inboxApproveButton.text
                  color: "white"
                  font.pixelSize: 11
                  font.weight: Font.DemiBold
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                }
              }
            }
          }

          MouseArea {
            id: mouse
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: rowData.decisionType.length > 0 ? 112 : parent.height
            hoverEnabled: true
            onClicked: game.markInboxRead(rowData.id)
          }
        }
      }
    }
  }
}
