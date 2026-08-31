# 공개 전 검수 기록

이 저장소는 원본 개발 폴더를 그대로 업로드하지 않고, 공개 가능한 소스와 문서만 선별한 복사본입니다.

## 공통 제외 항목

- `.git` 등 원본 저장소 이력과 중첩 저장소 메타데이터
- `Binaries`, `Intermediate`, `DerivedDataCache`, `Saved`, `Build`, `build`, `bin`, `obj`
- 실행 파일, DLL, PDB, PAK, 압축 배포본과 IDE 캐시
- 로컬 DB, DB 백업, 로그, 덤프, 토큰, 비밀번호, 개인 경로와 공개가 불필요한 IP/DSN
- Marketplace/Fab 등 재배포 권한을 별도로 확인해야 하는 에셋
- 팀원의 독립 코드와 수업 제공 코드 중 공개 권한을 확인하지 못한 항목

## 프로젝트별 경계

### Dophamine Addiction

서버·네트워크·서버 권위 판정과 물리 관련 검토 대상만 포함합니다. Content, SourceArt, 패키징 결과와 팀원 중심 클라이언트/UI 영역은 제외합니다.

### C++ IOCP 2D MMORPG

직접 작성한 C++ 소스와 프로젝트 설정을 중심으로 포함합니다. 수업 제공 가능성이 있는 protocol header와 sample 기반 파일, Release 실행본, 발표용 원본 개인정보는 제외합니다.

### EverPlanet World Study

현재 Godot source/scene/data와 자체 진행 capture, 이전 Unreal C++ Source·자동화 테스트·Unreal Python 도구를 포함합니다. 원작 영상 추출 frame, Unreal Content/engine 생성 파일, local-only credential을 포함한 Config와 외부 에셋은 제외합니다.

### Football Owner

시뮬레이션 코어, repository, QML, schema/migration, 테스트와 필요한 문서를 포함합니다. 코드 안의 소규모 1992-93 prototype fixture와 출처 문서는 포함하되, 실존 인물·구단의 대규모 원천 데이터와 source-game 기반 ratings 자료, 로컬 save DB와 build 결과는 제외합니다.

### 계약과 공명 RPG

기획 문서와 공개용 요약만 포함합니다. 아직 구현 코드는 없으며 구현 완료로 표현하지 않습니다.

## 자동 검사

최종 복사 후 다음 항목을 검사합니다.

- SHA-256 source manifest 생성
- 50 MiB 이상 파일과 실행/압축/DB 파일 탐지
- 비밀번호·토큰·private key·connection string·절대 사용자 경로 후보 탐지
- 각 프로젝트 README의 경로 링크와 복사 누락 확인
