#include "core/season.h"
#include "core/simulation_engine.h"
#include "core/funding.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

using namespace football_owner;
namespace {
void check(bool ok,const std::string& msg) {if(!ok) throw std::runtime_error(msg);}
GameState world()
{
    auto s=makeManchesterUnitedPrototype(); s.fixtures.clear(); s.competitionSeasonTeams.clear();
    s.seasons={{"test-season",{1992,7,1},{1993,6,30}}}; s.activeSeasonId="test-season";
    s.runtimeCompetitions={{"upper-old",s.activeSeasonId,"upper","Upper",1,0,0,3,3,1,0,42,"test_model",{}},
        {"lower-old",s.activeSeasonId,"lower","Lower",2,2,4,3,3,1,0,43,"test_model",{}}};
    for(int league=0;league<2;++league) for(int i=0;i<(league?10:8);++i) {
        const auto& c=s.runtimeCompetitions[static_cast<std::size_t>(league)];
        const auto id=league==0 && i==0?s.club.id:c.competitionId+std::to_string(i);
        s.competitionSeasonTeams.push_back({c.id,c.name,id,id,{},i+1,c.competitionId,50+i});
    }
    ensureSeasonSchedules(s); recalculateSeasonStandings(s); return s;
}
std::string fingerprint(const GameState& s)
{
    std::string result=s.activeSeasonId+std::to_string(s.rngSeed);
    for(const auto& f:s.fixtures) result+=f.id+f.date.toIsoString()+f.homeClubId+f.awayClubId
        +std::to_string(f.played)+std::to_string(f.homeGoals)+std::to_string(f.awayGoals)+f.winnerClubId;
    return result;
}
void finishUnitRegular(GameState& s)
{
    for(auto& f:s.fixtures){f.played=true;f.homeGoals=1;f.awayGoals=0;}
    s.currentDate={1993,5,27}; recalculateSeasonStandings(s);
}
}
int runSeasonCoreTests()
{
    const std::vector<std::pair<std::string,std::function<void()>>> tests={
        {"schedule counts, pair reciprocity, date slots and range",[]{
            auto s=world(); check(s.fixtures.size()==8*7+10*9,"fixture count");
            check(validateSeasonState(s).empty(),"schedule invariants");
            for(const auto& t:s.competitionSeasonTeams) check(std::count_if(s.fixtures.begin(),s.fixtures.end(),[&](const auto& f){return f.homeClubId==t.clubId || f.awayClubId==t.clubId;})==(t.competitionId=="upper"?14:18),"team count");
        }},
        {"schedule deterministic independent of input order and market RNG",[]{
            auto s=world(), b=s; std::reverse(b.competitionSeasonTeams.begin(),b.competitionSeasonTeams.end()); b.fixtures.clear(); b.rngSeed=999;
            ensureSeasonSchedules(b); b.rngSeed=s.rngSeed;check(fingerprint(s)==fingerprint(b),"schedule changed");
            const auto count=s.fixtures.size();ensureSeasonSchedules(s);check(count==s.fixtures.size(),"duplicate initialization");
        }},
        {"player match filtering uses stable club IDs rather than labels",[]{
            auto s=world();for(auto& f:s.fixtures){f.homeClub=s.club.name;f.awayClub=s.club.name;}
            check(std::count_if(s.fixtures.begin(),s.fixtures.end(),[&](const auto& f){return isPlayerFixture(s,f);})==14,"name-based player match filtering");
        }},
        {"odd participant count is scheduled with byes",[]{
            auto s=world(); std::vector<CompetitionSeasonTeam> teams;
            for(const auto& t:s.competitionSeasonTeams)if(t.seasonId==s.runtimeCompetitions[0].id&&teams.size()<7)teams.push_back(t);
            check(teams.size()==7,"fixture world has fewer than 7 participants");
            const auto fixtures=generateLeagueSchedule(s.seasons[0],s.runtimeCompetitions[0],teams);
            check(fixtures.size()==42,"7 teams should play 42 league matches");
            std::map<std::string,int> home,away; std::set<std::pair<std::string,std::string>> pairs,dayClub;
            for(const auto& f:fixtures){check(f.homeClubId!=f.awayClubId,"self match");++home[f.homeClubId];++away[f.awayClubId];
                check(pairs.insert({f.homeClubId,f.awayClubId}).second,"duplicate pair");
                check(dayClub.insert({f.date.toIsoString(),f.homeClubId}).second&&dayClub.insert({f.date.toIsoString(),f.awayClubId}).second,"two matches on one day");}
            for(const auto& t:teams)check(home[t.clubId]==6&&away[t.clubId]==6,"unbalanced home/away");
            std::vector<CompetitionSeasonTeam> all;
            for(const auto& t:s.competitionSeasonTeams)if(t.seasonId==s.runtimeCompetitions[0].id)all.push_back(t);
            const auto even=generateLeagueSchedule(s.seasons[0],s.runtimeCompetitions[0],all);
            check(all.size()%2==0&&even.size()==all.size()*(all.size()-1),"even count changed");
        }},
        {"invalid participants and insufficient calendar rejected",[]{
            auto s=world(); auto teams=s.competitionSeasonTeams; teams.erase(teams.begin()+8,teams.end()); teams[1].clubId=teams[0].clubId;
            bool failed=false;try{(void)generateLeagueSchedule(s.seasons[0],s.runtimeCompetitions[0],teams);}catch(...){failed=true;}check(failed,"duplicate accepted");
            auto shortSeason=s.seasons[0];shortSeason.endDate=shortSeason.startDate.addDays(50);failed=false;
            teams[1].clubId="restored";try{(void)generateLeagueSchedule(shortSeason,s.runtimeCompetitions[0],teams);}catch(...){failed=true;}check(failed,"short calendar accepted");
        }},
        {"self match duplicate pair double booking and out-of-range rejected",[]{
            auto s=world();auto bad=s;bad.fixtures[0].awayClubId=bad.fixtures[0].homeClubId;check(!validateSeasonState(bad).empty(),"self accepted");
            bad=s;bad.fixtures.push_back(bad.fixtures[0]);check(!validateSeasonState(bad).empty(),"duplicate accepted");
            bad=s;bad.fixtures[0].date={1994,1,1};check(!validateSeasonState(bad).empty(),"date accepted");
            bad=s;
            auto booked=std::find_if(bad.fixtures.begin()+1,bad.fixtures.end(),[&](const auto& f){return f.homeClubId==bad.fixtures[0].homeClubId && f.date!=bad.fixtures[0].date;});
            check(booked!=bad.fixtures.end(),"double-booking fixture missing");booked->date=bad.fixtures[0].date;
            check(!validateSeasonState(bad).empty(),"double booking accepted");
            bad=s;bad.competitionSeasonTeams.push_back(bad.competitionSeasonTeams[0]);check(!validateSeasonState(bad).empty(),"membership accepted");
        }},
        {"both leagues required before postseason",[]{
            auto s=world();for(auto& f:s.fixtures) if(f.competitionId=="upper") f.played=true;
            s.currentDate={1993,7,1};processSeasonLifecycle(s);check(s.seasons.size()==1 && s.seasons[0].stage=="regular","incomplete finalized");
        }},
        {"playoff creation idempotent and excluded from points",[]{
            auto s=world();finishUnitRegular(s);processSeasonLifecycle(s);const auto size=s.fixtures.size();processSeasonLifecycle(s);check(size==s.fixtures.size(),"duplicate semis");
            auto rows=s.leagueStandings;for(auto& f:s.fixtures) if(f.knockoutRound){f.played=true;f.homeGoals=7;f.winnerClubId=f.homeClubId;}
            recalculateSeasonStandings(s);for(std::size_t i=0;i<rows.size();++i)check(rows[i].points==s.leagueStandings[i].points,"playoff points");
        }},
        {"failed transition leaves membership and season untouched",[]{
            auto s=world();finishUnitRegular(s);s.runtimeCompetitions[0].relegationSlots=2;
            const auto before=fingerprint(s);const auto teams=s.competitionSeasonTeams.size();processSeasonLifecycle(s);
            check(before==fingerprint(s) && teams==s.competitionSeasonTeams.size() && s.seasonHistory.empty() && !s.seasonBlockReason.empty(),"partial transition");
        }},
        {"actual date-path season completion next-year fixtures and deterministic results",[]{
            SimulationEngine a(world()),b(world());a.advanceDays(410);b.advanceDays(410);
            check(a.state().seasons.size()==2 && a.state().seasons.front().finalized,"no rollover");
            check(a.state().seasonHistory.size()==18,"archive missing");
            check(std::count_if(a.state().inbox.begin(),a.state().inbox.end(),[](const auto& item){
                return item.body.find("정규리그 승점에 포함하지 않습니다")!=std::string::npos;
            })==std::count_if(a.state().fixtures.begin(),a.state().fixtures.end(),[&](const auto& f){return f.played && f.knockoutRound && (f.homeClubId==a.state().club.id || f.awayClubId==a.state().club.id);}),"player playoff report missing winner/league exclusion or AI inbox spam");
            check(std::count_if(a.state().fixtures.begin(),a.state().fixtures.end(),[](const auto& f){return f.played && f.knockoutRound && !f.winnerClubId.empty();})==3,"playoff winner missing");
            check(fingerprint(a.state())==fingerprint(b.state()),"non deterministic");
            check(std::any_of(a.state().fixtures.begin(),a.state().fixtures.end(),[&](const auto& f){return isActiveFixture(a.state(),f)&&f.played;}),"no next-year match");
            check(validateSeasonState(a.state()).empty(),"transition invalid");
            const auto count=a.state().fixtures.size(), inbox=a.state().inbox.size();processSeasonLifecycle(a.mutableState());
            check(count==a.state().fixtures.size() && inbox==a.state().inbox.size(),"duplicate transition");
        }},
        {"relegated player follows runtime membership and past standings isolated",[]{
            auto s=world();finishUnitRegular(s);
            for(auto& f:s.fixtures){if(f.homeClubId==s.club.id){f.homeGoals=0;f.awayGoals=9;}if(f.awayClubId==s.club.id){f.homeGoals=9;f.awayGoals=0;}}
            SimulationEngine e(s);e.advanceDays(80);check(e.state().club.leagueName=="Lower","player not relegated");
            for(const auto& r:e.state().leagueStandings) if(r.seasonId=="upper-old" && r.clubId==s.club.id)check(r.points==0 && r.played==14,"old points mixed");
        }},
        {"legacy catch-up only matches on reload and no duplicate results",[]{
            auto s=world();s.currentDate={1993,6,28}; const auto cash=s.club.finance.cashPence;SimulationEngine e(s);
            check(e.state().club.finance.cashPence==cash && std::none_of(e.state().fixtures.begin(),e.state().fixtures.end(),[](const auto& f){return f.played;}),"constructor side effects");
            e.advanceDays(5);check(e.state().seasons.size()==2,"late fixture catch-up blocked");
            const auto old=e.state().seasonHistory.front().row.points;e.advanceDays(1);check(e.state().seasonHistory.front().row.points==old,"old results repeated");
        }},
        {"ambiguous multi-year legacy recovery held",[]{
            auto s=world();s.currentDate={1995,1,1};processSeasonLifecycle(s);
            check(s.seasons.size()==1 && !s.seasonBlockReason.empty(),"fabricated seasons");
        }},
        {"next season registration cutoff advances with calendar",[]{
            for(const auto date:{GameDate{1993,7,1},GameDate{1994,3,31},GameDate{1994,4,1}}){
                auto s=world();s.currentDate=date;SimulationEngine e(s);LoanTerms terms;
                terms.endDate=date.addDays(60);terms.loanFeePence=1700000;
                check(e.startIncomingLoan("dion-dublin",terms).success==(date.month!=4),"next-season registration helper mismatch");
            }
        }},
        {"loan return and obligation conversion survive the season boundary once",[]{
            for(const auto future:{std::string("none"),std::string("mandatory")}) {
                auto s=world();finishUnitRegular(s);s.currentDate={1993,3,1};
                for(auto& club:s.aiClubMarketStates)club.nextEvaluationDate={1995,1,1};
                s.club.finance.cashPence=2000000000;s.club.finance.transferBudgetPence=2000000000;
                SimulationEngine e(s);LoanTerms terms{20000000,100,"Squad Player",{1993,7,1},true,7,future,future=="none"?0:105000000,true,0,"loan_club"};
                const auto candidate=std::find_if(e.state().transferCandidates.begin(),e.state().transferCandidates.end(),[](const auto& c){return c.player.id=="dion-dublin";});
                check(candidate!=e.state().transferCandidates.end(),"loan candidate missing");
                // Match the existing seller valuation, replacing the option value with cash for a pure loan.
                terms.loanFeePence=std::max<Money>(100000,candidate->marketValuePence/50)+candidate->askingPricePence/5;
                check(e.startIncomingLoan("dion-dublin",terms).success,"boundary loan setup");e.advanceDays(4);
                auto loan=std::find_if(e.state().loanDeals.begin(),e.state().loanDeals.end(),[](const auto& v){return v.player.id=="dion-dublin";});
                check(loan!=e.state().loanDeals.end() && loan->stage==LoanStage::Active,
                    "boundary loan not active ("+future+"): "+(loan==e.state().loanDeals.end()?"missing":loanStageKey(loan->stage)+" / "+loan->responseNote));
                const auto id=loan->id;e.advanceDays(e.state().currentDate.daysUntil({1993,7,1}));
                check(e.state().seasons.size()==2,"loan rollover failed");
                loan=std::find_if(e.state().loanDeals.begin(),e.state().loanDeals.end(),[&](const auto& v){return v.id==id;});
                check(loan->stage==LoanStage::Completed && loan->actualEndDate==GameDate{1993,7,1},"loan did not close once");
                const bool player=std::any_of(e.state().players.begin(),e.state().players.end(),[](const auto& p){return p.id=="dion-dublin";});
                check(player==(future=="mandatory"),"loan player continuity wrong");
                const auto feeCount=std::count_if(e.state().transferPayments.begin(),e.state().transferPayments.end(),[&](const auto& p){return p.sourceId=="ai-loan-future-"+id;});
                check(feeCount==(future=="mandatory"?1:0),"future fee not unique");
                SimulationEngine reload(e.state());reload.advanceDays(1);
                check(std::count_if(reload.state().transferPayments.begin(),reload.state().transferPayments.end(),[&](const auto& p){return p.sourceId=="ai-loan-future-"+id;})==feeCount,"loan duplicate after reconstruction");
            }
        }},
        {"arrears and AI ledger are not reset at season rollover",[]{
            auto s=world();finishUnitRegular(s);s.currentDate={1993,6,28};SimulationEngine e(s);e.advanceDays(2);
            auto& m=e.mutableState();const auto ai=m.aiClubMarketStates.front().clubId;
            for(const auto& id:{m.club.id,ai}) {
                m.debts.push_back({"overdue-"+id,id,{},"test","bank_loan",12000,12000,1000,{1993,6,1},{},{},"active","test"});
                auto schedule=makeDebtRepayments("overdue-"+id,12000,1000,12,{1993,6,1});m.debtRepayments.insert(m.debtRepayments.end(),schedule.begin(),schedule.end());
            }
            m.club.finance.cashPence=0;m.aiClubMarketStates.front().finance.cashPence=0;
            e.advanceDays(1);check(e.state().seasons.size()==2,"arrears rollover");
            check(hasOverdueDebt(e.state(),m.club.id)&&hasOverdueDebt(e.state(),ai),"arrears reset");
            check(e.state().club.finance.cashPence==0,"new-season money invented");
            const auto events=e.state().inbox.size();settleDebtRepayments(e.mutableState());check(e.state().inbox.size()==events,"arrears repeat report");
        }},
        {"season boundary preserves financial schedules and one-time expiry",[]{
            SimulationEngine e(world());e.advanceDays(364);auto& s=e.mutableState();check(s.currentDate==GameDate{1993,6,30},"date mismatch");
            s.debts.push_back({"boundary-debt",s.club.id,{},"boundary test","bank_loan",12000,12000,1000,{1993,6,1},GameDate{1993,7,1},GameDate{1994,6,1},"active","test"});
            const auto schedule=makeDebtRepayments("boundary-debt",12000,1000,12,{1993,6,1});s.debtRepayments.insert(s.debtRepayments.end(),schedule.begin(),schedule.end());
            s.transferPayments.push_back({"boundary-payment","test",{},"test",TransferPaymentDirection::Receivable,"test",{1993,7,1},5000});
            s.club.finance.cashPence=1000000;const auto cash=s.club.finance.cashPence;
            if(!s.playerContracts.empty()){s.playerContracts.front().endDate={1993,6,30};s.playerContracts.front().expiryNotified=false;s.playerContracts.front().lifecycleStatus="active";}
            e.advanceDays(1);check(e.state().seasons.size()==2,"boundary rollover");
            check(e.state().club.finance.cashPence==cash+5000-schedule[0].principalPence-schedule[0].interestPence,"due order/cash mismatch");
            check(e.state().debtRepayments.front().settledDate==GameDate{1993,7,1},"debt due lost");
            check(e.state().transferPayments.back().status==TransferPaymentStatus::Settled,"payment lost");
            if(!e.state().playerContracts.empty()) check(e.state().playerContracts.front().expiryNotified,"expiry lost");
            const auto count=e.state().inbox.size();processSeasonLifecycle(e.mutableState());check(e.state().inbox.size()==count,"duplicate boundary report");
        }}
    };
    int failed=0;
    for(const auto& [name,run]:tests) try{run();std::cout<<"[PASS] season "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"[FAIL] season "<<name<<": "<<e.what()<<'\n';}
    std::cout<<tests.size()-static_cast<std::size_t>(failed)<<'/'<<tests.size()<<" season tests passed\n";
    return failed;
}
