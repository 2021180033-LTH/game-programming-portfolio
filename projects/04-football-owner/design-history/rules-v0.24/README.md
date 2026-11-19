# 0.24.0 개발본의 구단 운영 규칙

인수 뒤 구단에서 실제 처리하는 규칙과 해당 검증 코드를 원래 파일 단위로 발췌했습니다. 아래 표의 왼쪽은 규칙을 적용하는 코드, 오른쪽은 입력과 기대 결과를 확인하는 테스트입니다. 발췌 폴더는 상위 프로젝트의 데이터 모델과 모듈을 참조합니다. 독립 실행이 필요한 경우에는 프로젝트 루트의 기존 0.10 공개 빌드 안내를 참고할 수 있습니다.

| 플레이 상황 | 규칙 | 테스트 |
|---|---|---|
| 구단 인수 조건 선택 | [ownership.cpp](core/ownership.cpp) | [인수 조건](tests/ownership_tests.cpp) |
| 취임 후 첫 100일과 약속 이행 | [first_hundred_days.cpp](core/first_hundred_days.cpp) | [이행·기한 판정](tests/first_hundred_days_tests.cpp) |
| 선수 요구와 팬 신뢰 | [owner_governance.cpp](core/owner_governance.cpp) | [조치와 보상](tests/owner_governance_tests.cpp) |
| 직원 고용과 권한 | [staff_operations.cpp](core/staff_operations.cpp) | [채용·위임](tests/staff_operations_tests.cpp) |
| 출전과 부상에 따른 성장 | [player_growth.cpp](core/player_growth.cpp) | [능력별 성장](tests/player_growth_tests.cpp) |
| 시즌 결산과 다음 시즌 | [season.cpp](core/season.cpp) | [시즌 전환](tests/season_tests.cpp) |
