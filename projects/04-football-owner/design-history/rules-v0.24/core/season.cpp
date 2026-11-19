#include "core/season.h"
#include "core/era_rules.h"
#include "core/operating_finance.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace football_owner {
std::uint32_t seasonStableHash(const std::string& text)
{
    std::uint32_t value = 2166136261U;
    for (const unsigned char c : text) { value ^= c; value *= 16777619U; }
    return value;
}
std::string careerSeasonId(const GameDate& start)
{ return "season-" + std::to_string(start.year) + "-" + std::to_string(start.year + 1); }
const CareerSeason* activeSeason(const GameState& s)
{
    const auto it = std::find_if(s.seasons.begin(), s.seasons.end(), [&](const auto& v) { return v.id == s.activeSeasonId; });
    return it == s.seasons.end() ? nullptr : &*it;
}
const RuntimeCompetitionSeason* runtimeCompetition(const GameState& s, const std::string& id)
{
    const auto it = std::find_if(s.runtimeCompetitions.begin(), s.runtimeCompetitions.end(), [&](const auto& v) { return v.id == id; });
    return it == s.runtimeCompetitions.end() ? nullptr : &*it;
}
bool isActiveFixture(const GameState& s, const Fixture& f)
{
    if (s.activeSeasonId.empty()) return true;
    const auto* c = runtimeCompetition(s, f.seasonId);
    return c && c->careerSeasonId == s.activeSeasonId;
}
bool isPlayerFixture(const GameState& s, const Fixture& f)
{
    return isActiveFixture(s, f) && (f.homeClubId == s.club.id || f.awayClubId == s.club.id
        || (f.homeClubId.empty() && (f.homeClub == s.club.name || f.awayClub == s.club.name)));
}
namespace {
auto fixtureOrder(const Fixture& f) { return std::tie(f.date, f.id); }
std::vector<CompetitionSeasonTeam> participants(const GameState& s, const std::string& id)
{
    std::vector<CompetitionSeasonTeam> teams;
    for (const auto& t : s.competitionSeasonTeams) if (t.seasonId == id) teams.push_back(t);
    return teams;
}
void report(GameState& s, const std::string& key, const std::string& title, const std::string& body)
{
    if (std::any_of(s.inbox.begin(), s.inbox.end(), [&](const auto& i) { return i.eventKey == key; })) return;
    int id = 1;
    for (const auto& i : s.inbox) id = std::max(id, i.id + 1);
    s.inbox.push_back({id, key, s.currentDate, InboxCategory::Football, title, body, true, false});
}
Fixture match(const RuntimeCompetitionSeason& c, const CompetitionSeasonTeam& h,
              const CompetitionSeasonTeam& a, const GameDate& date, const std::string& suffix, int round)
{
    Fixture f;
    f.id = "runtime-" + c.id + "-" + suffix; f.date = date; f.competition = c.name;
    f.homeClub = h.clubName; f.awayClub = a.clubName; f.homeStrength = h.strength; f.awayStrength = a.strength;
    f.seasonId = c.id; f.competitionId = c.competitionId; f.homeClubId = h.clubId; f.awayClubId = a.clubId;
    f.generationMethod = round ? "seeded_single_leg_playoff_v1" : "circle_weekly_midweek_v1";
    f.generationSeed = c.scheduleSeed; f.provenance = "gameplay_model_not_historical_fixture;" + c.provenance;
    f.knockoutRound = round;
    return f;
}
std::vector<LeagueStandingRow> standings(const GameState& s, const std::string& id)
{
    std::vector<LeagueStandingRow> rows;
    for (const auto& r : s.leagueStandings) if (r.seasonId == id) rows.push_back(r);
    return rows;
}
}

std::vector<Fixture> generateLeagueSchedule(const CareerSeason& season, const RuntimeCompetitionSeason& c,
                                          std::vector<CompetitionSeasonTeam> teams)
{
    if (teams.size() < 2) throw std::runtime_error("League requires at least 2 participants");
    std::set<std::string> ids;
    for (const auto& t : teams) if (t.clubId.empty() || !ids.insert(t.clubId).second
        || t.seasonId != c.id || t.competitionId != c.competitionId) throw std::runtime_error("Invalid league participants");
    std::sort(teams.begin(), teams.end(), [&](const auto& a, const auto& b) {
        return std::tuple{seasonStableHash(std::to_string(c.scheduleSeed) + a.clubId), a.clubId}
            < std::tuple{seasonStableHash(std::to_string(c.scheduleSeed) + b.clubId), b.clubId};
    });
    // An odd participant count (Serie B 2018-19 had 19 clubs) gets a bye slot in the circle:
    // the team paired with it sits that round out. Even counts are scheduled exactly as before.
    std::vector<CompetitionSeasonTeam> circle = teams;
    if (circle.size() % 2 != 0) {
        CompetitionSeasonTeam bye;
        bye.clubId = "__bye__";
        bye.seasonId = c.id;
        bye.competitionId = c.competitionId;
        circle.push_back(bye);
    }
    const int rounds = 2 * (static_cast<int>(circle.size()) - 1);
    std::vector<GameDate> slots;
    const auto last = season.endDate.addDays(-35); // leave room for postseason
    for (auto date = season.startDate.addDays(42); date <= last; date = date.addDays(1)) {
        using namespace std::chrono;
        const weekday wd{sys_days{year{date.year}/month{static_cast<unsigned>(date.month)}/day{static_cast<unsigned>(date.day)}}};
        if (wd == Saturday || wd == Wednesday) slots.push_back(date);
    }
    if (static_cast<int>(slots.size()) < rounds) throw std::runtime_error("Insufficient weekly/midweek schedule slots");
    std::vector<Fixture> result;
    auto ring = circle;
    for (int leg = 0; leg < 2; ++leg) {
        ring = circle;
        for (int r = 0; r < rounds/2; ++r) {
            const int round = leg * (rounds/2) + r;
            const auto date = slots.at(static_cast<std::size_t>(round) * (slots.size()-1) / static_cast<std::size_t>(rounds-1));
            for (std::size_t i = 0; i < ring.size()/2; ++i) {
                const auto& first = ring[i]; const auto& second = ring[ring.size()-1-i];
                if (first.clubId == "__bye__" || second.clubId == "__bye__") continue;
                const bool swap = ((r + static_cast<int>(i)) % 2 == 1) != (leg == 1);
                const auto& h = swap ? second : first; const auto& a = swap ? first : second;
                result.push_back(match(c,h,a,date,"r"+std::to_string(round+1)+"-"+std::to_string(i+1),0));
            }
            std::rotate(ring.begin()+1, ring.end()-1, ring.end());
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return fixtureOrder(a) < fixtureOrder(b); });
    return result;
}

std::string validateSeasonState(const GameState& s)
{
    if (s.activeSeasonId.empty() && s.seasons.empty()) return {};
    if (!activeSeason(s)) return "활성 시즌이 없습니다";
    if (activeSeason(s)->finalized) return "활성 시즌이 이미 마감되었습니다";
    std::set<std::string> seasons, competitions, fixtureIds;
    for (const auto& season : s.seasons) {
        if (!seasons.insert(season.id).second || !season.startDate.isValid() || !season.endDate.isValid()
            || season.endDate <= season.startDate) return "시즌 ID/기간 오류";
        if (season.finalized != !season.nextSeasonId.empty()) return "시즌 전환 guard 불일치";
        if (season.finalized != (season.stage == "finalized")) return "시즌 단계/마감 상태 불일치";
    }
    for (const auto& season : s.seasons) if (season.finalized && !seasons.contains(season.nextSeasonId)) return "다음 시즌 누락";
    std::set<std::pair<std::string,std::string>> worldMembership;
    for (const auto& c : s.runtimeCompetitions) {
        if (!competitions.insert(c.id).second || !seasons.contains(c.careerSeasonId)) return "대회 시즌 관계 오류";
        const auto teams = participants(s,c.id);
        if (teams.size()<2) return "참가팀 수 오류";
        std::set<std::string> clubs;
        for (const auto& t : teams) {
            if (t.clubId.empty() || t.competitionId != c.competitionId || !clubs.insert(t.clubId).second
                || !worldMembership.insert({c.careerSeasonId,t.clubId}).second) return "중복/잘못된 참가팀";
        }
        std::set<std::pair<std::string,std::string>> pairs, dates;
        std::size_t count = 0, semis = 0, finals = 0;
        const auto& season = *std::find_if(s.seasons.begin(),s.seasons.end(),[&](const auto& v){return v.id==c.careerSeasonId;});
        for (const auto& f : s.fixtures) if (f.seasonId == c.id) {
            if (f.competitionId != c.competitionId || f.homeClubId == f.awayClubId
                || !clubs.contains(f.homeClubId) || !clubs.contains(f.awayClubId)) return "경기 참가팀/대회 오류";
            if (!f.date.isValid() || f.date < season.startDate || season.endDate < f.date) return "시즌 범위 밖 경기";
            if (!dates.insert({f.date.toIsoString(),f.homeClubId}).second
                || !dates.insert({f.date.toIsoString(),f.awayClubId}).second) return "같은 날짜 중복 경기";
            if (f.knockoutRound == 0) {
                ++count;
                if (!pairs.insert({f.homeClubId,f.awayClubId}).second) return "중복 리그 대진";
            } else if (f.knockoutRound < 1 || f.knockoutRound > 2
                || (f.played && f.winnerClubId != f.homeClubId && f.winnerClubId != f.awayClubId)) return "플레이오프 결과 오류";
            if (f.knockoutRound == 1) ++semis;
            if (f.knockoutRound == 2) ++finals;
            if (season.finalized && !f.played) return "미완료 경기를 가진 시즌 마감";
        }
        if (count != teams.size()*(teams.size()-1)) return "리그 일정 수 불일치";
        if ((semis != 0 && semis != 2) || finals > 1 || (finals && semis != 2)) return "플레이오프 대진 수 오류";
        std::set<std::string> archivedClubs;
        for (const auto& h : s.seasonHistory) if (h.row.seasonId == c.id) {
            if (!season.finalized || !clubs.contains(h.row.clubId) || !archivedClubs.insert(h.row.clubId).second
                || h.row.competitionId != c.competitionId || h.row.played != static_cast<int>(2*(teams.size()-1))
                || h.row.position < 1 || h.row.position > static_cast<int>(teams.size())) return "확정 순위 기록 오류";
        }
        if (season.finalized && archivedClubs.size() != teams.size()) return "시즌 확정 기록 누락";
        if (season.finalized && c.playoffSlots && (finals != 1 || c.playoffWinnerId.empty())) return "승격 결정 누락";
    }
    for (const auto& t : s.competitionSeasonTeams) if (!competitions.contains(t.seasonId)) return "참가팀 시즌 없음";
    for (const auto& f : s.fixtures) if (!fixtureIds.insert(f.id).second || !competitions.contains(f.seasonId)) return "경기 ID/시즌 오류";
    for (const auto& h : s.seasonHistory) if (!competitions.contains(h.row.seasonId)) return "확정 기록의 시즌 없음";
    return {};
}

void ensureSeasonSchedules(GameState& state)
{
    if (state.seasons.empty()) return;
    GameState staged = state;
    for (const auto& c : staged.runtimeCompetitions) {
        if (std::any_of(staged.fixtures.begin(),staged.fixtures.end(),[&](const auto& f){return f.seasonId==c.id;})) continue;
        const auto season = std::find_if(staged.seasons.begin(),staged.seasons.end(),[&](const auto& v){return v.id==c.careerSeasonId;});
        if (season==staged.seasons.end()) throw std::runtime_error("Missing season for schedule");
        auto generated = generateLeagueSchedule(*season,c,participants(staged,c.id));
        staged.fixtures.insert(staged.fixtures.end(),generated.begin(),generated.end());
    }
    const auto error = validateSeasonState(staged);
    if (!error.empty()) throw std::runtime_error(error);
    std::sort(staged.fixtures.begin(),staged.fixtures.end(),[](const auto& a,const auto& b){return fixtureOrder(a)<fixtureOrder(b);});
    state = std::move(staged);
}

void recalculateSeasonStandings(GameState& s)
{
    std::map<std::pair<std::string,std::string>,LeagueStandingRow> rows;
    for (const auto& t : s.competitionSeasonTeams) {
        LeagueStandingRow row; row.seasonId=t.seasonId; row.competition=t.competition;
        row.clubId=t.clubId; row.clubName=t.clubName; row.competitionId=t.competitionId;
        rows[{t.seasonId,t.clubId}]=row;
    }
    for (const auto& f : s.fixtures) {
        if (!f.played || f.knockoutRound) continue;
        const auto* c=runtimeCompetition(s,f.seasonId);
        const auto hi=rows.find({f.seasonId,f.homeClubId}), ai=rows.find({f.seasonId,f.awayClubId});
        if (!c || hi==rows.end() || ai==rows.end()) continue;
        auto& h=hi->second; auto& a=ai->second;
        ++h.played; ++a.played; h.goalsFor+=f.homeGoals; h.goalsAgainst+=f.awayGoals;
        a.goalsFor+=f.awayGoals; a.goalsAgainst+=f.homeGoals;
        if (f.homeGoals>f.awayGoals) {++h.won; ++a.lost; h.points+=c->pointsWin; a.points+=c->pointsLoss;}
        else if (f.homeGoals<f.awayGoals) {++a.won; ++h.lost; a.points+=c->pointsWin; h.points+=c->pointsLoss;}
        else {++h.drawn; ++a.drawn; h.points+=c->pointsDraw; a.points+=c->pointsDraw;}
    }
    s.leagueStandings.clear();
    for (auto& [key,row] : rows) { row.goalDifference=row.goalsFor-row.goalsAgainst; s.leagueStandings.push_back(row); }
    std::sort(s.leagueStandings.begin(),s.leagueStandings.end(),[](const auto& a,const auto& b){
        return std::tuple{a.seasonId,-a.points,-a.goalDifference,-a.goalsFor,a.clubId}
            < std::tuple{b.seasonId,-b.points,-b.goalDifference,-b.goalsFor,b.clubId};
    });
    std::string current; int position=0;
    for (auto& row : s.leagueStandings) { if(row.seasonId!=current){current=row.seasonId;position=0;} row.position=++position; }
    // Finalized records are snapshots, never recomputed from mutable future state.
    for (auto& row : s.leagueStandings) {
        const auto old=std::find_if(s.seasonHistory.begin(),s.seasonHistory.end(),[&](const auto& h){return h.row.seasonId==row.seasonId && h.row.clubId==row.clubId;});
        if(old!=s.seasonHistory.end()) row=old->row;
    }
}

std::string seasonStatus(const GameState& s)
{
    if (!s.seasonBlockReason.empty()) return s.seasonBlockReason;
    const auto* season=activeSeason(s); if (!season) return "시즌 runtime 없음";
    std::string text;
    for (const auto& c : s.runtimeCompetitions) if(c.careerSeasonId==season->id) {
        int regular=0,playoff=0;
        for(const auto& f:s.fixtures) if(f.seasonId==c.id && !f.played) { if(f.knockoutRound) ++playoff; else ++regular; }
        text+=c.name+": 정규 "+std::to_string(regular)+"경기 / 플레이오프 "+std::to_string(playoff)+"경기 미완료. ";
    }
    if(season->stage=="ready") text+="결산 준비 완료 · "+season->endDate.addDays(1).toIsoString()+"에 다음 시즌 진입";
    return text;
}

namespace {
const char* const kLegacySingleLeagueBlock="v1은 인접한 두 runtime 리그를 요구합니다";

// The registration regime follows the calendar (core/era_rules.h). The owner hears about it
// only when it actually changes (free movement from 1996-97, registration periods from 2002-03).
void applySeasonEraRules(GameState& s, const CareerSeason& next)
{
    const auto before=s.eraRules;
    applyModelEraRules(s,next.startDate);
    if(before.transferWindowsActive==s.eraRules.transferWindowsActive
        && before.freeMovementAfterContract==s.eraRules.freeMovementAfterContract) return;
    report(s,"season-rules-"+next.id,"새 시즌부터 등록 규정이 바뀝니다",
        eraSeasonLabel(next.startDate.year)+" 시즌: "+eraRuleSummary(s.eraRules,next.startDate)
            +". 시대별 게임 모델이며 리그별 실제 날짜와 다를 수 있습니다.");
}

// Shared end of a lifecycle step: the staged copy replaces the state only when it is valid.
void commitSeasonState(GameState& state, GameState& s)
{
    std::sort(s.fixtures.begin(),s.fixtures.end(),[](const auto& a,const auto& b){return fixtureOrder(a)<fixtureOrder(b);});
    const auto error=validateSeasonState(s);
    if(!error.empty()){state.seasonBlockReason=error;return;}
    rollOperatingSeason(s);
    const auto financeError=validateOperatingFinance(s);
    if(!financeError.empty()){state.seasonBlockReason=financeError;return;}
    state=std::move(s);
}

// A career whose data has one league and nothing below it (296 of the 438 roster packages):
// the season closes with its champion and the same clubs start the next one. No promotion or
// relegation is invented for a tier the data does not carry.
void processSingleLeagueSeason(GameState& state, GameState& s, CareerSeason& season, const std::size_t comp)
{
    // Saves made before this path existed carry the old block text; it no longer applies.
    if(state.seasonBlockReason==kLegacySingleLeagueBlock) state.seasonBlockReason.clear();
    const auto league=s.runtimeCompetitions[comp]; // a copy: the vector grows below
    if(league.promotionSlots!=0 || league.playoffSlots!=0 || league.relegationSlots!=0) {
        state.seasonBlockReason="단일 리그 시즌은 승격·강등 슬롯을 지원하지 않습니다";return;
    }
    for(const auto& f:s.fixtures) if(isActiveFixture(s,f) && !f.played) return;
    recalculateSeasonStandings(s);
    report(s,"season-regular-"+season.id,"정규리그 일정 완료","리그 정규 경기가 완료되었습니다. 하부 리그가 없는 시즌이라 승강 없이 결산을 준비합니다.");
    season.stage="ready";
    if(season.endDate<s.currentDate) {
        const auto rows=standings(s,league.id);
        CareerSeason next; next.startDate=season.endDate.addDays(1); next.endDate=next.startDate.addMonths(12).addDays(-1);
        next.id=careerSeasonId(next.startDate); next.boundaryPolicy=season.boundaryPolicy;
        const auto oldId=season.id;
        for(const auto& row:rows) s.seasonHistory.push_back({row,row.position==1?"champion":"retained"});
        auto newLeague=league;
        newLeague.careerSeasonId=next.id; newLeague.id=newLeague.competitionId+"-"+next.id;
        newLeague.scheduleSeed=seasonStableHash(newLeague.id+std::to_string(newLeague.scheduleSeed));
        newLeague.playoffWinnerId.clear(); newLeague.provenance+=";rules_carried_forward_gameplay_v1";
        int count=0;
        for(auto member:participants(s,league.id)) {
            member.seasonId=newLeague.id; member.seedOrder=++count;
            s.competitionSeasonTeams.push_back(member);
        }
        season.finalized=true; season.nextSeasonId=next.id; season.stage="finalized";
        s.seasons.push_back(next); s.runtimeCompetitions.push_back(newLeague); // `season` dangles from here
        s.activeSeasonId=next.id;
        applySeasonEraRules(s,next);
        try {ensureSeasonSchedules(s);} catch(const std::exception& e){state.seasonBlockReason=e.what();return;}
        report(s,"season-final-"+oldId,"시즌 최종 결과 확정","우승과 최종 순위가 시즌 기록에 보존되었습니다.");
        report(s,"season-movement-"+oldId,"승격·강등 없음","이 커리어의 데이터에는 하부 리그가 없어 같은 구단들이 다음 시즌을 시작합니다. 게임 모델이며 역사 기록이 아닙니다.");
        report(s,"season-next-"+next.id,"다음 시즌 준비 완료",next.id+" · 생성 일정은 역사 일정이 아닌 게임 모델입니다. 재정·계약·부채는 이월됩니다.");
        recalculateSeasonStandings(s);
    }
    commitSeasonState(state,s);
}
}

void processSeasonLifecycle(GameState& state)
{
    const auto* active=activeSeason(state); if(!active || active->finalized) return;
    // Ambiguous very old saves are held, never manufacture multiple missing seasons.
    if(state.currentDate > active->endDate.addMonths(12)) {state.seasonBlockReason="구버전 날짜가 복구 가능 시즌을 초과합니다. 자동 다중 시즌 생성 보류";return;}
    const auto invalid=validateSeasonState(state);
    if(!invalid.empty()){state.seasonBlockReason=invalid;return;}
    GameState s=state;
    s.seasonBlockReason.clear();
    auto& season=*std::find_if(s.seasons.begin(),s.seasons.end(),[&](const auto& v){return v.id==s.activeSeasonId;});
    std::vector<std::size_t> comps;
    for(std::size_t i=0;i<s.runtimeCompetitions.size();++i) if(s.runtimeCompetitions[i].careerSeasonId==season.id) comps.push_back(i);
    std::sort(comps.begin(),comps.end(),[&](auto a,auto b){return s.runtimeCompetitions[a].tier<s.runtimeCompetitions[b].tier;});
    if(comps.size()==1){processSingleLeagueSeason(state,s,season,comps[0]);return;}
    if(comps.size()!=2){state.seasonBlockReason="v1은 한 개 또는 인접한 두 runtime 리그를 요구합니다";return;}
    for(const auto& f:s.fixtures) if(isActiveFixture(s,f) && !f.knockoutRound && !f.played) return;
    recalculateSeasonStandings(s);
    report(s,"season-regular-"+season.id,"정규리그 일정 완료","두 리그의 정규 경기가 완료되었습니다. 승격 플레이오프와 결산을 준비합니다.");
    auto& upper=s.runtimeCompetitions[comps[0]]; auto& lower=s.runtimeCompetitions[comps[1]];
    auto upperRows=standings(s,upper.id), lowerRows=standings(s,lower.id);
    if(lower.playoffSlots!=4 || lower.promotionSlots<0 || upper.relegationSlots!=lower.promotionSlots+1
        || static_cast<int>(lowerRows.size())<lower.promotionSlots+lower.playoffSlots
        || static_cast<int>(upperRows.size())<=upper.relegationSlots) {
        state.seasonBlockReason="지원되지 않는 승격/플레이오프 슬롯 조합";return;
    }
    const auto lowerTeams=participants(s,lower.id);
    const auto team=[&](const std::string& id)->const CompetitionSeasonTeam& {
        return *std::find_if(lowerTeams.begin(),lowerTeams.end(),[&](const auto& t){return t.clubId==id;});
    };
    std::vector<Fixture> semifinals, finals;
    for(const auto& f:s.fixtures) if(f.seasonId==lower.id) {
        if(f.knockoutRound==1) semifinals.push_back(f);
        if(f.knockoutRound==2) finals.push_back(f);
    }
    if(semifinals.empty()) {
        GameDate last=season.startDate;
        for(const auto& f:s.fixtures) if(isActiveFixture(s,f) && last<f.date) last=f.date;
        // Catch-up uses original scheduled dates without replaying economic events.
        const auto date=last.addDays(7);
        if(season.endDate<date.addDays(7)){state.seasonBlockReason="플레이오프 달력 공간 부족";return;}
        const auto n=static_cast<std::size_t>(lower.promotionSlots);
        s.fixtures.push_back(match(lower,team(lowerRows[n].clubId),team(lowerRows[n+3].clubId),date,"semi-1",1));
        s.fixtures.push_back(match(lower,team(lowerRows[n+1].clubId),team(lowerRows[n+2].clubId),date,"semi-2",1));
        season.stage="playoff";
    } else if(semifinals.size()!=2) {state.seasonBlockReason="플레이오프 준결승 수 불일치";return;}
    else if(std::all_of(semifinals.begin(),semifinals.end(),[](const auto& f){return f.played;})) {
        if(finals.empty()) {
            s.fixtures.push_back(match(lower,team(semifinals[0].winnerClubId),team(semifinals[1].winnerClubId),
                std::max(semifinals[0].date,semifinals[1].date).addDays(7),"final",2));
        } else if(finals.size()==1 && finals[0].played) {
            lower.playoffWinnerId=finals[0].winnerClubId; season.stage="ready";
        }
    }
    if(season.stage=="ready" && season.endDate< s.currentDate) {
        std::set<std::string> promoted, relegated;
        for(int i=0;i<lower.promotionSlots;++i) promoted.insert(lowerRows.at(static_cast<std::size_t>(i)).clubId);
        promoted.insert(lower.playoffWinnerId);
        for(int i=0;i<upper.relegationSlots;++i) relegated.insert(upperRows[upperRows.size()-1-static_cast<std::size_t>(i)].clubId);
        if(promoted.size()!=relegated.size()){state.seasonBlockReason="승강 참가팀 검증 실패";return;}
        CareerSeason next; next.startDate=season.endDate.addDays(1); next.endDate=next.startDate.addMonths(12).addDays(-1);
        next.id=careerSeasonId(next.startDate); next.boundaryPolicy=season.boundaryPolicy;
        const auto oldId=season.id;
        for(const auto& rows:{upperRows,lowerRows}) for(const auto& row:rows) {
            std::string outcome=row.position==1?"champion":"retained";
            if(promoted.contains(row.clubId)) outcome=row.clubId==lower.playoffWinnerId?"playoff_promoted":"promoted";
            if(relegated.contains(row.clubId)) outcome="relegated";
            s.seasonHistory.push_back({row,outcome});
        }
        auto newUpper=upper, newLower=lower;
        for(auto* c:{&newUpper,&newLower}) {
            c->careerSeasonId=next.id; c->id=c->competitionId+"-"+next.id;
            c->scheduleSeed=seasonStableHash(c->id+std::to_string(c->scheduleSeed));
            c->playoffWinnerId.clear(); c->provenance+=";rules_carried_forward_gameplay_v1";
        }
        auto members=participants(s,upper.id), lowerMembers=participants(s,lower.id);
        members.insert(members.end(),lowerMembers.begin(),lowerMembers.end());
        int upperCount=0, lowerCount=0;
        for(auto member:members) {
            bool goesUpper=member.competitionId==upper.competitionId;
            if(promoted.contains(member.clubId)) goesUpper=true;
            if(relegated.contains(member.clubId)) goesUpper=false;
            const auto& destination=goesUpper?newUpper:newLower;
            member.seasonId=destination.id; member.competitionId=destination.competitionId; member.competition=destination.name;
            member.seedOrder=goesUpper?++upperCount:++lowerCount;
            if(member.clubId==s.club.id) s.club.leagueName=member.competition;
            s.competitionSeasonTeams.push_back(member);
        }
        if(upperCount!=static_cast<int>(upperRows.size()) || lowerCount!=static_cast<int>(lowerRows.size())) {
            state.seasonBlockReason="다음 시즌 리그 규모 불일치";return;
        }
        season.finalized=true; season.nextSeasonId=next.id; season.stage="finalized";
        s.seasons.push_back(next); s.runtimeCompetitions.push_back(newUpper); s.runtimeCompetitions.push_back(newLower);
        s.activeSeasonId=next.id;
        applySeasonEraRules(s,next);
        try {ensureSeasonSchedules(s);} catch(const std::exception& e){state.seasonBlockReason=e.what();return;}
        report(s,"season-final-"+oldId,"시즌 최종 결과 확정","우승·최종 순위와 승강 결과가 시즌 기록에 보존되었습니다.");
        report(s,"season-movement-"+oldId,"승격·강등 반영", "다음 시즌 소속: "+s.club.leagueName+". 3부 이하 교환은 runtime 범위 밖입니다.");
        report(s,"season-next-"+next.id,"다음 시즌 준비 완료",next.id+" · 생성 일정은 역사 일정이 아닌 게임 모델입니다. 재정·계약·부채는 이월됩니다.");
        recalculateSeasonStandings(s);
    }
    commitSeasonState(state,s);
}
}
