# 이태현 | 게임 기획 · 프로그래밍 포트폴리오

어떤 선수를 영입해 어느 시설에서 키울지, 상대에게 보여 준 화투를 실제 승부에도 쓸지, 오늘 밤 탐색에 동료를 몇 명 보낼지. 아래에는 이런 결정을 플레이어에게 맡기기 위해 작성한 기획서와 실제 작업물을 모았습니다.

**[게임기획 포트폴리오 PDF · 29쪽](portfolio/game-design/LeeTaehyun_GameDesign_Portfolio.pdf)** · [편집용 PowerPoint · 본문 18장](portfolio/game-design/LeeTaehyun_GameDesign_Portfolio.pptx) · **[전체 프로젝트 및 작업물 46건](catalog/README.md)**

한국공학대학교 게임공학과 · 2027년 2월 졸업 예정 · mdk73000@naver.com

## 게임 기획 대표작

| 프로젝트 | 개인 기여와 작업물 |
|---|---|
| [Football Owner](projects/04-football-owner/README.md) | FM에서 좋아하던 선수단 운영을 구단주 게임으로 기획했습니다. [1~8차 기획서 전문과 개정 이력](projects/04-football-owner/design-history/README.md), [0.24 운영 규칙과 테스트](projects/04-football-owner/design-history/rules-v0.24/README.md)를 볼 수 있습니다. |
| [Dophamine Addiction](projects/01-dophamine-addiction/design/README.md) | 4인 졸업작품에서 TPS와 섯다의 결합을 제안하고 공개 카드와 최종 패를 따로 고르도록 판정을 수정했습니다. [실제 시연 영상](https://youtu.be/Gi9b-EheNaE) |
| [계약과 공명 RPG](projects/05-rpg-game-design/README.md) | 계약체에 따라 같은 적의 공격에 다르게 대응하는 파티 RPG입니다. [v0.11 기획서](projects/05-rpg-game-design/design/RPG_GDD_v0.11.pdf), 광산 전투 데모 화면, 재현 가능한 규칙 검사를 담았습니다. |
| [By the Cthul](projects/06-by-the-cthul/README.md) | 2021년 카드 전략 게임 제안서와 직접 그린 보드에서 시작했습니다. [보완한 종이판 규칙](projects/06-by-the-cthul/design/paper-rules.md)과 [9일간의 플레이 예시](projects/06-by-the-cthul/design/first-playthrough.md)를 원안과 함께 읽을 수 있습니다. |
| [Football Manager 2022 전술 실험](projects/07-fm22-tactics/README.md) | 컷백 전술을 고정하고 두 팀의 능력치 조건을 바꿔 영입 우선순위를 검토했습니다. 원본 발표의 실험 조건·결과·결론을 첨부했습니다. |
| [PC 던전앤파이터 무기고 역기획](projects/08-dnf-armory/README.md) | 계정에 등록된 품목을 부캐도 사용할 때 달라지는 파밍과 상위 던전 보상을 분석한 개인 기획 과제입니다. |

[종이 삼킨 새벽 · Teammate Chain · 이어지는 이야기](projects/09-indie-experiments/README.md)에서는 전투 규칙, 선수 연결 판정, 대화 분기와 기억 정책을 실제 작업에서 발췌했습니다. 수업 과제, 팀 기여와 도구 제작은 [분야별 전체 목록](catalog/README.md)에서 이어집니다.

## 프로그래밍 포트폴리오

[프로그래밍 PDF](portfolio/LeeTaehyun_GameProgrammer_Portfolio_2026.pdf) · [졸업작품의 네트워크·게임 판정](projects/01-dophamine-addiction/README.md) · [C++ IOCP 2D MMORPG](projects/02-iocp-2d-mmorpg/README.md) · [EverPlanet World Study](projects/03-everplanet-world-study/README.md)


## 핵심 역량과 코드 근거

| 역량 | 확인할 프로젝트 |
|---|---|
| C/C++ | Dophamine Addiction, IOCP MMORPG, EverPlanet의 Unreal C++ 기술 연구, Football Owner |
| Unreal Engine | Dophamine Addiction, EverPlanet의 이전 C++ 기술 연구 |
| Godot | EverPlanet의 현재 playable slice |
| 네트워크·멀티플레이 | Dophamine Addiction, IOCP MMORPG |
| 서버·백엔드 | IOCP lobby, Dedicated Server orchestration, ODBC/MS SQL, SQLite repository |
| 멀티스레드·병렬 프로그래밍 | IOCP worker, world thread, DB worker, atomic/mutex/SRWLOCK/condition_variable |
| 게임플레이·물리 | 서버 권위 판정, TPS/카드 phase, 구면 중력과 접선 이동 |
| 툴·파이프라인 | Unreal Python 콘텐츠 생성·검증, 자동화 테스트, Football Owner 데이터 검증 |
| 기획 | Football Manager 기반 Football Owner 재해석, 선택형 파티 RPG GDD |

## 5분 코드 리뷰 순서

1. `projects/01-dophamine-addiction` — IOCP lobby → Dedicated Server handoff → readiness/reconnect → 서버 권위 게임 판정
2. `projects/02-iocp-2d-mmorpg` — accept/recv/send → TCP framing → session lifetime → world/DB thread → 게임 로직
3. `projects/03-everplanet-world-study` — 현재 Godot slice → 이전 UE surface gravity/tangent camera 연구 → 자동화 테스트
4. `projects/04-football-owner` — Controller → simulation core/repository → SQLite transaction → QML → tests
5. [계약과 공명 RPG](projects/05-rpg-game-design/README.md) — v0.4·v0.11 문서, 규칙 검사 및 초기 데모 화면

## 저장소 원칙

- 원본 프로젝트는 수정하지 않았습니다. 이 저장소에는 공개 가능한 파일을 별도로 선별해 복사했습니다.
- 빌드 산출물, 실행 파일, 캐시, 대용량 패키지, 계정·접속정보, 외부 상용 에셋은 포함하지 않습니다.
- 팀 프로젝트와 수업 프로젝트는 담당 범위와 제외 범위를 각 README에 명시합니다.
- AI를 활용한 프로젝트는 활용 방식과 본인이 수행한 판단·검증 범위를 [`docs/AUTHORSHIP_AND_AI_USE.md`](docs/AUTHORSHIP_AND_AI_USE.md)에 공개합니다.
- 기존 코드 공개본의 파일별 출처와 SHA-256은 [`docs/SOURCE_MANIFEST.csv`](docs/SOURCE_MANIFEST.csv)에서 확인할 수 있습니다.
- 자동 검사 결과와 수동 판정은 [`docs/VERIFICATION_REPORT.md`](docs/VERIFICATION_REPORT.md), [`docs/REVIEWED_WARNINGS.md`](docs/REVIEWED_WARNINGS.md)에 기록합니다.
- 실제로 재수행한 빌드·테스트와 재현 범위는 [`docs/QA_SUMMARY.md`](docs/QA_SUMMARY.md)에 기록합니다.
- 이 저장소 전체에 일괄 적용되는 오픈소스 라이선스는 부여하지 않습니다. 자세한 내용은 [`NOTICE.md`](NOTICE.md)를 확인해 주세요.

## 실행과 검증에 관하여

프로젝트별 엔진·라이브러리·DB 의존성이 서로 달라 저장소 루트에서 한 번에 빌드하지 않습니다. 각 폴더의 README에 빌드 조건, 실행 방법, 확인된 검증 결과와 재현하지 못한 항목을 구분해 기록했습니다.

기획·프로그래밍 자료의 구성과 개별 링크는 [포트폴리오 안내](docs/SUBMISSION_GUIDE.md)에 정리했습니다.
