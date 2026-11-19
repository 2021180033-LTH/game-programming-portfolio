#include "core/player_growth.h"
#include "core/player_directory.h"
#include "core/match_engine.h"
#include "core/season.h"
#include "core/staff_operations.h"
#include <algorithm>
#include <array>
#include <numeric>

namespace football_owner {
namespace {
constexpr int unit = 1000; // Fixed-point skill progress; no floating point or mutable RNG.
std::string role(const std::string& position) {
    return matchPositionCovers(position,"GK")?"GK":matchPositionCovers(position,"DEF")?"DEF":
        matchPositionCovers(position,"ATT")?"ATT":"MID";
}
std::array<int,7> weights(const std::string& r) {
    if(r=="GK")return {60,80,0,60,85,115,155};
    if(r=="DEF")return {85,90,45,155,115,110,0};
    if(r=="ATT")return {110,85,155,35,120,105,0};
    return {110,155,75,80,90,125,0};
}
// Relative gameplay curves from design section 58, NOT measured historical coefficients.
int ageRate(int age,int skill,bool keeper) {
    if(skill==6 && !keeper)return 0;
    if(keeper && (skill==4 || skill==6))age-=3;
    if(skill==4)return age<18?85:age<22?65:age<25?30:age<28?5:age<31?-25:age<34?-55:-85;
    if(skill==5)return age<18?30:age<22?45:age<25?45:age<28?40:age<31?35:age<34?20:age<37?5:-15;
    return age<18?65:age<22?70:age<25?40:age<28?20:age<31?10:age<34?0:-20;
}
void startPeriod(GameState& s,const std::string& id,PlayerGrowth& g,const std::string& club) {
    g.clubId=club;g.tenureStart=g.periodStart=s.currentDate;
    g.baselineSkills=s.playerDevelopment.at(id).skills;g.baselineEstimate.clear();
    for(int i=0;i<7;++i) {
        const auto key=std::to_string(s.rngSeed)+"|growth_assessment_v2|"+id+"|"+club+"|"+std::to_string(i);
        const int bias=static_cast<int>(seasonStableHash(key)%9)-4;
        g.baselineEstimate.push_back(std::clamp(g.baselineSkills[i]+bias,1,100));
    }
    g.periodDays=g.periodMinutes=g.periodCoaching=g.restrictedDays=g.recoveryDays=0;
}
void weekly(GameState& s,const std::string& id,const std::string& position,std::optional<int> age) {
    auto& d=s.playerDevelopment.at(id);auto& g=*d.growth;auto& a=s.playerAbilities.at(id);
    const auto w=weights(role(position)),anchor=weights(g.anchorRole);
    const int total=std::accumulate(anchor.begin(),anchor.end(),0);
    int net=0;for(int i=0;i<7;++i)net+=(d.skills[i]-g.anchorSkills[i])*anchor[i];
    int minutes=0;for(const auto& x:g.exposure)if(x.date>=s.currentDate.addDays(-7)&&x.date<s.currentDate)minutes+=x.minutes;
    const int coach=g.trainingDays?g.coachingSum/g.trainingDays:50;
    for(int i=0;i<7;++i) {
        int gain=age?ageRate(*age,i,role(position)=="GK"):0;
        if(gain>0) {
            if(a.current>=a.potential)gain=0;
            const int aptitude=85+static_cast<int>(seasonStableHash(std::to_string(s.rngSeed)+"|growth_aptitude_v2|"+id+"|"+std::to_string(i))%31);
            const int experience=age && *age<18?85+std::min(minutes,180)*25/180:70+std::min(minutes,180)*50/180;
            const int headroom=50+std::clamp(a.potential-a.current,0,20)*50/20;
            // int64 retains precision until the final division; every factor has a named unit.
            std::int64_t value=gain;
            for(int factor:{w[i],aptitude,70+coach*60/100,experience,headroom,150-d.skills[i]})value=value*factor/100;
            gain=static_cast<int>(value*g.trainingUnits/14);
        } else if(gain<0) {
            // Ageing is not cancelled by a good coach or injury; judgement may still improve.
            gain=gain*(i==4?100:w[i])/100;
        }
        g.remainder[i]+=gain;
        if(g.remainder[i]>=unit) {
            if(d.skills[i]<100 && net+anchor[i]<=(a.potential-g.anchorAbility)*total) {
                ++d.skills[i];net+=anchor[i];
            }
            g.remainder[i]-=unit; // No banked burst beyond PA/skill cap.
        } else if(g.remainder[i]<=-unit) {
            if(d.skills[i]>1){--d.skills[i];net-=anchor[i];}
            g.remainder[i]+=unit;
        }
    }
    const int previous=a.current;
    a.current=std::clamp(g.anchorAbility+net/total,1,a.potential);
    if(a.reachablePotential<a.current)a.reachablePotential=a.current; // The moving ceiling follows; the innate maximum never moves.
    d.lastChange=std::clamp(a.current-previous,-1,1);
    d.lastTrainingDate=g.trainingDate=s.currentDate;
    g.trainingDays=g.trainingUnits=g.coachingSum=0;
}
}
void addPlayerGrowth(GameState& s,const std::string& id,const std::string& club,const std::string& position) {
    auto& d=s.playerDevelopment.at(id);if(d.growth)return;
    PlayerGrowth g;g.initializedDate=g.trainingDate=s.currentDate;g.anchorRole=role(position);
    g.anchorSkills=d.skills;g.anchorAbility=s.playerAbilities.at(id).current;
    // Preserve the old fractional investment without changing any present skill or CA.
    g.remainder.assign(7,d.progress*10);startPeriod(s,id,g,club);d.growth=std::move(g);
}
void initializePlayerGrowth(GameState& s) {
    if(s.growthActivation)return;
    s.growthActivation=s.currentDate;
    const auto rows=playerDirectory(s,s.currentDate);
    for(const auto& [id,d]:s.playerDevelopment) {
        const auto row=rows.find(id);
        addPlayerGrowth(s,id,row==rows.end()?"":row->second.activeClubId,row==rows.end()?"":row->second.player.position);
    }
}
void trainPlayerGrowth(GameState& s,const std::string& id,const std::string& club,
                       const std::string& position,std::optional<int> age,bool eligible) {
    auto& d=s.playerDevelopment.at(id);auto& g=*d.growth;
    if(g.clubId!=club)startPeriod(s,id,g,club);
    std::erase_if(g.exposure,[&](const auto& x){return x.date<s.currentDate.addDays(-7);});
    if(g.tenureStart==s.currentDate)return; // Joining/activation date is a baseline, not a full training day.
    const int coach=staffRoleAbility(s,club,"coach");
    ++g.periodDays;g.periodCoaching+=coach;
    if(!eligible)++g.restrictedDays;
    else if(d.programme=="recovery")++g.recoveryDays;
    if(g.trainingDays<7) {
        ++g.trainingDays;g.coachingSum+=coach;
        if(eligible)g.trainingUnits+=d.programme=="recovery"?1:2;
    }
    if(g.trainingDate.daysUntil(s.currentDate)>=7)weekly(s,id,position,age);
    if(g.periodStart.daysUntil(s.currentDate)>=28) {
        GrowthReport r;r.clubId=club;r.tenureStart=g.tenureStart;r.from=g.periodStart;r.to=s.currentDate;
        r.before=g.baselineEstimate;
        for(int i=0;i<7;++i)r.after.push_back(std::clamp(r.before[i]+d.skills[i]-g.baselineSkills[i],1,100));
        r.minutes=g.periodMinutes;r.coach=g.periodDays?g.periodCoaching/g.periodDays:50;
        r.restrictedDays=g.restrictedDays;r.recoveryDays=g.recoveryDays;
        r.context=(age?std::to_string(*age)+"세":"생년월일 미상 · 연령 성장 보류")+std::string(" · ")+role(position)
            +" · 실제 출전 "+std::to_string(r.minutes)+"분 · 코치진 효과 "+std::to_string(r.coach)+"/100"
            +" · 치료·자격 제한 "+std::to_string(r.restrictedDays)+"일 · 회복 훈련 "+std::to_string(r.recoveryDays)+"일";
        g.reports.push_back(r);if(g.reports.size()>24)g.reports.erase(g.reports.begin());
        g.baselineSkills=d.skills;g.baselineEstimate=r.after;g.periodStart=s.currentDate;
        g.periodDays=g.periodMinutes=g.periodCoaching=g.restrictedDays=g.recoveryDays=0;
    }
}
void recordGrowthMinutes(GameState& s,const std::string& id,const std::string& club,const GameDate& date,int minutes) {
    auto& d=s.playerDevelopment.at(id);
    // Legacy catch-up may settle old fixtures, but cannot grant today's growth or rewrite a closed report.
    if(!d.growth || date!=s.currentDate || date<d.growth->initializedDate || minutes<=0)return;
    auto& g=*d.growth;
    if(g.clubId!=club)startPeriod(s,id,g,club);
    auto it=std::find_if(g.exposure.begin(),g.exposure.end(),[&](const auto& x){return x.date==date;});
    const int previous=it==g.exposure.end()?0:it->minutes;
    const int credited=std::min(240-previous,std::clamp(minutes,0,120));
    if(it==g.exposure.end())g.exposure.push_back({date,credited});
    else it->minutes+=credited;
    g.periodMinutes+=credited;
}
std::string validatePlayerGrowth(const GameState& s) {
    if(!s.growthActivation || !s.growthActivation->isValid() || *s.growthActivation>s.currentDate)return "Invalid growth activation";
    const auto skills=[](const auto& v,int lo,int hi){return v.size()==7 && std::all_of(v.begin(),v.end(),[&](int x){return x>=lo&&x<=hi;});};
    for(const auto& [id,d]:s.playerDevelopment) {
        if(!d.growth)return "Missing growth state";
        const auto& g=*d.growth;
        if(!g.initializedDate.isValid() || g.initializedDate<*s.growthActivation || g.initializedDate<d.initializedDate || g.initializedDate>s.currentDate
            || !g.trainingDate.isValid() || g.trainingDate<g.initializedDate || g.trainingDate>s.currentDate
            || !g.tenureStart.isValid() || g.tenureStart<g.initializedDate || g.tenureStart>s.currentDate
            || !g.periodStart.isValid() || g.periodStart<g.tenureStart || g.periodStart>s.currentDate
            || g.anchorAbility<1 || g.anchorAbility>100 || (g.anchorRole!="GK"&&g.anchorRole!="DEF"&&g.anchorRole!="MID"&&g.anchorRole!="ATT")
            || !skills(g.anchorSkills,1,100)||!skills(g.remainder,-999,999)||!skills(g.baselineSkills,1,100)||!skills(g.baselineEstimate,1,100)
            || g.trainingDays<0||g.trainingDays>7||g.trainingUnits<0||g.trainingUnits>g.trainingDays*2||g.coachingSum<0||g.coachingSum>g.trainingDays*100
            || g.periodDays<0||g.periodDays>28||g.periodMinutes<0||g.periodMinutes>7000||g.periodCoaching<0||g.periodCoaching>g.periodDays*100
            || g.restrictedDays<0||g.recoveryDays<0||g.restrictedDays+g.recoveryDays>g.periodDays||g.reports.size()>24||g.exposure.size()>8)return "Invalid growth state";
        std::optional<GameDate> previous;
        for(const auto& x:g.exposure) {
            if(!x.date.isValid()||x.date<g.initializedDate||x.date>s.currentDate||x.minutes<1||x.minutes>240||(previous&&x.date<=*previous))return "Invalid growth exposure";
            previous=x.date;
        }
        previous.reset();
        for(const auto& r:g.reports) {
            if(r.clubId.empty()||!r.tenureStart.isValid()||r.tenureStart<g.initializedDate||!r.from.isValid()||!r.to.isValid()
                ||r.from<r.tenureStart||r.from.daysUntil(r.to)<28||r.to>s.currentDate||(previous&&r.from<*previous)
                ||!skills(r.before,1,100)||!skills(r.after,1,100)||r.minutes<0||r.minutes>7000||r.coach<0||r.coach>100
                ||r.restrictedDays<0||r.recoveryDays<0||r.restrictedDays+r.recoveryDays>28||r.context.empty())return "Invalid growth report";
            previous=r.to;
        }
        const auto latest=std::find_if(g.reports.rbegin(),g.reports.rend(),[&](const auto& r){return r.tenureStart==g.tenureStart&&r.clubId==g.clubId;});
        if(latest!=g.reports.rend() && (latest->to!=g.periodStart || latest->after!=g.baselineEstimate))return "Inconsistent growth comparison baseline";
        if(latest==g.reports.rend() && g.periodStart!=g.tenureStart)return "Missing current growth comparison history";
    }
    return {};
}
} // namespace football_owner
