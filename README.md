# 이태현 | Game Programmer Portfolio

Unreal Engine 기반 멀티플레이 게임과 C++ IOCP 서버를 중심으로 정리한 코드 포트폴리오입니다.  
각 프로젝트는 독립적으로 개발된 작업이며, 이 저장소는 실행 파일과 외부 에셋을 모은 배포본이 아니라 **직접 담당한 코드와 설계·검증 근거를 빠르게 검토할 수 있도록 구성한 제출용 저장소**입니다.

- 한국공학대학교 게임공학과
- 2027년 2월 졸업 예정
- 지원 분야: 게임 프로그래밍 · 서버/멀티플레이 · 게임플레이 · 기획
- 연락처: mdk73000@naver.com
- 포트폴리오 PDF: [`portfolio/LeeTaehyun_GameProgrammer_Portfolio_2026.pdf`](portfolio/LeeTaehyun_GameProgrammer_Portfolio_2026.pdf)
- 졸업작품 소개 영상: [Dophamine Addiction](https://youtu.be/Gi9b-EheNaE)

## 프로젝트 한눈에 보기

| 프로젝트 | 성격 / 인원 | 역할 | 핵심 기술 | 상태 |
|---|---|---|---|---|
| [Dophamine Addiction](projects/01-dophamine-addiction/README.md) | 졸업작품 / 4인 | 서버 파트 전담, 서버 권위 게임 판정, 충돌·획득/배치 판정 일부 — 공동 통합 파일은 기여표로 구분 | Unreal Engine, C++, Dedicated Server, WinSock2/IOCP, TCP | 4대 PC 실제 플레이 검증 |
| [C++ IOCP 2D MMORPG](projects/02-iocp-2d-mmorpg/README.md) | 학기 개인 프로젝트 / 1인 | 네트워크·동시성·DB·게임 로직·부하 시험 | C++, IOCP, SFML, ODBC/MS SQL, Lua | 구현 및 자체 시험 완료 |
| [EverPlanet World Study](projects/03-everplanet-world-study/README.md) | 개인 기술 연구 / 1인 | UE 구면 기술 연구 → Godot playable slice·저장·지역·퀘스트 | Godot/GDScript, Unreal C++, Python, Automation Test | 진행 중 |
| [Football Owner](projects/04-football-owner/README.md) | 개인 기획·구현 / 1인 | 시뮬레이션 기획, 요구사항·검증, 구현 통합 | C++20, Qt 6/QML, SQLite, CMake | 개발 중 |
| [계약과 공명 RPG](projects/05-rpg-game-design/README.md) | 개인 기획 / 1인 | GDD, 전투·성장·선택 구조, 기술 구조 설계 | C++20·SFML·Lua·CMake 목표 | 기획 단계 |

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
5. `projects/05-rpg-game-design` — 구현물이 아니라 기획 의도와 시스템 구조 확인

## 저장소 원칙

- 원본 프로젝트는 수정하지 않았습니다. 이 저장소에는 공개 가능한 파일을 별도로 선별해 복사했습니다.
- 빌드 산출물, 실행 파일, 캐시, 대용량 패키지, 계정·접속정보, 외부 상용 에셋은 포함하지 않습니다.
- 팀 프로젝트와 수업 프로젝트는 담당 범위와 제외 범위를 각 README에 명시합니다.
- AI를 활용한 프로젝트는 활용 방식과 본인이 수행한 판단·검증 범위를 [`docs/AUTHORSHIP_AND_AI_USE.md`](docs/AUTHORSHIP_AND_AI_USE.md)에 공개합니다.
- 각 복사본의 출처 구분과 SHA-256은 [`docs/SOURCE_MANIFEST.csv`](docs/SOURCE_MANIFEST.csv)에서 확인할 수 있습니다.
- 자동 검사 결과와 수동 판정은 [`docs/VERIFICATION_REPORT.md`](docs/VERIFICATION_REPORT.md), [`docs/REVIEWED_WARNINGS.md`](docs/REVIEWED_WARNINGS.md)에 기록합니다.
- 실제로 재수행한 빌드·테스트와 재현 범위는 [`docs/QA_SUMMARY.md`](docs/QA_SUMMARY.md)에 기록합니다.
- 이 저장소 전체에 일괄 적용되는 오픈소스 라이선스는 부여하지 않습니다. 자세한 내용은 [`NOTICE.md`](NOTICE.md)를 확인해 주세요.

## 실행과 검증에 관하여

프로젝트별 엔진·라이브러리·DB 의존성이 서로 달라 저장소 루트에서 한 번에 빌드하지 않습니다. 각 폴더의 README에 빌드 조건, 실행 방법, 확인된 검증 결과와 재현하지 못한 항목을 구분해 기록했습니다.

채용 사이트 제출 항목에는 PDF를 파일로 등록하고, 이 저장소의 공개 URL을 링크로 등록하는 구성을 권장합니다. 제출 전 확인 순서는 [`docs/SUBMISSION_GUIDE.md`](docs/SUBMISSION_GUIDE.md)에 정리했습니다.
