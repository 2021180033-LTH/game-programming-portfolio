# 역사 데이터 출처와 신뢰도

## 기준점

| ID | 출처 | 사용 범위 |
| --- | --- | --- |
| `pl-inaugural-history` | <https://www.premierleague.com/en/news/59001> | 창립 시즌 22개 구단 |
| `manutd-1992-opening-match` | <https://www.manutd.com/en/mutv/videos/detail/sheffield-united-2-man-utd-1-extended-highlights-premier-league-1992-93> | 1992-08-15 개막 일정과 상대 |
| `pl-manutd-1992-squad` | <https://www.premierleague.com/en/news/2713105> | 1992-93 리그 출전 선수 일부 |
| `mufcinfo-1991-92-player-records` | <https://www.mufcinfo.com/manupag/season/1991-1992> | 직전 시즌 1군 기록과 누락자 복구 |
| `manutd-class-92-trainees` | <https://www.manutd.com/en/news/30-years-since-class-of-92-players-beckham-butt-neville-scholes-signed-as-man-utd-trainees> | 1991-07-08 훈련생 집단 |
| `manutd-1992-youth-cup-squad` | <https://www.manutd.com/en/news/thirty-years-since-1992-fa-youth-cup-final-where-are-they-now> | 1992 FA Youth Cup 유스 집단 |
| `mufcinfo-1990s-transfers` | <https://www.mufcinfo.com/manupag/transfers/transfers_pages/1990-1999_manchester_united_transfers.htm> | 1992년 합류·퇴단 시점 교차 확인 |
| `pl-1992-93-season-review` | <https://www.premierleague.com/en/news/58731> | Shearer의 Southampton→Blackburn 이적과 £3.6m 기록 |
| `manutd-cantona-story` | <https://www.manutd.com/en/news/the-story-of-eric-cantona-at-manchester-united-26-november-2022> | Cantona의 1992년 Leeds→Manchester United 이동 |
| `manutd-1992-striker-hunt` | <https://www.manutd.com/en/news/sir-alex-on-his-united-striker-hunt-in-1992-utd-podcast> | Dublin과 1992년 공격수 탐색 맥락 |
| `spurs-sheringham-profile` | <https://www.tottenhamhotspur.com/news/1043662/the-big-interview-teddy-sheringham> | Sheringham의 1992년 Tottenham 합류 |
| `manutd-ferguson-kershaw` | <https://www.manutd.com/en/news/archived-magazine-interview-with-sir-alex-ferguson-from-march-2009-featuring-famous-fans-questions> | Ferguson과 Les Kershaw의 Manchester United 영입·스카우팅 역할 맥락 |
| `rsc-les-kershaw` | <https://www.rsc.org/news/2005/september/honour-for-scientist-who-became-manchester-united-s-chief-scout> | Les Kershaw의 Manchester United 수석 스카우트 경력 |
| `pl-transfer-window-history-2025` | <https://www.premierleague.com/en/news/4212638> | 현대식 이적시장 창 도입 시점, 이전 3월 말 마감과 1995년 보스만 판결 |

이 출처는 SQLite의 `source_records`와 `data_assertions`에도 저장합니다. 선수 신원, 기준일 소속, 그룹, 재정 조건을 하나의 신뢰도로 뭉치지 않고 분리합니다.

## 0.2.0 명단 판정

- 1군은 1991-92 1군 기록 23명을 시작점으로 삼고, 7월 1일 이후 이적자는 시작 명단에 유지합니다.
- Mal Donaghy와 Mark Robins는 각각 8월 13일과 14일 퇴단이므로 7월 1일 명단에 포함합니다.
- Dion Dublin과 Eric Cantona는 7월 1일 이후 합류하므로 시작 명단에서 제외합니다.
- Class of '92 훈련생과 1992 Youth Cup 집단은 유스 화면에 포함합니다.
- 리저브·유스의 정확한 그룹 경계와 전체성은 아직 잠정입니다.
- 확인하지 못한 Colin Telford의 생년월일은 빈 값으로 보존하고 화면에서 `미상`으로 표시합니다.

## 신뢰도 태그

| 태그 | 의미 |
| --- | --- |
| `official_confirmed` | 해당 필드를 공식 1차 출처에서 직접 확인 |
| `official_partial` | 공식 출처가 일부 범위만 뒷받침 |
| `confirmed_1991_92_first_team_membership` | 직전 시즌 1군 기록과 시작일 소속을 대조 |
| `membership_confirmed_group_provisional` | 당시 소속은 확인했으나 정확한 그룹은 잠정 |
| `official_1992_youth_cohort` | 공식 회고에서 유스 집단을 확인 |
| `academy_membership_provisional` | 아카데미 소속은 유력하나 시작일 그룹을 추가 검수해야 함 |
| `prototype_estimate` | 기능 검증용 수치이며 역사적 사실로 사용 금지 |

## 영입 안건

Alan Shearer의 신원과 1992년 7월 1일 Southampton 소속은 역사 데이터로 관리합니다. 그러나 이 버전의 £3.00m 이적료, £10.0k 주급, 4년 계약, 3일 뒤 합류는 승인·저장 흐름을 검증하기 위한 게임 수치입니다.

## 0.4.0 이적시장 후보

1차 후보는 Alan Shearer, Teddy Sheringham, Dion Dublin, Eric Cantona,
Les Ferdinand, Roy Keane, Paul Warhurst와 Brian Deane입니다.

- 선수 신원, 생년월일, 포지션과 1992-07-01 소속은 역사 데이터로 관리합니다.
- 실제 1992년 이동이 공식 출처로 확인되는 선수는 해당 사실을
  `data_assertions`에 별도로 기록합니다.
- 미래의 실제 이적은 게임 안에서 강제하지 않습니다.
- 현재 능력·잠재력 범위, 감독 우선순위, 선수 관심과 역할 적합성은 게임용 초기값입니다.
- 첫 제안, 판매 하한, 요구액, 예상 주급과 계약 기간은 출처가 확보되기 전까지
  역사적 사실이 아닌 `gameplay_estimate`입니다.
- Shearer의 £3.6m Blackburn 이적료는 경쟁 시장 맥락으로만 사용하며,
  Manchester United와의 가상 협상 조건을 사실로 표시하지 않습니다.

Mark Robins의 실제 1992년 Norwich 이동은 방향성 참고 자료로 사용하지만,
0.4.0 시작일의 £800k 제안, £3k 주급 약정, 계약 시작·종료일, 결정 기한과
3일 완료는 판매·주급 해제 시스템 검증용 시나리오입니다.

## 0.5.0 상세 조건

이번 버전은 새로운 역사적 선수 사실을 추가하지 않습니다. 다음 값은 모두 상세
협상과 재정 정산을 검증하기 위한 `gameplay_estimate`입니다.

- 후보별 즉시 지급액, 할부, 출전 보너스와 다음 이적 수익 비율
- 계약금, 로열티 보너스, 출전·득점 수당, 에이전트 수수료와 바이아웃
- 상대 구단과 선수 측의 조건 평가·역제안 기준
- 메디컬 위험 확률과 등록 판정용 시나리오 값
- 6개월 할부 간격과 12개월 로열티 보너스 지급 시점
- 판매 제안의 구매 구단 최대 조건

1992년 당시 실제 계약 원문이나 에이전트 수수료 장부로 표시하지 않으며, 화면과
문서에서도 추정치임을 유지합니다. 실제 선수 소속과 이적 방향에 대한 출처는
0.4.0 항목을 그대로 사용합니다.

## 0.6.0 스카우팅과 영입 계획

Les Kershaw가 Manchester United의 스카우팅·영입 조직에서 활동한 사실은 공식
구단 회고와 Royal Society of Chemistry 자료로 확인합니다. 다만 1992년 7월 1일
당시의 정확한 직함, 판단 능력치, 담당 지역, 동시 배정 한도는 공개된 원장 데이터가
아니므로 각각 `provisional` 또는 `gameplay_estimate`로 구분합니다.

- Les Kershaw와 Alex Ferguson의 스카우팅 판단 수치·배정 용량은 게임용 추정치
- 후보별 초기 지식 수준과 보고서 오차 범위는 게임용 추정치
- 3·7·14일 관찰 기간, 지식 상승량, 30일 최신성은 게임 규칙
- 프리시즌 목표 3명·최소 기대 2명과 신뢰 변화 폭은 게임 규칙

역사적 인물 관계와 게임 밸런스 수치를 같은 사실로 표시하지 않습니다.

## 0.7.0 임대·재계약과 시대 규정

Premier League의 이적시장 역사 설명은 현대식 두 차례 창 제도가 2002-03부터
도입됐고, 그전에는 시즌 중 3월 말까지 선수를 영입할 수 있었다고 설명합니다.
같은 자료는 계약 만료 선수가 이적료 없이 이동할 수 있게 된 기준점으로 1995년
보스만 판결을 듭니다. 따라서 1992-93 규칙 프로필은 다음처럼 설정합니다.

- 현대식 여름·겨울 이적시장 창 미적용
- 잉글랜드 국내 등록 마감 3월 31일
- 계약 만료 후 즉시 자유계약 이탈 미적용

1992-93 국내 임대의 정확한 인원 한도, 기간 분류, 리콜·치료·원구단 상대 출전
세칙을 모두 입증하는 1차 규정집은 아직 확보하지 못했습니다. 다음 값은 구조와
게임 흐름을 검증하기 위한 `provisional` 또는 `gameplay_estimate`입니다.

- Cambridge United의 Kieran Toal 임대 제안 자체와 모든 금액·기간·역할
- 임대료, 주급 부담률, 리콜 통지, 미출전 보상과 치료 책임
- 후보 8명의 임대 가능 여부와 상대 구단의 수락·역제안 기준
- Bryan Robson·Mark Hughes·Lee Martin·Kieran Toal의 계약 시작·종료일과 주급
- 재계약 권고 조건, 선수 관심·에이전트 인내와 협상 반응
- 재계약 로열티 보너스의 12개월 지급 시점

Manchester United 공식 자료는 Class of '92의 1991년 trainee 집단을 확인하는
용도로만 사용합니다. 개인별 프로 계약일·주급·보너스를 그 자료에서 확인한 것으로
표시하지 않습니다.

## 남은 데이터 작업

1. 리저브와 유스의 1992-07-01 전체 등록 명단 확정
2. 후보 8명과 맨유 선수별 정확한 계약 시작·종료일·임대 가능 조건
3. 구단 재무와 당시 임금·이적료의 출처
4. Premier League 21개 상대 구단과 하부리그의 전체 로스터
5. 전체 리그 일정과 대회 규칙
6. 필드 단위 출처·추정 방식·검수 이력
