# 빌드·테스트 재현 기록

이 문서는 포트폴리오를 공개하기 직전에 **이 저장소에 들어 있는 복사본으로 다시 수행한 검사**와, 원본 프로젝트의 기존 검증 기록을 구분합니다.

## 공개 저장소 전체 검사

- 대용량·빌드 산출물·금지 확장자·비밀정보 후보 자동 검사: 통과
- 환경 의존 경고 1건: IOCP 서버의 로컬 ODBC DSN 이름
- 해당 경고의 공개 판단: [`REVIEWED_WARNINGS.md`](REVIEWED_WARNINGS.md)
- 파일별 출처와 SHA-256: [`SOURCE_MANIFEST.csv`](SOURCE_MANIFEST.csv)

최신 수치는 [`VERIFICATION_REPORT.md`](VERIFICATION_REPORT.md)에서 확인할 수 있습니다.

## Football Owner — 이번 공개본에서 재수행

- 일자: 2026-08-31
- 범위: GUI·대규모 역사 데이터 제외, C++ core와 core regression test
- 도구: Visual Studio 2022 MSVC 19.44, CMake, Ninja, CTest
- 구성: `FOOTBALL_OWNER_BUILD_GUI=OFF`, `FOOTBALL_OWNER_BUILD_TESTS=ON`, Debug
- 결과: 구성 성공, 빌드 성공, CTest `1/1` 통과 (`football_owner_core_tests`)

저장소의 일반 재현 명령은 [`projects/04-football-owner/README.md`](../projects/04-football-owner/README.md)에 있습니다.

## 이번 패키징 단계에서 재수행하지 않은 항목

- Dophamine Addiction: Unreal Engine 콘텐츠와 팀 전체 저장소를 재배포하지 않아 이 복사본만으로 패키징하지 않았습니다. 프로젝트 README는 원본의 4대 PC 실제 플레이 검증 기록·영상과 고정 commit에서 추출한 코드 발췌본을 구분해 제공합니다.
- C++ IOCP 2D MMORPG: 수업 제공 protocol header를 재배포하지 않아 이 복사본만으로 전체 재빌드하지 않았습니다. 기존 캡처와 자체 측정값은 README에서 `기존 기록`으로만 표시합니다.
- EverPlanet World Study: 현재 Godot 작업과 이전 Unreal 연구를 함께 보존하되, 엔진 프로젝트 전체 에셋을 공개하지 않아 이번 단계에서 end-to-end 재빌드하지 않았습니다.
- 계약과 공명 RPG: 기획서만 있는 프로젝트이므로 빌드 대상이 없습니다.

재수행하지 않은 항목을 완료 또는 독립 재현된 결과로 주장하지 않습니다.
