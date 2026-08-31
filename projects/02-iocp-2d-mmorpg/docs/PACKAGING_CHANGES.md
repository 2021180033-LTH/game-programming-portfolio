# 공개 저장소 포장 변경

원본 개발 폴더는 수정하지 않았습니다. 아래는 공개 복사본에서만 적용한 변경과 제외입니다.

## 변경

- 최종 `Client_Release/main.cpp`와 그 옆의 `GameProtocol.h`를 `source/Client`로 배치했습니다.
- `Client.vcxproj`의 개인 PC 절대 include 경로를 저장소 상대 경로로 바꿨습니다.
- 실행 화면 파일명은 GitHub에서 의미를 알 수 있는 영문 이름으로 바꿨습니다.

## 제외

- 수업 제공 `protocol_2026.h`
- third-party SFML package 본체
- Lua header/library 본체와 로컬 연결 설정
- 공개 권리를 다시 확인해야 하는 client `Assets`
- NeHe 기반 drawing code와 개인 LAN IP를 포함한 StressTest harness 전체
- `.vs`, `x64`, Debug/Release, 실행 파일, PDB, DB backup과 ZIP
- Office 문서는 개인정보 metadata와 현행 코드에 없는 설명이 있어 Markdown 설명으로 대체

`protocol_2026.h`, Lua 개발 파일과 client asset이 없기 때문에 공개본만으로 server/client를 완전 빌드하거나 캡처와 같은 시각 결과를 재현할 수 없습니다. 이 제한은 수업 제공물·third-party dependency·asset의 저작권 경계를 지키기 위한 의도적인 선택입니다.
