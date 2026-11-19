#include "development_test_support.h"
#include "core/player_growth.h"
#include "core/staff_operations.h"
#include <functional>
#include <iostream>
#include <numeric>
using namespace development_test;
namespace {
GameState controlled() {
 auto s=basis();s.fixtures.clear();s.growthActivation.reset();
 for(auto& [id,d]:s.playerDevelopment){d.growth.reset();d.skills.assign(7,50);s.playerAbilities[id].current=50;s.playerAbilities[id].potential=90;}
 initializePlayerGrowth(s);return s;
}
void train(GameState& s,int n,int age=19,const std::string& position="CB",int minutes=0,bool eligible=true,const std::string& id="home-1",const std::string& club="home") {
 for(int i=0;i<n;++i){
    if(minutes && i%7==0)recordGrowthMinutes(s,id,club,s.currentDate,minutes);
    s.currentDate=s.currentDate.addDays(1);trainPlayerGrowth(s,id,club,position,age,eligible);
 }
}
int progress(const GameState& s,int skill,const std::string& id="home-1") {
 const auto& d=s.playerDevelopment.at(id);return (d.skills[skill]-50)*1000+d.growth->remainder[skill];
}
void coach(GameState& s,int ability){
 initializeStaffOperations(s);
 for(auto& [id,p]:s.staffPeople)p.coaching=ability;
 // Remove secondary-role load only in this controlled numerical fixture.
 for(auto& c:s.staffContracts)if(std::find(c.roles.begin(),c.roles.end(),"coach")!=c.roles.end())c.roles={"coach"};
}
}
int runPlayerGrowthTests(){
 const std::vector<std::pair<const char*,std::function<void()>>> tests={
 {"growth activation preserves skills CA PA and old fraction",[]{auto s=controlled();auto& d=s.playerDevelopment.at("home-1");d.progress=43;d.growth.reset();s.growthActivation.reset();const auto skills=d.skills;initializePlayerGrowth(s);check(d.skills==skills&&s.playerAbilities.at("home-1").current==50&&d.progress==43&&d.growth->remainder[0]==430,"legacy reset");}},
 {"growth initial validation",[]{auto s=controlled();check(validatePlayerGrowth(s).empty(),validatePlayerGrowth(s));}},
 {"growth initialization idempotent",[]{auto s=controlled();train(s,9);const auto before=development_test::snapshot(s);initializePlayerGrowth(s);check(before==development_test::snapshot(s),"init replay");}},
 {"defender prioritizes defending not finishing",[]{auto s=controlled();train(s,140);check(progress(s,3)>progress(s,2)&&s.playerDevelopment.at("home-1").skills[6]==50,"uniform development");}},
 {"striker prioritizes finishing not defending",[]{auto s=controlled();train(s,140,19,"ST");check(progress(s,2)>progress(s,3),"striker weights");}},
 {"midfielder prioritizes passing",[]{auto s=controlled();train(s,140,19,"CM");check(progress(s,1)>progress(s,3),"midfield weights");}},
 {"keeper develops goalkeeping",[]{auto s=controlled();train(s,140,19,"GK");check(progress(s,6)>progress(s,2)&&progress(s,6)>0,"GK weight");}},
 {"outfield never trains goalkeeping",[]{auto s=controlled();train(s,365,19,"ST");check(progress(s,6)==0,"outfield GK");}},
 {"under18 physical foundation faster than mature player",[]{auto a=controlled(),b=a;train(a,140,17);train(b,140,26);check(progress(a,4)>progress(b,4),"age curves");}},
 {"veteran judgement and physical follow different directions",[]{auto s=controlled();train(s,280,32);check(progress(s,4)<0&&progress(s,5)>0,"uniform ageing");}},
 {"goalkeeper physical ageing is delayed",[]{auto a=controlled(),b=a;train(a,140,31,"GK");train(b,140,31,"CB");check(progress(a,4)>progress(b,4),"GK age curve");}},
 {"unknown age does not fabricate progress",[]{auto s=controlled();for(int i=0;i<35;++i){s.currentDate=s.currentDate.addDays(1);trainPlayerGrowth(s,"home-1","home","CB",{},true);}check(progress(s,3)==0&&s.playerAbilities.at("home-1").current==50,"unknown age");}},
 {"playing 90 minutes beats bench and cameo",[]{auto a=controlled(),b=a,c=a;train(a,140,19,"CB",90);train(b,140,19,"CB",1);train(c,140);check(progress(a,3)>progress(b,3)&&progress(b,3)>=progress(c,3),"minutes ignored");}},
 {"weekly playing bonus is capped",[]{auto a=controlled(),b=a;for(int i=0;i<70;++i){for(int n=0;n<2;++n)recordGrowthMinutes(a,"home-1","home",a.currentDate,90);for(int n=0;n<3;++n)recordGrowthMinutes(b,"home-1","home",b.currentDate,90);a.currentDate=a.currentDate.addDays(1);b.currentDate=b.currentDate.addDays(1);trainPlayerGrowth(a,"home-1","home","CB",19,true);trainPlayerGrowth(b,"home-1","home","CB",19,true);}check(progress(a,3)==progress(b,3),"uncapped playing bonus");}},
 {"strong coaching improves development",[]{auto a=controlled(),b=a;coach(a,95);coach(b,25);train(a,140);train(b,140);check(progress(a,3)>progress(b,3),"coach not applied");}},
 {"role sharing reduces coaching contribution",[]{auto a=controlled(),b=a;coach(a,75);coach(b,75);for(auto& c:b.staffContracts)if(c.roles==std::vector<std::string>{"coach"})c.roles.push_back("manager");check(staffRoleAbility(a,"home","coach")>staffRoleAbility(b,"home","coach"),"workload missing");}},
 {"unavailable week cannot develop",[]{auto s=controlled();train(s,35,19,"CB",0,false);check(progress(s,3)==0,"injury gain");}},
 {"partial injury week loses training opportunity",[]{auto a=controlled(),b=a;train(a,7);train(b,6,19,"CB",0,false);train(b,1);check(progress(a,3)>progress(b,3)&&progress(b,3)>0,"last-day-only injury check");}},
 {"recovery training reduces progress",[]{auto a=controlled(),b=a;b.playerDevelopment.at("home-1").programme="recovery";train(a,140);train(b,140);check(progress(a,3)>progress(b,3),"recovery ignored");}},
 {"age decline persists during rehabilitation",[]{auto s=controlled();train(s,140,35,"CB",0,false);check(progress(s,4)<0,"age stopped by injury");}},
 {"CA respects potential and stops banking skill gain",[]{auto s=controlled();s.playerAbilities.at("home-1").potential=50;train(s,280);check(s.playerAbilities.at("home-1").current==50&&progress(s,3)==0,"PA escape");}},
 {"near ceiling slows positive progress",[]{auto a=controlled(),b=a;b.playerAbilities.at("home-1").potential=52;train(a,35);train(b,35);check(progress(a,3)>progress(b,3),"headroom ignored");}},
 {"high skill costs more to improve",[]{auto a=controlled(),b=a;b.playerDevelopment.at("home-1").skills[3]=90;train(a,7);train(b,7);check(a.playerDevelopment.at("home-1").growth->remainder[3]>b.playerDevelopment.at("home-1").growth->remainder[3],"skill cost");}},
 {"monthly report pending until full period",[]{auto s=controlled();train(s,27);check(s.playerDevelopment.at("home-1").growth->reports.empty(),"early report");train(s,1);check(s.playerDevelopment.at("home-1").growth->reports.size()==1,"missing report");}},
 {"unchanged skills cannot produce random reported gain",[]{auto s=controlled();s.playerAbilities.at("home-1").potential=50;train(s,56);for(const auto& r:s.playerDevelopment.at("home-1").growth->reports)check(r.before==r.after,"noise presented as growth");}},
 {"report has numeric per skill deltas",[]{auto s=controlled();s.playerDevelopment.at("home-1").skills[3]+=2;train(s,28,19,"CB",0,false);const auto& r=s.playerDevelopment.at("home-1").growth->reports.back();check(r.after[3]-r.before[3]==2&&r.after[2]==r.before[2],"delta mismatch");}},
 {"completed reports are immutable",[]{auto s=controlled();train(s,28);const auto old=s.playerDevelopment.at("home-1").growth->reports.front();train(s,28);const auto& r=s.playerDevelopment.at("home-1").growth->reports.front();check(old.after==r.after&&old.context==r.context,"old report rewritten");}},
 {"report exposes numeric context",[]{auto s=controlled();train(s,28,19,"CB",90);const auto& r=s.playerDevelopment.at("home-1").growth->reports.back();check(r.minutes==360&&r.coach==50&&r.context.find("360")!=std::string::npos,"context");}},
 {"club change starts new comparison without resetting ability",[]{auto s=controlled();train(s,28);const auto before=s.playerDevelopment.at("home-1").skills;train(s,1,19,"CB",0,true,"home-1","away");const auto& g=*s.playerDevelopment.at("home-1").growth;check(g.clubId=="away"&&g.periodStart==s.currentDate&&g.reports.size()==1&&before==s.playerDevelopment.at("home-1").skills,"transfer reset");}},
 {"reports retain latest24 without invalid state",[]{auto s=controlled();train(s,28*26);check(s.playerDevelopment.at("home-1").growth->reports.size()==24&&validatePlayerGrowth(s).empty(),validatePlayerGrowth(s));}},
 {"AI uses the same growth function",[]{auto s=controlled();train(s,140,19,"ST",90,true,"away-18","away");check(s.playerDevelopment.at("away-18").skills[2]>50,"AI growth missing");}},
 {"same seed gives same fractional progression",[]{auto a=controlled(),b=a;train(a,168);train(b,91);train(b,77);check(development_test::snapshot(a)==development_test::snapshot(b),"split divergence");}},
 {"minutes ledger records real appearance only once",[]{auto s=basis();play(s);int total=0;for(const auto& p:s.matchResults[0].home.players)if(p.appearance&&!p.synthetic){const auto& g=*s.playerDevelopment.at(p.playerId).growth;check(g.periodMinutes==p.minutes,"minutes ledger");total+=g.periodMinutes;}check(total>0,"no minutes");const auto before=development_test::snapshot(s);applyPlayerMatchDevelopment(s,s.matchResults[0]);check(before==development_test::snapshot(s),"minutes replay");}},
 {"legacy match minutes are not replayed",[]{auto s=fixture();play(s);initializePlayerDevelopment(s);applyPlayerMatchDevelopment(s,s.matchResults[0]);for(const auto& [id,d]:s.playerDevelopment)check(d.growth->periodMinutes==0,"legacy experience replay");}},
 {"growth rejects missing player payload",[]{auto s=controlled();s.playerDevelopment.at("home-1").growth.reset();check(!validatePlayerGrowth(s).empty(),"missing accepted");}},
 {"growth rejects fractional overflow",[]{auto s=controlled();s.playerDevelopment.at("home-1").growth->remainder[0]=1000;check(!validatePlayerGrowth(s).empty(),"overflow accepted");}},
 {"growth rejects future dates",[]{auto s=controlled();s.playerDevelopment.at("home-1").growth->periodStart=s.currentDate.addDays(1);check(!validatePlayerGrowth(s).empty(),"future accepted");}},
 {"growth rejects duplicate exposures",[]{auto s=controlled();auto& g=*s.playerDevelopment.at("home-1").growth;g.exposure={{s.currentDate,90},{s.currentDate,90}};check(!validatePlayerGrowth(s).empty(),"duplicate accepted");}},
 {"growth rejects malformed report shape",[]{auto s=controlled();train(s,28);s.playerDevelopment.at("home-1").growth->reports[0].after.pop_back();check(!validatePlayerGrowth(s).empty(),"report corruption");}}
 ,{"transfer assessment has exactly28 observed training days",[]{auto s=controlled();train(s,28);train(s,29,19,"CB",0,false,"home-1","away");const auto& r=s.playerDevelopment.at("home-1").growth->reports.back();check(r.restrictedDays==28&&validatePlayerGrowth(s).empty(),validatePlayerGrowth(s));}}
 ,{"adopted growth review follows new comparison date",[]{auto s=basis();s.fixtures.clear();days(s,10);s.growthActivation.reset();for(auto& [id,d]:s.playerDevelopment)d.growth.reset();initializePlayerGrowth(s);days(s,18);check(s.skillObservations.front().date<s.currentDate,"old schedule fired numeric report");days(s,10);check(s.playerDevelopment.at("home-1").growth->reports.size()==1&&s.skillObservations.front().date==s.currentDate,"new comparison review not connected");}}
 ,{"transfer day prunes old experience before new match",[]{auto s=controlled();for(int i=0;i<9;++i){s.currentDate=s.currentDate.addDays(1);const std::string club=i==8?"away":"home";trainPlayerGrowth(s,"home-1",club,"CB",19,true);recordGrowthMinutes(s,"home-1",club,s.currentDate,90);}check(validatePlayerGrowth(s).empty()&&s.playerDevelopment.at("home-1").growth->exposure.size()==8,validatePlayerGrowth(s));}}
 ,{"historical catch-up cannot grant retrospective growth minutes",[]{auto s=controlled();const auto old=s.currentDate;train(s,35);const auto before=development_test::snapshot(s);recordGrowthMinutes(s,"home-1","home",old,90);check(before==development_test::snapshot(s),"historical experience replay");}}
 ,{"controlled annual growth balance samples stay bounded",[]{for(const int age:{17,21,27,32,37}){auto s=controlled();int first=0;for(int day=1;day<=364;++day){train(s,1,age,"CB",day%7==1?90:0);if(!first&&s.playerDevelopment.at("home-1").skills!=std::vector<int>(7,50))first=day;}check(validatePlayerGrowth(s).empty(),validatePlayerGrowth(s));std::cout<<"[BALANCE] age="<<age<<" CB firstSkillDay="<<first<<" CA="<<s.playerAbilities.at("home-1").current<<" skills=";for(int value:s.playerDevelopment.at("home-1").skills)std::cout<<value<<',';std::cout<<'\n';}}}
 };
 int failed=0;for(const auto& [name,test]:tests)try{test();std::cout<<"[PASS] "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"[FAIL] "<<name<<": "<<e.what()<<'\n';}
 std::cout<<tests.size()-failed<<"/"<<tests.size()<<" player growth v2 tests passed\n";return failed;
}
