# 자동 검사 경고 수동 검토

최종 자동 검사에서 남은 경고는 1건입니다.

## IOCP MMORPG의 local DSN

- 위치: `projects/02-iocp-2d-mmorpg/source/Server/DatabaseManager.cpp`
- 값: `DSN=MMORPG_DB;Trusted_Connection=yes;...`
- 판단: 사용자명·비밀번호·host IP가 없는 일반적인 local ODBC DSN 이름이며 credential이 아니므로 원본 code evidence로 유지했습니다.
- 실행 시 `MMORPG_DB_CONNECTION` environment variable로 바꿀 수 있습니다.
- 별도 위험: environment variable에 비밀번호가 든 connection string을 넣으면 현재 success log가 전체 문자열을 출력할 수 있습니다. project README에 알려진 보안 한계로 명시했습니다.

이 경고는 검토 완료했지만, 실제 공개 전 새 Git history 전체에 대해 GitHub secret scanning 결과도 다시 확인해야 합니다.

