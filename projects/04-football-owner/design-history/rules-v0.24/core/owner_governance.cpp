#include "core/owner_governance.h"
#include "core/operating_finance.h"
#include "core/player_directory.h"
#include "core/staff_operations.h"
#include "core/season.h"
#include <algorithm>
#include <set>
#include <tuple>

namespace football_owner {
namespace {
bool oneOf(const std::string& v,std::initializer_list<const char*> list) {
    return std::any_of(list.begin(),list.end(),[&](auto x){return v==x;});
}
bool open(const OwnerCase& c){return c.status=="requested" || c.status=="promised";}
std::string relationKey(const std::string& club,const std::string& person){return club+"|"+person;}
FanTendencyState& fans(GameState& s,const std::string& club) {
    return club==s.club.id?s.fanTendency:s.governanceClubs.at(club).aiFans;
}
Money unpaidWages(const GameState& s,const std::string& club) {
    Money n=0;for(const auto& o:s.operatingObligations)
        if(o.clubId==club && o.category=="player_payroll" && o.status==TransferPaymentStatus::Overdue)n+=o.amountPence;
    return n;
}
void event(GameState& s,const std::string& club,const std::string& kind,const std::string& subject,
           const std::string& detail,int delta=0,bool notify=true) {
    const auto id="governance:"+std::to_string(++s.governanceRevision);
    auto& f=fans(s,club);const int old=f.trustScore;
    if(club==s.club.id) {
        OwnerActionImpact impact;impact.fanTrustDelta=delta;impact.reputationDelta=delta;impact.skepticismDelta=std::max(0,-delta)/2;
        recordOwnerAction(s,"governance_"+kind,"owner_governance",subject,subject,detail,impact,"owner_governance_v1_gameplay_not_historical");
    } else {
        f.trustScore=std::clamp(f.trustScore+delta,0,100);
        f.patienceScore=std::clamp(f.patienceScore+delta/2,0,100);
        f.skepticismScore=std::clamp(f.skepticismScore-delta/2,0,100);
        f.updatedDate=s.currentDate;f.mood=fanMoodKey(f);
    }
    s.governanceEvents.push_back({id,club,kind,subject,detail,s.currentDate,f.trustScore-old});
    if(club==s.club.id && notify) {
        int n=1;for(const auto& i:s.inbox)n=std::max(n,i.id+1);
        s.inbox.push_back({n,id,s.currentDate,InboxCategory::Board,"구단주 운영 · "+ownerCaseLabel(kind),
            detail+"\n구단주 홈 → 약속·팬·선수단에서 근거와 다음 행동을 확인하세요.",true,kind=="request"});
    }
}
void finish(GameState& s,OwnerCase& c,const std::string& status,const std::string& why) {
    c.status=status;c.resolvedDate=s.currentDate;c.response=why;
    // An administrative request timeout/withdrawal from another club is not a public broken promise.
    int awarded=0;for(const auto& e:s.governanceEvents)if(e.clubId==c.clubId && e.kind=="fulfilled" && e.date.daysUntil(s.currentDate)<28)awarded+=std::max(0,e.trustDelta);
    const int delta=status=="fulfilled"?std::clamp(4-awarded,0,2):status=="broken"?-4:0;
    auto r=s.playerRelationships.find(relationKey(c.clubId,c.subjectId));
    if(r!=s.playerRelationships.end())
        r->second.morale=std::clamp(r->second.morale+(status=="fulfilled"?8:status=="broken"?-(8+(100-r->second.patience)/10):status=="declined"?-4:0),0,100);
    event(s,c.clubId,status,c.id,c.subjectName+" · "+ownerCaseLabel(c.kind)+" · "+why,delta);
}
void ensureRelations(GameState& s,const std::map<std::string,DirectoryPlayer>& directory) {
    for(const auto& [id,p]:directory) {
        if(!s.governanceClubs.contains(p.activeClubId))continue;
        const auto key=relationKey(p.activeClubId,id);if(s.playerRelationships.contains(key))continue;
        PlayerRelationship r;r.playerId=id;r.clubId=p.activeClubId;r.initializedDate=r.reviewedDate=s.currentDate;
        r.patience=40+static_cast<int>(seasonStableHash(std::to_string(s.rngSeed)+"|temperament|"+id)%41);
        const int age=ageOnDate(p.player,s.currentDate).value_or(24);
        const int tenure=p.player.joinedDate?std::max(0,p.player.joinedDate->daysUntil(s.currentDate)/365):0;
        r.influence=std::clamp(10+std::max(0,age-18)*2+std::min(10,tenure)*3,10,95);
        s.playerRelationships.emplace(key,r);
    }
}
void refreshRelations(GameState& s,const std::string& club,const std::map<std::string,DirectoryPlayer>& directory,int result) {
    const auto wages=unpaidWages(s,club);
    for(const auto& [id,p]:directory) {
        if(p.activeClubId!=club)continue;
        auto& r=s.playerRelationships.at(relationKey(club,id));
        int minutes=0;
        for(const auto& match:s.matchResults) {
            const auto* competition=runtimeCompetition(s,match.seasonId);
            if(!s.activeSeasonId.empty() && (!competition || competition->careerSeasonId!=s.activeSeasonId))continue;
            for(const auto* team:{&match.home,&match.away})if(team->clubId==club)
                for(const auto& stat:team->players)if(stat.playerId==id)minutes+=stat.minutes;
        }
        const int age=ageOnDate(p.player,s.currentDate).value_or(24);
        const int tenure=p.player.joinedDate?std::max(0,p.player.joinedDate->daysUntil(s.currentDate)/365):0;
        r.influence=std::clamp(10+std::max(0,age-18)*2+std::min(10,tenure)*3+std::min(30,minutes/90),10,95);
        int target=50+result*2-(wages>0?12:0);
        for(const auto& c:s.ownerCases)if(c.clubId==club && c.subjectId==id && c.resolvedDate
            && c.resolvedDate->daysUntil(s.currentDate)<90)
            target+=c.status=="fulfilled"?6:c.status=="broken"?-12:c.status=="declined"?-3:0;
        target=std::clamp(target,15,85);
        r.morale+=std::clamp(target-r.morale,-8,5);r.reviewedDate=s.currentDate;
    }
}
bool recentCase(const GameState& s,const std::string& club,const std::string& kind,const std::string& person) {
    return std::any_of(s.ownerCases.begin(),s.ownerCases.end(),[&](const auto& c){
        return c.clubId==club && c.kind==kind && (kind=="pay_wages" || c.subjectId==person)
            && (open(c)||c.createdDate.daysUntil(s.currentDate)<180);
    });
}
void createRequest(GameState& s,const std::string& club,const std::map<std::string,DirectoryPlayer>& directory) {
    if(std::any_of(s.ownerCases.begin(),s.ownerCases.end(),[&](const auto& c){return c.clubId==club && c.status=="requested";}))return;
    const bool wages=unpaidWages(s,club)>0;
    for(const auto& [id,p]:directory) {
        if(p.activeClubId!=club || p.ownerClubId!=club || !p.contract)continue;
        const int remaining=s.currentDate.daysUntil(p.contract->endDate);
        const auto kind=wages?"pay_wages":"renew_player";
        if(!wages && (remaining<1 || remaining>120))continue;
        if(recentCase(s,club,kind,id))continue;
        // One club-wide wage request at a time, regardless of the spokesperson.
        if(wages && std::any_of(s.ownerCases.begin(),s.ownerCases.end(),[&](const auto& c){return c.clubId==club && c.kind==kind && open(c);}))return;
        OwnerCase c;c.id="owner-case:"+std::to_string(++s.governanceRevision);c.clubId=club;c.subjectId=id;
        c.subjectName=p.player.fullName;c.kind=kind;c.source="player_request";c.createdDate=s.currentDate;c.deadline=s.currentDate.addDays(14);
        c.baselineContractEnd=p.contract->endDate;
        c.reason=wages?"실제 미지급 선수 급여가 있습니다. 지급 방법은 기존 재정 화면에서 결정합니다.":
            "현재 고용 계약 만료가 120일 이내입니다. 계약을 최소 180일 연장하는 약속을 요청합니다.";
        s.ownerCases.push_back(c);event(s,club,"request",c.id,c.subjectName+" · "+c.reason);
        break;
    }
}
bool resolvedEvidence(const GameState& s,const OwnerCase& c,const std::map<std::string,DirectoryPlayer>& directory) {
    if(c.kind=="hire_medical")return staffForRole(s,c.clubId,"medical")!=nullptr;
    if(c.kind=="pay_wages")return unpaidWages(s,c.clubId)==0;
    if(c.kind=="renew_player") {
        const auto p=directory.find(c.subjectId);
        return p!=directory.end() && p->second.ownerClubId==c.clubId && p->second.contract && c.baselineContractEnd
            && p->second.contract->endDate>=c.baselineContractEnd->addDays(180);
    }
    return false;
}
}
const FanTendencyState* governanceFans(const GameState& s,const std::string& club) {
    const auto i=s.governanceClubs.find(club);return i==s.governanceClubs.end()?nullptr:club==s.club.id?&s.fanTendency:&i->second.aiFans;
}
std::string ownerCaseLabel(const std::string& k) {
    if(k=="renew_player")return "선수 계약 연장";if(k=="retain_player")return "선수 보유 유지";
    if(k=="hire_medical")return "의료 담당 채용";if(k=="pay_wages")return "미지급 선수 급여 정리";
    if(k=="keep_manager")return "현 감독 지지";if(k=="request")return "선수 요구";
    if(k=="fulfilled")return "약속 이행";if(k=="broken")return "약속 미이행";if(k=="declined")return "요구 거절";
    if(k=="cancelled")return "평가 취소";if(k=="expired")return "응답 기한 경과";if(k=="review")return "28일 운영 평가";
    if(k=="statement")return "대외 운영 방향";if(k=="promised")return "약속 등록";return k;
}
std::string ownerDirectionLabel(const std::string& k) {
    if(k=="youth")return "유망주 육성 중시";if(k=="stability")return "재정 안정 중시";return "신구 조화 중시";
}
void initializeOwnerGovernance(GameState& s) {
    if(s.governanceActivation)return;
    ensureDefaultOwnerReputation(s);s.governanceActivation=s.governanceProcessedDate=s.currentDate;
    std::set<std::string> clubs{s.club.id};for(const auto& c:s.aiClubMarketStates)clubs.insert(c.clubId);
    for(const auto& club:clubs)if(!club.empty() && operatingClubFinance(s,club)) {
        GovernanceClub g;g.nextReviewDate=s.currentDate.addDays(28);g.aiFans.updatedDate=s.currentDate;
        s.governanceClubs.emplace(club,g);
    }
    for(const auto& f:s.fixtures)if(f.played)s.governanceMatchIds.push_back(f.id);
    std::sort(s.governanceMatchIds.begin(),s.governanceMatchIds.end());
    s.governanceMatchIds.erase(std::unique(s.governanceMatchIds.begin(),s.governanceMatchIds.end()),s.governanceMatchIds.end());
    ensureRelations(s,playerDirectory(s,s.currentDate));
}
int fanMatchIncomePercent(const GameState& s,const std::string& club) {
    const auto p=s.governanceClubs.find(club);
    const auto* f=governanceFans(s,club);
    return p!=s.governanceClubs.end() && f && f->trustScore<35 && p->second.lowTrustSince
        && p->second.lowTrustSince->daysUntil(s.currentDate)>=56?90:100;
}
int playerMoraleAdjustment(const GameState& s,const std::string& person,const std::string& club) {
    const auto p=s.playerRelationships.find(relationKey(club,person));
    return p==s.playerRelationships.end()?0:std::clamp((p->second.morale-50)/15,-2,2);
}
std::string ownerCaseEligibility(const GameState& s,const std::string& club,const std::string& kind,const std::string& subject,int days) {
    if(!s.governanceActivation || !s.governanceClubs.contains(club))return "구단주 운영 정보가 없습니다.";
    if(days!=28 && days!=56 && days!=84)return "이행 기간은 28일·56일·84일 중에서 고르세요.";
    if(std::count_if(s.ownerCases.begin(),s.ownerCases.end(),[&](const auto& c){return c.clubId==club && c.status=="promised";})>=3)
        return "동시에 진행할 약속은 3개까지입니다. 기존 약속을 먼저 확인하세요.";
    if(!oneOf(kind,{"renew_player","retain_player","hire_medical","pay_wages","keep_manager"}))return "지원하지 않는 약속입니다.";
    if(recentCase(s,club,kind,subject))return "같은 약속/요구가 열려 있거나 180일 이내에 이미 다뤘습니다.";
    if(kind=="renew_player" || kind=="retain_player") {
        const auto p=directoryPlayer(s,subject,s.currentDate);
        if(!p || p->ownerClubId!=club || !p->contract || p->contract->endDate<s.currentDate)return "현재 우리 구단과 고용 계약이 있는 선수만 대상입니다.";
        if(kind=="renew_player" && p->contract->endDate<s.currentDate.addDays(days))return "계약이 약속 기한보다 먼저 끝납니다. 짧은 기간 또는 즉시 재계약을 선택하세요.";
    }
    if(kind=="hire_medical" && (!subject.empty() || staffForRole(s,club,"medical")))return "의료 담당 공석을 보강할 때만 약속할 수 있습니다.";
    if(kind=="pay_wages" && (!subject.empty() || unpaidWages(s,club)<=0))return "미지급 선수 급여가 있을 때 구단 전체에 약속할 수 있습니다.";
    if(kind=="keep_manager") {const auto* m=staffForRole(s,club,"manager");if(!m || m->id!=subject)return "현재 재직 감독을 선택하세요.";}
    return {};
}
bool makeOwnerPromise(GameState& s,const std::string& club,const std::string& kind,const std::string& subject,int days,std::string* error) {
    const auto reason=ownerCaseEligibility(s,club,kind,subject,days);if(!reason.empty()){if(error)*error=reason;return false;}
    OwnerCase c;c.id="owner-case:"+std::to_string(++s.governanceRevision);c.clubId=club;c.kind=kind;c.subjectId=subject;
    c.source="owner_promise";c.status="promised";c.createdDate=s.currentDate;c.deadline=s.currentDate.addDays(days);c.subjectName=ownerCaseLabel(kind);
    if(kind=="renew_player" || kind=="retain_player") {const auto p=directoryPlayer(s,subject,s.currentDate);c.subjectName=p->player.fullName;c.baselineContractEnd=p->contract->endDate;}
    if(kind=="keep_manager")c.subjectName=staffForRole(s,club,"manager")->name;
    c.reason="이행 기한 "+c.deadline.toIsoString()+". 선언만으로 돈·계약·신뢰가 바뀌지 않으며 실제 결과를 평가합니다.";
    s.ownerCases.push_back(c);event(s,club,"promised",c.id,c.subjectName+" · "+ownerCaseLabel(kind)+" · "+c.reason,0);return true;
}
bool respondOwnerCase(GameState& s,const std::string& id,bool promise,int days,const std::string& reason,std::string* error) {
    auto it=std::find_if(s.ownerCases.begin(),s.ownerCases.end(),[&](const auto& c){return c.id==id;});
    if(it==s.ownerCases.end() || it->status!="requested" || it->deadline<s.currentDate){if(error)*error="이미 처리됐거나 기한이 지난 요구입니다.";return false;}
    const auto dir=playerDirectory(s,s.currentDate);
    const auto p=dir.find(it->subjectId);
    if(p==dir.end() || p->second.ownerClubId!=it->clubId){if(error)*error="요구한 선수가 더 이상 우리 구단 소속이 아닙니다.";return false;}
    if(promise) {
        if(std::count_if(s.ownerCases.begin(),s.ownerCases.end(),[&](const auto& c){return c.clubId==it->clubId && c.status=="promised";})>=3){
            if(error)*error="동시에 진행할 약속은 3개까지입니다.";return false;}
        if(days!=28 && days!=56 && days!=84){if(error)*error="이행 기간은 28일·56일·84일입니다.";return false;}
        if(it->kind=="renew_player" && (!p->second.contract || p->second.contract->endDate<s.currentDate.addDays(days))){
            if(error)*error="약속 기한 전에 계약이 끝납니다. 더 짧은 기간 또는 즉시 재계약을 검토하세요.";return false;}
        it->status="promised";it->deadline=s.currentDate.addDays(days);it->response="요구를 약속으로 수락했습니다. 실제 결과로만 평가합니다.";
        event(s,it->clubId,"promised",it->id,it->subjectName+" · "+it->response);return true;
    }
    if(!oneOf(reason,{"budget","no_guarantee","direction"})){if(error)*error="예산·보장 불가·장기 방향 중 사유를 고르세요.";return false;}
    const auto* finance=operatingClubFinance(s,it->clubId);
    const bool justified=reason=="budget" && finance && finance->cashPence<=operatingLiquidityReserve(s,it->clubId);
    finish(s,*it,"declined",justified?"실제 재정 제약을 설명했습니다.":"요구를 거절했습니다. 선수는 결정에 불만을 가질 수 있습니다.");
    auto r=s.playerRelationships.find(relationKey(it->clubId,it->subjectId));
    if(justified && r!=s.playerRelationships.end())r->second.morale=std::min(100,r->second.morale+3);
    return true;
}
bool withdrawOwnerPromise(GameState& s,const std::string& id,std::string* error) {
    auto it=std::find_if(s.ownerCases.begin(),s.ownerCases.end(),[&](const auto& c){return c.id==id;});
    if(it==s.ownerCases.end() || it->status!="promised"){if(error)*error="진행 중인 약속만 철회할 수 있습니다.";return false;}
    finish(s,*it,"broken","구단주가 약속을 철회했습니다. 신뢰 하락은 한 번만 반영합니다.");return true;
}
bool declareOwnerDirection(GameState& s,const std::string& club,const std::string& direction,std::string* error) {
    auto i=s.governanceClubs.find(club);
    if(i==s.governanceClubs.end() || !oneOf(direction,{"balanced","youth","stability"})){if(error)*error="운영 방향을 다시 선택하세요.";return false;}
    auto& g=i->second;
    if(g.lastStatementDate && g.lastStatementDate->daysUntil(s.currentDate)<7){if(error)*error="공개 설명은 7일 간격으로 할 수 있습니다. 같은 말을 반복해 신뢰를 올리지 않습니다.";return false;}
    if(g.lastStatementDate && direction==g.direction){if(error)*error="이미 같은 운영 방향을 밝혔습니다. 실제 행동으로 평가받습니다.";return false;}
    const int penalty=g.lastStatementDate && g.lastStatementDate->daysUntil(s.currentDate)<84?-2:0;
    g.direction=direction;g.lastStatementDate=s.currentDate;
    event(s,club,"statement",club,ownerDirectionLabel(direction)+" 방향을 공개했습니다. 실제 영입/기용은 별개이며 선언 자체의 보상은 없습니다."
          +(penalty?" 84일 안의 방향 변경으로 일관성에 의문이 생겼습니다.":std::string()),penalty);
    return true;
}
void processOwnerGovernanceDay(GameState& s) {
    initializeOwnerGovernance(s);if(*s.governanceProcessedDate>=s.currentDate)return;
    const auto dir=playerDirectory(s,s.currentDate);ensureRelations(s,dir);
    std::set<std::string> seen(s.governanceMatchIds.begin(),s.governanceMatchIds.end());
    for(const auto& f:s.fixtures)if(f.played && f.date<=s.currentDate && seen.insert(f.id).second) {
        s.governanceMatchIds.push_back(f.id);if(f.date<=*s.governanceActivation)continue;
        for(const auto& club:{f.homeClubId,f.awayClubId}) {
            auto i=s.governanceClubs.find(club);if(i==s.governanceClubs.end())continue;
            const int own=club==f.homeClubId?f.homeGoals:f.awayGoals,other=club==f.homeClubId?f.awayGoals:f.homeGoals;
            ++i->second.matches;i->second.points+=own>other?3:own==other?1:0;
        }
    }
    for(auto& c:s.ownerCases)if(open(c)) {
        const auto p=dir.find(c.subjectId);
        const bool left=(c.kind=="renew_player" || c.kind=="retain_player" || (c.kind=="pay_wages"&&!c.subjectId.empty()))
            && (p==dir.end() || p->second.ownerClubId!=c.clubId);
        if(left){finish(s,c,c.status=="promised"&&c.kind!="pay_wages"?"broken":"cancelled","선수 고용 소속이 바뀌었습니다.");continue;}
        if(c.status=="requested") {
            if(resolvedEvidence(s,c,dir)){finish(s,c,"cancelled","요구가 실제 실행으로 해결되어 약속 평가 없이 닫았습니다.");continue;}
            if(c.deadline<s.currentDate)finish(s,c,"expired","응답 기한이 지났습니다. 자동 계약이나 출전 보장은 없습니다.");
            continue;
        }
        if(c.kind=="keep_manager" && (!staffForRole(s,c.clubId,"manager") || staffForRole(s,c.clubId,"manager")->id!=c.subjectId)) {
            bool ownerExit=false;for(const auto& contract:s.staffContracts)if(contract.personId==c.subjectId && contract.clubId==c.clubId
                && oneOf(contract.status,{"dismissed","replaced"}) && contract.endedDate && *contract.endedDate>=c.createdDate)ownerExit=true;
            finish(s,c,ownerExit?"broken":"cancelled",ownerExit?"약속 기간 중 감독을 교체했습니다.":"감독의 사임·만료 등으로 지지 약속 평가를 취소합니다.");continue;
        }
        if(s.currentDate<=c.deadline && resolvedEvidence(s,c,dir))finish(s,c,"fulfilled","실제 계약·채용·지급 결과를 확인했습니다.");
        else if(c.deadline<=s.currentDate)finish(s,c,(c.kind=="retain_player"||c.kind=="keep_manager")?"fulfilled":"broken",
            (c.kind=="retain_player"||c.kind=="keep_manager")?"약속 기간 동안 보유·지지를 유지했습니다.":"기한까지 약속한 결과가 확인되지 않았습니다.");
    }
    for(auto& [club,g]:s.governanceClubs) {
        auto& f=fans(s,club);
        if(f.trustScore<25 && !g.lowTrustSince)g.lowTrustSince=s.currentDate;
        if(f.trustScore>=35)g.lowTrustSince.reset();
        if(g.nextReviewDate>s.currentDate)continue;
        const int result=g.matches?g.points>=g.matches*2?3:g.points<g.matches?-3:0:0;
        const int delta=result>0?std::max(1,result-f.skepticismScore/40):result<0?-std::max(1,-result-f.patienceScore/40):0;
        int young=0,veteran=0,total=0;for(const auto& [id,p]:dir)if(p.activeClubId==club){
            ++total;if(const auto age=ageOnDate(p.player,s.currentDate)){if(*age<23)++young;if(*age>=30)++veteran;}
        }
        if(result>0 && total) {
            if(young*100/total>=30)f.youthDevelopmentPreference=std::min(100,f.youthDevelopmentPreference+1);
            if(young*100/total>=20 && veteran*100/total>=20)f.balancedSquadPreference=std::min(100,f.balancedSquadPreference+1);
        }
        refreshRelations(s,club,dir,result);
        const auto summary=std::to_string(g.matches)+"경기 · "+std::to_string(g.points)+"점. 신뢰 변화 "
            +(delta>0?"+":"")+std::to_string(delta)+". 기존 인내 "+std::to_string(f.patienceScore)+"/의심 "+std::to_string(f.skepticismScore)
            +"을 반영했습니다. 공표 방향: "+ownerDirectionLabel(g.direction)+". 모델 수입 비율 "+std::to_string(fanMatchIncomePercent(s,club))+"%.";
        event(s,club,"review",club,summary,delta,g.matches>0 || unpaidWages(s,club)>0);
        createRequest(s,club,dir);g.matches=g.points=0;g.nextReviewDate=s.currentDate.addDays(28);
    }
    // AI makes the same explicit commitments; no hidden wage payment or contract rewrite.
    std::vector<std::string> requests;
    for(const auto& c:s.ownerCases)if(c.clubId!=s.club.id && c.status=="requested")requests.push_back(c.id);
    for(const auto& id:requests) {
        const auto it=std::find_if(s.ownerCases.begin(),s.ownerCases.end(),[&](const auto& c){return c.id==id;});
        const auto* f=operatingClubFinance(s,it->clubId);
        const bool affordable=f && f->cashPence>operatingLiquidityReserve(s,it->clubId)+unpaidWages(s,it->clubId);
        std::string error;if(!respondOwnerCase(s,id,affordable,28,"budget",&error)) (void)respondOwnerCase(s,id,false,28,"no_guarantee");
    }
    s.governanceProcessedDate=s.currentDate;++s.governanceRevision;
}
std::optional<GameDate> nextOwnerGovernanceDate(const GameState& s) {
    std::optional<GameDate> d;
    const auto add=[&](GameDate n){if(n<=s.currentDate)n=s.currentDate.addDays(1);if(!d||n<*d)d=n;};
    const auto i=s.governanceClubs.find(s.club.id);if(i!=s.governanceClubs.end())add(i->second.nextReviewDate);
    for(const auto& c:s.ownerCases)if(c.clubId==s.club.id && open(c))add(c.deadline.addDays(c.status=="requested"?1:0));
    return d;
}
std::string validateOwnerGovernance(const GameState& s) {
    if(!s.governanceActivation || !s.governanceProcessedDate || !s.governanceActivation->isValid() || !s.governanceProcessedDate->isValid()
        || *s.governanceActivation>*s.governanceProcessedDate || *s.governanceProcessedDate>s.currentDate || s.governanceRevision<0
        || !s.governanceClubs.contains(s.club.id))return "Invalid owner governance clock";
    for(const auto& [id,g]:s.governanceClubs) {
        if(!operatingClubFinance(s,id) || !oneOf(g.direction,{"balanced","youth","stability"}) || !g.nextReviewDate.isValid()
            || g.matches<0 || g.points<0 || g.points>g.matches*3)return "Invalid governance club";
        for(auto d:{g.lastStatementDate,g.lowTrustSince})if(d && (!d->isValid() || *d>s.currentDate))return "Invalid governance club date";
        for(int n:{g.aiFans.trustScore,g.aiFans.patienceScore,g.aiFans.skepticismScore,g.aiFans.starSigningPreference,
            g.aiFans.homegrownPreference,g.aiFans.youthDevelopmentPreference,g.aiFans.balancedSquadPreference})
            if(n<0 || n>100)return "Invalid AI fan value";
    }
    for(const auto& [key,r]:s.playerRelationships)
        if(key!=relationKey(r.clubId,r.playerId) || r.playerId.empty() || !s.governanceClubs.contains(r.clubId)
            || r.morale<0 || r.morale>100 || r.influence<0 || r.influence>100 || r.patience<0 || r.patience>100
            || !r.initializedDate.isValid() || !r.reviewedDate.isValid() || r.initializedDate>r.reviewedDate || r.reviewedDate>s.currentDate)
            return "Invalid player relationship";
    std::set<std::string> ids;
    for(const auto& c:s.ownerCases) {
        if(c.id.empty() || !ids.insert(c.id).second || !s.governanceClubs.contains(c.clubId) || c.subjectName.empty()
            || !oneOf(c.kind,{"renew_player","retain_player","hire_medical","pay_wages","keep_manager"})
            || !oneOf(c.source,{"player_request","owner_promise","hidden_risk"})
            || (c.source=="hidden_risk" && (c.kind!="retain_player" || c.subjectId.empty()))
            || !oneOf(c.status,{"requested","promised","fulfilled","broken","declined","expired","cancelled"})
            || !c.createdDate.isValid() || !c.deadline.isValid() || c.createdDate>s.currentDate || c.deadline<c.createdDate
            || open(c)==c.resolvedDate.has_value() || (c.resolvedDate && (!c.resolvedDate->isValid() || *c.resolvedDate<c.createdDate || *c.resolvedDate>s.currentDate))
            || (c.baselineContractEnd && !c.baselineContractEnd->isValid())
            || ((c.kind=="renew_player" || c.kind=="retain_player") && (c.subjectId.empty() || !c.baselineContractEnd))
            || (c.kind=="keep_manager" && c.subjectId.empty())
            || (c.kind=="hire_medical" && !c.subjectId.empty())
            || (c.source=="player_request" && (c.subjectId.empty() || !oneOf(c.kind,{"renew_player","pay_wages"})))
            || (c.source=="owner_promise" && c.status=="requested"))return "Invalid owner case";
    }
    ids.clear();for(const auto& e:s.governanceEvents)if(e.id.empty() || !ids.insert(e.id).second || !s.governanceClubs.contains(e.clubId)
        || !e.date.isValid() || e.date>s.currentDate || e.trustDelta<-100 || e.trustDelta>100)return "Invalid governance event";
    ids.clear();for(const auto& id:s.governanceMatchIds)if(id.empty()||!ids.insert(id).second)return "Duplicate governance match";
    return {};
}
}
