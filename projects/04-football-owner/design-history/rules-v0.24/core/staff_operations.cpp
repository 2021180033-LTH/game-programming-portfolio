#include "core/staff_operations.h"
#include "core/operating_finance.h"
#include "core/funding.h"
#include "core/season.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <tuple>

namespace football_owner {
namespace {
constexpr Money moneyLimit=1'000'000'000LL;
bool has(const std::vector<std::string>& v,const std::string& x){return std::find(v.begin(),v.end(),x)!=v.end();}
bool active(const StaffContract& c,const GameDate& d){return c.status=="active" && c.startDate<=d && (!c.endDate || d<=*c.endDate);}
std::string join(const std::vector<std::string>& v){std::string out;for(const auto& x:v){if(!out.empty())out+=" / ";out+=staffRoleLabel(x);}return out;}
void notice(GameState& s,const std::string& id,const std::string& title,const std::string& body){
    if(std::any_of(s.inbox.begin(),s.inbox.end(),[&](const auto& i){return i.eventKey==id;}))return;
    int n=1;for(const auto& i:s.inbox)n=std::max(n,i.id+1);
    s.inbox.push_back({n,id,s.currentDate,InboxCategory::Board,title,body,true,false});
}
void event(GameState& s,const std::string& club,const std::string& person,const std::string& kind,const std::string& detail,Money amount=0){
    const auto id="staff-event:"+std::to_string(++s.staffRevision);
    s.staffEvents.push_back({id,club,person,kind,detail,s.currentDate,amount});
    if(club==s.club.id)notice(s,id,"직원 운영 · "+kind,detail+"\n감독 → 직원 운영에서 계약·비용·요청을 확인하세요.");
}
void charge(GameState& s,const std::string& club,Money amount,const std::string& kind,const std::string& source){
    if(amount<=0)return;
    operatingClubFinance(s,club)->cashPence-=amount;
    s.operatingTransactions.push_back({"staff-cash:"+std::to_string(s.staffRevision),club,s.activeSeasonId,s.currentDate,kind,
        "outflow",amount,source,"staff_v1_gameplay_not_historical",kind,true});
}
Money severance(const GameState& s,const StaffContract& c){
    if(c.legacyTerms || !c.endDate)return 0; // Never invent an unknown historic cancellation clause.
    return c.weeklyPence*std::max(0,s.currentDate.daysUntil(*c.endDate)+1)/7;
}
void endContract(GameState& s,StaffContract& c,const std::string& why){
    c.status=why;c.endedDate=s.currentDate;
    const auto& p=s.staffPeople.at(c.personId);
    if(c.clubId==s.club.id && has(c.roles,"scout")){
        for(auto& a:s.scoutingAssignments)if(a.scoutId==p.scoutId && a.status==ScoutingAssignmentStatus::Active){
            a.status=ScoutingAssignmentStatus::Cancelled;a.completedDate=s.currentDate;
        }
        for(auto& d:s.scoutingDirectives)if(d.scoutId==p.scoutId && d.status==ScoutingDirectiveStatus::Active){
            d.status=ScoutingDirectiveStatus::Cancelled;d.completedDate=s.currentDate;d.summary+=" 담당자 계약 종료. 기존 보고는 유지됩니다.";
        }
        for(auto& m:s.scoutingMissions)if(m.scoutId==p.scoutId && m.status=="active"){
            m.status="paused";m.note="담당자 계약 종료 · 다른 스카우터에게 재배정하세요.";
        }
    }
    if(has(c.roles,"manager")){
        for(auto& r:s.staffRequests)if(r.managerId==c.personId && r.clubId==c.clubId && r.status=="pending"){
            r.status="cancelled";r.reason="요청 감독 이탈";r.decidedDate=s.currentDate;
        }
        s.staffPolicies.at(c.clubId).resignationDate.reset();
    }
    projectStaffRoles(s);
}
const StaffPerson* candidateFor(const GameState& s,const std::string& role){
    const StaffPerson* best=nullptr;
    for(const auto& [id,p]:s.staffPeople){
        if(!has(p.qualifiedRoles,role) || activeStaffContract(s,id))continue;
        // Keep the player's pending named request from being silently poached by AI.
        if(std::any_of(s.staffRequests.begin(),s.staffRequests.end(),[&](const auto& r){return r.clubId==s.club.id && r.status=="pending" && r.candidateId==id;}))continue;
        if(!best || std::tie(p.askingWeeklyPence,p.id)<std::tie(best->askingWeeklyPence,best->id))best=&p;
    }
    return best;
}
void requestReview(GameState& s,const std::string& club){
    auto& policy=s.staffPolicies.at(club);
    if(s.currentDate<policy.nextReviewDate)return;
    policy.nextReviewDate=s.currentDate.addDays(28);
    const auto* manager=staffForRole(s,club,"manager");if(!manager)return;
    for(const auto& role:{"medical","coach","scout","director","finance"}){
        if(staffForRole(s,club,role))continue;
        if(std::any_of(s.staffRequests.begin(),s.staffRequests.end(),[&](const auto& r){
            return r.clubId==club && r.managerId==manager->id && r.role==role &&
                (r.status=="pending" || r.createdDate.daysUntil(s.currentDate)<90);
        }))continue;
        const auto* candidate=candidateFor(s,role);if(!candidate)continue;
        StaffRequest r;r.id="staff-request:"+std::to_string(++s.staffRevision);r.clubId=club;r.managerId=manager->id;
        r.candidateId=candidate->id;r.role=role;r.kind=manager->authority>=70?"approval":"recruitment";
        r.createdDate=s.currentDate;r.deadline=s.currentDate.addDays(28);
        r.reason=staffRoleLabel(role)+" 전담 인력이 없습니다. "+candidate->name+" 검토 요청.";
        s.staffRequests.push_back(r);
        if(club==s.club.id)notice(s,r.id,manager->name+" · 직원 "+(r.kind=="approval"?"승인 요청":"영입 의뢰"),r.reason+" 자동 영입하지 않습니다.");
        break; // One comprehensible new decision per review.
    }
}
}
std::string staffRoleLabel(const std::string& r){
    if(r=="manager")return "감독";if(r=="coach")return "코치";if(r=="scout")return "스카우트";
    if(r=="director")return "단장";if(r=="medical")return "의료";if(r=="finance")return "재정";
    return r;
}
const StaffContract* activeStaffContract(const GameState& s,const std::string& person){
    for(const auto& c:s.staffContracts)if(c.personId==person && active(c,s.currentDate))return &c;
    return nullptr;
}
const StaffPerson* staffForRole(const GameState& s,const std::string& club,const std::string& role){
    const StaffPerson* best=nullptr;
    for(const auto& c:s.staffContracts)if(c.clubId==club && active(c,s.currentDate) && has(c.roles,role)){
        const auto p=s.staffPeople.find(c.personId);if(p==s.staffPeople.end())continue;
        if(!best || p->first<best->id)best=&p->second;
    }
    return best;
}
int staffRoleAbility(const GameState& s,const std::string& club,const std::string& role){
    if(!s.staffActivation)return 50; // Previous model baseline.
    int best=role=="coach"?40:role=="medical"?40:50;
    for(const auto& c:s.staffContracts)if(c.clubId==club && active(c,s.currentDate) && has(c.roles,role)){
        const auto& p=s.staffPeople.at(c.personId);
        const int raw=role=="medical"?p.medical:role=="coach"?p.coaching:p.judging;
        best=std::max(best,raw-static_cast<int>(c.roles.size()-1)*5);
    }
    return best;
}
Money staffWeeklyPayroll(const GameState& s,const std::string& club){
    Money sum=0;for(const auto& c:s.staffContracts)if(c.clubId==club && active(c,s.currentDate) && !c.legacyTerms)sum+=c.weeklyPence;return sum;
}
Money staffCashReserve(const GameState& s,const std::string& club,int days){
    const auto it=s.staffPolicies.find(club);if(it==s.staffPolicies.end())return 0;
    const auto& p=it->second;
    return p.accruedPence+(staffWeeklyPayroll(s,club)*std::clamp(days,0,730)+p.remainder)/7;
}
void initializeStaffOperations(GameState& s){
    if(s.staffActivation)return;
    s.staffActivation=s.staffProcessedDate=s.currentDate;
    std::set<std::string> clubs{s.club.id};for(const auto& a:s.aiClubMarketStates)clubs.insert(a.clubId);
    for(const auto& club:clubs){
        if(club.empty())continue;
        const auto* f=operatingClubFinance(s,club);if(!f)continue;
        StaffPolicy policy;policy.weeklyLimitPence=std::max<Money>(50'000,f->weeklyWageCommitmentPence/8);
        policy.nextReviewDate=s.currentDate.addDays(28);policy.nextPayDate=s.currentDate.addDays(7);policy.accruedDate=s.currentDate;
        s.staffPolicies.emplace(club,policy);
        StaffPerson p;p.id="staff-opening-manager:"+club;p.name="기존 감독 역할 (게임 모델)";p.identity="balanced";
        p.provenance="opening_manager_identity;attributes_terms_gameplay_unknown";p.qualifiedRoles={"manager","coach"};
        for(const auto& m:s.matchManagers)if(m.clubId==club){p.name=m.name;p.identity=m.tacticalIdentity;}
        if(club==s.club.id){p.name=s.club.manager.name;p.identity=s.club.manager.tacticalIdentity;}
        if(p.name.empty())p.name="감독 역할 대행 (게임 모델)";
        p.coaching=55;p.askingWeeklyPence=30'000;
        p.authority=40+static_cast<int>(seasonStableHash(club+"staff-authority")%41);
        s.staffPeople.emplace(p.id,p);
        s.staffContracts.push_back({"staff-opening:"+club,p.id,club,{"manager","coach"},s.currentDate,{},{},0,"active",true});
    }
    for(const auto& scout:s.scouts){
        StaffPerson p;p.id="staff-opening-scout:"+scout.id;p.scoutId=scout.id;p.name=scout.fullName;
        p.provenance="opening_scout_profile;terms_unknown";p.qualifiedRoles={"scout","director"};
        p.judging=scout.judgingCurrentAbility;p.potential=scout.judgingPotential;p.adaptability=scout.adaptability;
        p.capacity=scout.assignmentCapacity;p.region=scout.regionSpecialism;p.askingWeeklyPence=15'000;
        s.staffPeople.emplace(p.id,p);
        s.staffContracts.push_back({"staff-opening:"+scout.id,p.id,s.club.id,{"scout"},s.currentDate,{},{},0,"active",true});
    }
    // Explicit fictional candidates; no invented historic names or guessed real-person IDs.
    for(const auto& role:{"manager","coach","medical","scout","director","finance"})for(int n=1;n<=12;++n){
        StaffPerson p;p.id="staff-model:"+std::string(role)+":"+std::to_string(n);
        p.name="가상 "+staffRoleLabel(role)+" 후보 "+std::to_string(n);
        p.provenance="staff_v1_generated_candidate_not_historical";
        p.qualifiedRoles={role};if(std::string(role)=="manager")p.qualifiedRoles.push_back("coach");
        if(std::string(role)=="director")p.qualifiedRoles.push_back("scout");
        p.scoutId=has(p.qualifiedRoles,"scout")?p.id:"";p.identity=n%3==0?"attacking":n%3==1?"balanced":"defensive";
        const auto seed=seasonStableHash(std::to_string(s.rngSeed)+"|"+p.id);
        p.coaching=45+seed%46;p.medical=45+(seed/47)%46;p.judging=45+(seed/97)%46;
        p.potential=p.judging;p.authority=40+(seed/131)%46;p.patience=30+(seed/197)%56;
        p.askingWeeklyPence=(std::string(role)=="manager"?30'000:10'000)+n*2'000;
        s.staffPeople.emplace(p.id,p);
    }
    // Do not replay overdue meetings from an older save.
    if(s.operatingMeetingSettings.nextMeetingDate && *s.operatingMeetingSettings.nextMeetingDate<=s.currentDate)
        s.operatingMeetingSettings.nextMeetingDate=nextStaffMeetingDate(s.currentDate,s.operatingMeetingSettings.cadence);
}
void projectStaffRoles(GameState& s){
    if(!s.staffActivation)return;
    if(const auto* p=staffForRole(s,s.club.id,"manager")){
        s.club.manager.name=p->name;s.club.manager.tacticalIdentity=p->identity;
    }else{s.club.manager.name="감독 공석 · 역할 대행";s.club.manager.tacticalIdentity="balanced";}
    // Rebuild active scouts, retaining durable archived reports/familiarities in their own ledgers.
    const auto previous=s.scouts;
    s.scouts.clear();
    std::vector<std::string> order;
    for(const auto& scout:previous)for(const auto& [id,p]:s.staffPeople)if(p.scoutId==scout.id)order.push_back(id);
    for(const auto& [id,p]:s.staffPeople)if(std::find(order.begin(),order.end(),id)==order.end())order.push_back(id);
    for(const auto& id:order){
        const auto& p=s.staffPeople.at(id);
        const auto* c=activeStaffContract(s,id);
        if(!c || c->clubId!=s.club.id || !has(c->roles,"scout"))continue;
        const auto old=std::find_if(previous.begin(),previous.end(),[&](const auto& v){return v.id==p.scoutId;});
        s.scouts.push_back({p.scoutId,p.name,old==previous.end()?"Scout":old->role,p.region,
            std::max(0,p.judging-static_cast<int>(c->roles.size()-1)*5),p.potential,p.adaptability,p.capacity,p.provenance});
    }
}
StaffQuote quoteStaffAction(const GameState& s,const std::string& club,const std::string& person,const std::string& action,
                           std::vector<std::string> roles,int years){
    StaffQuote q;q.action=action;q.personId=person;q.clubId=club;q.years=years;
    const auto fail=[&](const std::string& reason){q.reason=reason;return q;};
    const auto p=s.staffPeople.find(person),end=s.staffPeople.end();
    if(!s.staffActivation || p==end || !s.staffPolicies.contains(club) || !operatingProfile(s,club))return fail("직원 또는 구단 운영 정보가 없습니다.");
    const auto* old=activeStaffContract(s,person);
    if(action!="hire" && action!="renew" && action!="dismiss" && action!="roles")return fail("지원하지 않는 직원 작업입니다.");
    if(action=="hire" && old)return fail("이미 다른 계약이 있습니다. 계약 중인 직원 빼오기는 아직 지원하지 않습니다.");
    if(action!="hire" && (!old || old->clubId!=club))return fail("우리 구단의 활성 계약이 아닙니다.");
    if(action=="dismiss"){
        q.compensationPence=severance(s,*old);q.payNowPence=q.compensationPence;q.roles=old->roles;
    }else{
        if(action=="renew")roles=old->roles;
        std::sort(roles.begin(),roles.end());
        if(roles.empty() || roles.size()>2 || std::adjacent_find(roles.begin(),roles.end())!=roles.end())return fail("담당 역할은 중복 없이 1~2개를 고르세요.");
        for(const auto& role:roles)if(!has(p->second.qualifiedRoles,role))return fail("이 직원이 맡을 수 없는 역할입니다.");
        if(action=="roles" && has(old->roles,"manager")!=has(roles,"manager"))return fail("감독 변경은 선임·해임으로 진행하세요.");
        q.roles=roles;q.weeklyPence=action=="roles"?old->weeklyPence:p->second.askingWeeklyPence;
        if(action!="roles"){
            if(years<1 || years>3)return fail("계약 기간은 1~3년입니다.");
            q.payNowPence=q.weeklyPence*4;
        }
        for(const auto& c:s.staffContracts)if(c.clubId==club && active(c,s.currentDate) && c.personId!=person && has(c.roles,"manager") && has(roles,"manager")){
            if(action!="hire")return fail("감독은 한 명만 선임할 수 있습니다.");
            q.replacedContract=c.id;q.compensationPence=severance(s,c);q.payNowPence+=q.compensationPence;
        }
        const Money removed=old&&!old->legacyTerms?old->weeklyPence:0;
        Money replaced=0;for(const auto& c:s.staffContracts)if(c.id==q.replacedContract && !c.legacyTerms)replaced=c.weeklyPence;
        if(staffWeeklyPayroll(s,club)-removed-replaced+q.weeklyPence>s.staffPolicies.at(club).weeklyLimitPence)
            return fail("직원 주급 한도를 넘습니다. 기존 비용과 한도를 먼저 확인하세요.");
    }
    const auto* f=operatingClubFinance(s,club);
    const Money free=club==s.club.id?availableTransferCash(s):
        std::max<Money>(0,f->cashPence-operatingLiquidityReserve(s,club)-clubTransferPayables(s,club)-aiReservedMarketCommitments(s,club)-debtLiquidityReserve(s,club));
    if(q.payNowPence>free || (action=="hire" && q.payNowPence+q.weeklyPence*4>free))
        return fail("급여·부채·거래 약정 제외 후 사용 가능 "+formatMoney(free)+" / 필요 "+formatMoney(q.payNowPence+(action=="hire"?q.weeklyPence*4:0))+". 현금이 부족합니다. 차입으로 자동 실행하지 않습니다.");
    q.allowed=true;
    q.token=std::to_string(s.staffRevision)+"|"+s.currentDate.toIsoString()+"|"+club+"|"+person+"|"+action+"|"+join(q.roles)+"|"+std::to_string(years)
        +"|"+std::to_string(q.payNowPence)+"|"+std::to_string(q.weeklyPence);
    q.reason=action=="dismiss"?(old->legacyTerms?"기존 계약의 급여·해임 조항 미상: 추가 보상은 이 v1에서 청구하지 않는 임시 정책입니다.":"남은 계약 주급을 일할 보상합니다."):
        "계약금 4주분·주급·기간은 게임 모델입니다. 기존 기본 운영비는 유지되며 새 명시 계약의 급여가 추가됩니다.";
    return q;
}
bool confirmStaffAction(GameState& s,const StaffQuote& input,std::string* error){
    const auto q=quoteStaffAction(s,input.clubId,input.personId,input.action,input.roles,input.years);
    if(!q.allowed || input.token.empty() || input.token!=q.token){if(error)*error=q.allowed?"조건이 바뀌었습니다. 비용을 다시 확인하세요.":q.reason;return false;}
    const auto name=s.staffPeople.at(q.personId).name;
    if(q.action=="dismiss" || q.action=="renew"){
        for(auto& c:s.staffContracts)if(c.personId==q.personId && active(c,s.currentDate)){if(q.action=="dismiss")endContract(s,c,"dismissed");
            else {c.status="renewed";c.endedDate=s.currentDate;}break;}
    }
    if(!q.replacedContract.empty())for(auto& c:s.staffContracts)if(c.id==q.replacedContract){endContract(s,c,"replaced");break;}
    if(q.action=="roles"){
        for(auto& c:s.staffContracts)if(c.personId==q.personId && active(c,s.currentDate)){
            if(has(c.roles,"scout") && !has(q.roles,"scout")){
                // Preserve other employment while ending the departed scouting role's tasks.
                const auto oldStatus=c.status;endContract(s,c,"role_changed");c.status=oldStatus;c.endedDate.reset();
            }
            c.roles=q.roles;break;
        }
    }else if(q.action!="dismiss"){
        const auto id="staff-contract:"+std::to_string(s.staffRevision+1);
        s.staffContracts.push_back({id,q.personId,q.clubId,q.roles,s.currentDate,s.currentDate.addMonths(q.years*12).addDays(-1),{},q.weeklyPence,"active",false});
        if(has(q.roles,"manager") && q.action=="hire"){
            auto& policy=s.staffPolicies.at(q.clubId);policy.relationship=60;policy.resignationDate.reset();policy.nextReviewDate=s.currentDate.addDays(28);
            if(q.clubId==s.club.id)s.club.manager.boardConfidence=50;
        }
    }
    event(s,q.clubId,q.personId,q.action,name+" · "+join(q.roles)+" · 즉시 "+formatMoney(q.payNowPence)+" / 주급 "+formatMoney(q.weeklyPence),q.payNowPence);
    charge(s,q.clubId,q.payNowPence,q.action=="dismiss"?"staff_exit":"staff_contract",s.staffEvents.back().id);
    projectStaffRoles(s);return true;
}
bool setStaffPolicy(GameState& s,const std::string& club,Money weekly,bool delegation,Money limit,std::string* error){
    if(!s.staffPolicies.contains(club) || weekly<staffWeeklyPayroll(s,club) || weekly<0 || weekly>moneyLimit || limit<0 || limit>weekly){
        if(error)*error="현재 계약 주급 이상인 한도를 입력하세요. 위임 한도는 전체 직원 한도 이하여야 합니다.";return false;
    }
    auto& p=s.staffPolicies.at(club);
    if(p.weeklyLimitPence==weekly && p.delegateHiring==delegation && p.delegationLimitPence==limit)return true;
    p.weeklyLimitPence=weekly;p.delegateHiring=delegation;p.delegationLimitPence=limit;
    event(s,club,"","policy","직원 한도 "+formatMoney(weekly)+" / 단장 채용 위임 "+(delegation?"허용":"해제")+". 현금은 증가하지 않습니다.");
    return true;
}
bool decideStaffRequest(GameState& s,const std::string& id,bool approve,const std::string& reason,const std::string& token,std::string* error){
    auto it=std::find_if(s.staffRequests.begin(),s.staffRequests.end(),[&](const auto& r){return r.id==id;});
    if(it==s.staffRequests.end() || it->status!="pending" || it->deadline<s.currentDate){if(error)*error="이미 처리됐거나 기한이 지난 요청입니다.";return false;}
    const auto* manager=staffForRole(s,it->clubId,"manager");
    if(!manager || manager->id!=it->managerId){if(error)*error="요청한 감독이 현재 재직 중이 아닙니다.";return false;}
    const auto q=quoteStaffAction(s,it->clubId,it->candidateId,"hire",{it->role},1);
    auto& p=s.staffPolicies.at(it->clubId);
    if(approve){
        auto confirmed=q;confirmed.token=token;
        if(!confirmStaffAction(s,confirmed,error))return false;
        it->status="approved";it->reason="비용 확인 후 계약 완료";p.relationship=std::min(100,p.relationship+8);
    }else{
        if(reason!="budget" && reason!="duplicate" && reason!="direction"){if(error)*error="예산·역할 중복·장기 방향 중 거절 사유를 고르세요.";return false;}
        const bool justified=(reason=="budget" && !q.allowed) || (reason=="duplicate" && staffForRole(s,it->clubId,it->role));
        const int penalty=justified?1:std::max(5,(100-manager->patience)/4);
        p.relationship=std::max(0,p.relationship-penalty);it->status="rejected";
        it->reason=(reason=="budget"?"예산 부족":reason=="duplicate"?"역할 중복":"장기 방향 불일치")+std::string(justified?" · 근거를 수용했습니다.":" · 감독이 결정에 동의하지 않습니다.");
        event(s,it->clubId,it->managerId,"request_rejected",it->reason);
    }
    it->decidedDate=s.currentDate;
    if(p.relationship>=35)p.resignationDate.reset();
    if(p.relationship<=20 && !p.resignationDate){p.resignationDate=s.currentDate.addDays(14);event(s,it->clubId,it->managerId,"resignation_warning","감독 신뢰가 낮습니다. 14일 안에 관계를 회복하지 못하면 사임합니다. (게임 모델)");}
    return true;
}
void processStaffDay(GameState& s){
    initializeStaffOperations(s);
    if(*s.staffProcessedDate>=s.currentDate)return;
    // Due contracts end before selection/observation; past accrued obligations are retained.
    for(auto& c:s.staffContracts)if(c.status=="active" && c.endDate && *c.endDate<s.currentDate){
        const auto name=s.staffPeople.at(c.personId).name;endContract(s,c,"expired");
        event(s,c.clubId,c.personId,"expired",name+" 계약 만료 · 새 급여 발생 없음");
    }
    for(auto& [club,p]:s.staffPolicies){
        if(p.resignationDate && *p.resignationDate<=s.currentDate){
            for(auto& c:s.staffContracts)if(c.clubId==club && active(c,s.currentDate) && has(c.roles,"manager")){
                const auto id=c.personId;endContract(s,c,"resigned");event(s,club,id,"resigned","감독 사임. 공석 동안 역할 대행으로 경기를 진행합니다.");break;
            }p.resignationDate.reset();
        }
        // AI uses exactly the same quote, cash, budget and confirmation functions.
        if(club!=s.club.id && operatingProfile(s,club) && s.currentDate>=p.nextReviewDate){
            p.nextReviewDate=s.currentDate.addDays(28);
            for(const auto& role:{"manager","coach","medical"}){
                if(staffForRole(s,club,role))continue;
                const auto* candidate=candidateFor(s,role);if(!candidate)continue;
                auto q=quoteStaffAction(s,club,candidate->id,"hire",{role},1);
                if(q.allowed)(void)confirmStaffAction(s,q);
            }
            std::vector<std::string> renew;
            // Cover the full interval until the next review so an expiry is not skipped.
            for(const auto& c:s.staffContracts)if(c.clubId==club && active(c,s.currentDate) && c.endDate && s.currentDate.daysUntil(*c.endDate)<=28)renew.push_back(c.personId);
            for(const auto& id:renew){auto q=quoteStaffAction(s,club,id,"renew",{},1);if(q.allowed)(void)confirmStaffAction(s,q);}
        }
        if(club==s.club.id)requestReview(s,club);
        if(p.accruedDate<s.currentDate && operatingProfile(s,club)){
            const Money n=staffWeeklyPayroll(s,club)+p.remainder;p.accruedPence+=n/7;p.remainder=static_cast<int>(n%7);p.accruedDate=s.currentDate;
            if(p.nextPayDate<=s.currentDate){
                if(p.accruedPence>0)s.operatingObligations.push_back({"staff-payroll:"+club+":"+p.nextPayDate.toIsoString(),
                    club,s.activeSeasonId,p.nextPayDate,p.accruedPence,"staff_payroll",TransferPaymentStatus::Scheduled,{},{},"staff_v1_gameplay_not_historical"});
                p.accruedPence=0;p.nextPayDate=s.currentDate.addDays(7);
            }
        }
    }
    for(auto& r:s.staffRequests)if(r.status=="pending"){
        if(r.deadline<s.currentDate){r.status="expired";r.decidedDate=s.currentDate;r.reason="응답 기한 경과 · 자동 계약 없음";continue;}
        auto& p=s.staffPolicies.at(r.clubId);
        if(!p.delegateHiring || r.role=="manager" || !staffForRole(s,r.clubId,"director") || staffForRole(s,r.clubId,r.role))continue;
        auto q=quoteStaffAction(s,r.clubId,r.candidateId,"hire",{r.role},1);
        if(q.allowed && q.weeklyPence<=p.delegationLimitPence)(void)decideStaffRequest(s,r.id,true,"",q.token);
    }
    processStaffMeetings(s);
    s.staffProcessedDate=s.currentDate;++s.staffRevision;
}
bool setStaffMeetingPeople(GameState& s,const std::string& club,std::vector<std::string> people,std::string* error){
    if(!s.staffPolicies.contains(club)){if(error)*error="구단이 없습니다.";return false;}
    std::sort(people.begin(),people.end());
    if(std::adjacent_find(people.begin(),people.end())!=people.end()){if(error)*error="중복 참석자입니다.";return false;}
    for(const auto& id:people){
        const auto* c=activeStaffContract(s,id);
        if(!c || c->clubId!=club){if(error)*error="현재 재직 직원만 회의 참석자로 지정할 수 있습니다.";return false;}
    }
    auto& p=s.staffPolicies.at(club);
    const bool chooseManual=club==s.club.id && !people.empty()
        && s.operatingMeetingSettings.attendeeMode!=OperatingMeetingAttendeeMode::Manual;
    if(p.meetingPeople==people && !chooseManual)return true;
    p.meetingPeople=std::move(people);
    if(chooseManual)s.operatingMeetingSettings.attendeeMode=OperatingMeetingAttendeeMode::Manual;
    event(s,club,"","meeting_people","수동 회의 지정 인원 "+std::to_string(p.meetingPeople.size())+"명. 지정이 없으면 선택한 역할별 담당자를 배정합니다.");
    return true;
}
std::optional<GameDate> nextStaffDate(const GameState& s){
    std::optional<GameDate> next;
    const auto add=[&](const GameDate& d){const auto v=d<=s.currentDate?s.currentDate.addDays(1):d;if(!next || v<*next)next=v;};
    for(const auto& c:s.staffContracts)if(c.clubId==s.club.id && c.status=="active" && c.endDate)add(c.endDate->addDays(1));
    for(const auto& r:s.staffRequests)if(r.clubId==s.club.id && r.status=="pending")add(r.deadline.addDays(1));
    const auto p=s.staffPolicies.find(s.club.id);if(p!=s.staffPolicies.end()){add(p->second.nextReviewDate);if(p->second.resignationDate)add(*p->second.resignationDate);if(staffWeeklyPayroll(s,s.club.id)>0)add(p->second.nextPayDate);}
    if(s.operatingMeetingSettings.nextMeetingDate)add(*s.operatingMeetingSettings.nextMeetingDate);
    return next;
}
std::string validateStaffOperations(const GameState& s){
    if(!s.staffActivation || !s.staffProcessedDate || !s.staffActivation->isValid() || !s.staffProcessedDate->isValid()
        || *s.staffActivation>*s.staffProcessedDate || *s.staffProcessedDate>s.currentDate || s.staffRevision<0
        || !s.staffPolicies.contains(s.club.id) || s.staffPeople.empty() || s.staffContracts.empty())return "Invalid staff activation/clock";
    const auto validRoles=[](const auto& roles){std::set<std::string> seen;for(const auto& r:roles)if(!has({"manager","coach","medical","scout","director","finance"},r) || !seen.insert(r).second)return false;return !roles.empty();};
    for(const auto& [id,p]:s.staffPeople){
        if(id.empty() || id!=p.id || p.name.empty() || p.provenance.empty() || !validRoles(p.qualifiedRoles)
            || p.askingWeeklyPence<0 || p.askingWeeklyPence>moneyLimit || p.capacity<1 || p.capacity>10
            || (has(p.qualifiedRoles,"scout") && p.scoutId.empty()))return "Invalid staff identity";
        for(int v:{p.coaching,p.medical,p.judging,p.authority,p.patience,p.adaptability,p.potential})if(v<0 || v>100)return "Invalid staff ability";
    }
    std::set<std::string> ids,employed,managers;
    for(const auto& c:s.staffContracts){
        if(c.id.empty() || !ids.insert(c.id).second || !s.staffPeople.contains(c.personId) || !s.staffPolicies.contains(c.clubId)
            || !validRoles(c.roles) || c.roles.size()>2 || !c.startDate.isValid() || c.startDate>s.currentDate
            || (c.endDate && (!c.endDate->isValid() || *c.endDate<c.startDate))
            || (c.endedDate && (!c.endedDate->isValid() || *c.endedDate<c.startDate || *c.endedDate>s.currentDate))
            || (c.status=="active")==c.endedDate.has_value() || !has({"active","dismissed","replaced","renewed","expired","resigned"},c.status)
            || c.weeklyPence<0 || c.weeklyPence>moneyLimit || (c.legacyTerms && (c.weeklyPence!=0 || c.endDate))
            || (!c.legacyTerms && !c.endDate))return "Invalid staff contract";
        for(const auto& role:c.roles)if(!has(s.staffPeople.at(c.personId).qualifiedRoles,role))return "Unqualified staff role";
        if(c.status=="active"){
            if(!employed.insert(c.personId).second || (has(c.roles,"manager") && !managers.insert(c.clubId).second))return "Duplicate staff employment/manager";
        }
    }
    for(const auto& [club,p]:s.staffPolicies){
        if(!operatingClubFinance(s,club) || p.weeklyLimitPence<0 || p.weeklyLimitPence>moneyLimit
            || p.delegationLimitPence<0 || p.delegationLimitPence>p.weeklyLimitPence || p.accruedPence<0 || p.accruedPence>moneyLimit*100
            || p.remainder<0 || p.remainder>6 || p.relationship<0 || p.relationship>100 || !p.nextReviewDate.isValid()
            || !p.nextPayDate.isValid() || !p.accruedDate.isValid() || p.accruedDate>s.currentDate
            || (p.resignationDate && !p.resignationDate->isValid()))return "Invalid staff policy";
        std::set<std::string> people;
        for(const auto& id:p.meetingPeople)if(!s.staffPeople.contains(id) || !people.insert(id).second)return "Invalid manual meeting people";
    }
    ids.clear();
    for(const auto& r:s.staffRequests){
        if(r.id.empty() || !ids.insert(r.id).second || !s.staffPolicies.contains(r.clubId) || !s.staffPeople.contains(r.managerId)
            || !s.staffPeople.contains(r.candidateId) || !has(s.staffPeople.at(r.candidateId).qualifiedRoles,r.role)
            || !has({"approval","recruitment"},r.kind) || !has({"pending","approved","rejected","expired","cancelled"},r.status)
            || !r.createdDate.isValid() || !r.deadline.isValid() || r.createdDate>s.currentDate || r.deadline<r.createdDate
            || (r.status=="pending")==r.decidedDate.has_value()
            || (r.decidedDate && (!r.decidedDate->isValid() || *r.decidedDate<r.createdDate || *r.decidedDate>s.currentDate)))return "Invalid staff request";
    }
    ids.clear();
    for(const auto& e:s.staffEvents)if(e.id.empty() || !ids.insert(e.id).second || !s.staffPolicies.contains(e.clubId)
        || (!e.personId.empty() && !s.staffPeople.contains(e.personId)) || !e.date.isValid() || e.date>s.currentDate
        || e.amountPence<0 || e.amountPence>moneyLimit*200)return "Invalid staff event";
    return {};
}
}
