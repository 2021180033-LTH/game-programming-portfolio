cmake_minimum_required(VERSION 3.21)

project(FootballOwner
    VERSION 0.10.0
    DESCRIPTION "Historical football club-owner simulation"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FOOTBALL_OWNER_BUILD_GUI "Build the Qt/QML desktop application" ON)
option(FOOTBALL_OWNER_BUILD_TESTS "Build unit tests" ON)

file(GLOB_RECURSE FOOTBALL_OWNER_FACT_SAFE_V545_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/data/fact_safe_v545/*"
)

add_library(football_owner_core STATIC
    src/core/game_date.cpp
    src/core/game_date.h
    src/core/game_state.cpp
    src/core/game_state.h
    src/core/simulation_engine.cpp
    src/core/simulation_engine.h
)

target_include_directories(football_owner_core PUBLIC src)

if(MSVC)
    target_compile_options(football_owner_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(football_owner_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(FOOTBALL_OWNER_BUILD_GUI)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Sql)
    qt_standard_project_setup(REQUIRES 6.5)
    if(QT_KNOWN_POLICY_QTP0004)
        qt_policy(SET QTP0004 NEW)
    endif()

    qt_add_executable(FootballOwner
        src/main.cpp
        src/application/game_controller.cpp
        src/application/game_controller.h
        src/infrastructure/save_game_manager.cpp
        src/infrastructure/save_game_manager.h
        src/infrastructure/sqlite_game_repository.cpp
        src/infrastructure/sqlite_game_repository.h
    )

    # Keep the source tree grouped under qml/, while exposing Main.qml at the
    # root of the FootballOwner QML module for loadFromModule().
    set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
    set_source_files_properties(qml/components/NavigationButton.qml
        PROPERTIES QT_RESOURCE_ALIAS components/NavigationButton.qml)
    set_source_files_properties(qml/components/Panel.qml
        PROPERTIES QT_RESOURCE_ALIAS components/Panel.qml)
    set_source_files_properties(qml/components/StatCard.qml
        PROPERTIES QT_RESOURCE_ALIAS components/StatCard.qml)
    set_source_files_properties(qml/components/TransferTermsDialog.qml
        PROPERTIES QT_RESOURCE_ALIAS components/TransferTermsDialog.qml)
    set_source_files_properties(qml/components/ContractTermsDialog.qml
        PROPERTIES QT_RESOURCE_ALIAS components/ContractTermsDialog.qml)
    set_source_files_properties(qml/pages/DashboardPage.qml
        PROPERTIES QT_RESOURCE_ALIAS pages/DashboardPage.qml)
    set_source_files_properties(qml/pages/FinancePage.qml
        PROPERTIES QT_RESOURCE_ALIAS pages/FinancePage.qml)
    set_source_files_properties(qml/pages/InboxPage.qml
        PROPERTIES QT_RESOURCE_ALIAS pages/InboxPage.qml)
    set_source_files_properties(qml/pages/ManagerPage.qml
        PROPERTIES QT_RESOURCE_ALIAS pages/ManagerPage.qml)
    set_source_files_properties(qml/pages/MatchesPage.qml
        PROPERTIES QT_RESOURCE_ALIAS pages/MatchesPage.qml)
    set_source_files_properties(qml/pages/SquadPage.qml
        PROPERTIES QT_RESOURCE_ALIAS pages/SquadPage.qml)
    set_source_files_properties(qml/pages/TransfersPage.qml
        PROPERTIES QT_RESOURCE_ALIAS pages/TransfersPage.qml)
    set_source_files_properties(qml/pages/ScoutingPage.qml
        PROPERTIES QT_RESOURCE_ALIAS pages/ScoutingPage.qml)
    set_source_files_properties(qml/pages/ContractsPage.qml
        PROPERTIES QT_RESOURCE_ALIAS pages/ContractsPage.qml)
    set_source_files_properties(qml/pages/StartPage.qml
        PROPERTIES QT_RESOURCE_ALIAS pages/StartPage.qml)
    set_source_files_properties(qml/pages/GameShell.qml
        PROPERTIES QT_RESOURCE_ALIAS pages/GameShell.qml)

    qt_add_qml_module(FootballOwner
        URI FootballOwner
        VERSION 1.0
        QML_FILES
            qml/Main.qml
            qml/components/NavigationButton.qml
            qml/components/Panel.qml
            qml/components/StatCard.qml
            qml/components/TransferTermsDialog.qml
            qml/components/ContractTermsDialog.qml
            qml/pages/DashboardPage.qml
            qml/pages/FinancePage.qml
            qml/pages/InboxPage.qml
            qml/pages/ManagerPage.qml
            qml/pages/MatchesPage.qml
            qml/pages/SquadPage.qml
            qml/pages/TransfersPage.qml
            qml/pages/ScoutingPage.qml
            qml/pages/ContractsPage.qml
            qml/pages/StartPage.qml
            qml/pages/GameShell.qml
    )

    qt_add_resources(FootballOwner "database_resources"
        PREFIX "/database"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/data"
        FILES
            data/migrations/001_initial.sql
            data/migrations/002_squads_and_transfers.sql
            data/migrations/003_named_save_files.sql
            data/migrations/004_transfer_market.sql
            data/migrations/005_detailed_transfers.sql
            data/migrations/006_scouting_and_recruitment.sql
            data/migrations/007_loans_and_contract_renewals.sql
            data/migrations/008_ai_transfer_market.sql
            data/migrations/009_locked_world_baseline.sql
            data/migrations/010_competition_bridge_and_standings.sql
            data/migrations/011_squad_planning_opinions.sql
            data/migrations/012_operating_meetings.sql
            data/migrations/013_scouting_directives.sql
            data/migrations/014_fact_safe_handoff_v545.sql
            data/migrations/015_fact_safe_competition_schedule_bridge.sql
            data/migrations/016_fact_safe_roster_activation_bridge.sql
            data/migrations/017_owner_reputation_and_fan_trust.sql
            data/migrations/018_fact_safe_club_finance_model_bridge.sql
            data/seeds/1992_93_manchester_united.sql
            data/seeds/1992_93_transfer_market.sql
            data/seeds/1992_93_scouting.sql
            data/seeds/1992_93_loans_and_contracts.sql
            data/world/football_owner_1992_production_LOCKED_v11.sqlite
            data/world/VALIDATION_CERTIFICATE_LOCKED_v11.json
            ${FOOTBALL_OWNER_FACT_SAFE_V545_FILES}
    )

    target_include_directories(FootballOwner PRIVATE src)
    target_link_libraries(FootballOwner PRIVATE
        football_owner_core
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Sql
    )

    set_target_properties(FootballOwner PROPERTIES
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
    )

    install(TARGETS FootballOwner
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
    )
endif()

if(FOOTBALL_OWNER_BUILD_TESTS)
    include(CTest)
    enable_testing()

    add_executable(football_owner_core_tests tests/core_tests.cpp)
    target_link_libraries(football_owner_core_tests PRIVATE football_owner_core)

    if(MSVC)
        target_compile_options(football_owner_core_tests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(football_owner_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME football_owner_core_tests COMMAND football_owner_core_tests)

    if(FOOTBALL_OWNER_BUILD_GUI)
        add_executable(football_owner_repository_tests
            tests/repository_tests.cpp
            src/infrastructure/save_game_manager.cpp
            src/infrastructure/save_game_manager.h
            src/infrastructure/sqlite_game_repository.cpp
            src/infrastructure/sqlite_game_repository.h
        )
        target_include_directories(football_owner_repository_tests PRIVATE src)
        target_link_libraries(football_owner_repository_tests PRIVATE
            football_owner_core
            Qt6::Core
            Qt6::Sql
        )

        qt_add_resources(football_owner_repository_tests "test_database_resources"
            PREFIX "/database"
            BASE "${CMAKE_CURRENT_SOURCE_DIR}/data"
            FILES
                data/migrations/001_initial.sql
                data/migrations/002_squads_and_transfers.sql
                data/migrations/003_named_save_files.sql
                data/migrations/004_transfer_market.sql
                data/migrations/005_detailed_transfers.sql
                data/migrations/006_scouting_and_recruitment.sql
                data/migrations/007_loans_and_contract_renewals.sql
                data/migrations/008_ai_transfer_market.sql
                data/migrations/009_locked_world_baseline.sql
                data/migrations/010_competition_bridge_and_standings.sql
                data/migrations/011_squad_planning_opinions.sql
                data/migrations/012_operating_meetings.sql
                data/migrations/013_scouting_directives.sql
                data/migrations/014_fact_safe_handoff_v545.sql
                data/migrations/015_fact_safe_competition_schedule_bridge.sql
                data/migrations/016_fact_safe_roster_activation_bridge.sql
                data/migrations/017_owner_reputation_and_fan_trust.sql
                data/migrations/018_fact_safe_club_finance_model_bridge.sql
                data/seeds/1992_93_manchester_united.sql
                data/seeds/1992_93_transfer_market.sql
                data/seeds/1992_93_scouting.sql
                data/seeds/1992_93_loans_and_contracts.sql
                data/world/football_owner_1992_production_LOCKED_v11.sqlite
                data/world/VALIDATION_CERTIFICATE_LOCKED_v11.json
                ${FOOTBALL_OWNER_FACT_SAFE_V545_FILES}
        )

        if(MSVC)
            target_compile_options(football_owner_repository_tests PRIVATE /W4 /permissive- /utf-8)
        else()
            target_compile_options(football_owner_repository_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME football_owner_repository_tests COMMAND football_owner_repository_tests)
    endif()
endif()
