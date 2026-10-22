#include "../src/optimality.hpp"
#include <cassert>
namespace rx { std::atomic<bool> interrupted{false}; }
using namespace rx;
static int checks=0;
static void require(bool p) { if(!p) throw std::runtime_error("optimality test failed at "+std::to_string(checks)); ++checks; }
static std::set<std::string> wordset(R r,int maximum) {
    auto product=[&](const std::set<std::string>&a,const std::set<std::string>&b) {
        std::set<std::string> result;
        for(const auto&x:a)for(const auto&y:b)if(x.size()+y.size()<=size_t(maximum))result.insert(x+y);
        return result;
    };
    if(r->k==EMPTY)return {};
    if(r->k==EPS)return {""};
    if(r->k==CHAR) {std::set<std::string> result;for(int c=0;c<10;++c)if(r->mask&(1<<c))result.insert(std::string(1,char('0'+c)));return result;}
    if(r->k==ALT) {std::set<std::string> result;for(R c:r->sub){auto a=wordset(c,maximum);result.insert(a.begin(),a.end());}return result;}
    if(r->k==CAT) {std::set<std::string> result{""};for(R c:r->sub)result=product(result,wordset(c,maximum));return result;}
    auto a=wordset(r->sub[0],maximum);
    std::set<std::string> result,power{""};
    int cap=maximum+1;
    for(int n=0;n<=cap;++n) {
        if(n>=std::min(r->lo,I(cap)) && (r->hi==INF||n<=std::min(r->hi,I(cap))))result.insert(power.begin(),power.end());
        power=product(power,a);
    }
    return result;
}
int main() {
    std::set<std::string> samples{""};std::vector<std::string> frontier{""};
    for(int n=1;n<=5;++n){std::vector<std::string> next;for(auto w:frontier)for(char c:std::string("01")){samples.insert(w+c);next.push_back(w+c);}frontier=next;}
    FiniteObservation obs(samples);
    std::mt19937_64 rng(731);
    std::vector<R> bank{eps(),chars(1),chars(2),chars(3)};
    for(int n=0;n<140;++n) {
        R a=bank[rng()%bank.size()],b=bank[rng()%bank.size()],r;
        switch(rng()%4){case 0:r=cat({a,b});break;case 1:r=alt({a,b});break;case 2:r=rep(a,rng()%4,INF);break;default:{int x=rng()%4,y=x+rng()%4;r=rep(a,x,y);break;}}
        if(r->s.size()>40)continue;
        bank.push_back(r);auto words=wordset(r,5);uint64_t value=obs.evaluate(r);
        for(size_t i=0;i<obs.words.size();++i)require(bool(value&(1ULL<<i))==bool(words.count(obs.words[i])));
    }
    struct Case{const char* expression;int minimum;};
    for(auto c:std::vector<Case>{{"",0},{"0",1},{"0*",2},{"0+",2},{"0?",2},{"00",2},{"01",2},{"0|1",3},{"[01]",3},{"0|00",3},{"00*",2},{"0*1*",4},{"[01]*",5},{"[0-9]*",6},{"0{97}",5},{"(0|1){3}",7}}) {
        DFA d=compile(Parser(c.expression,false).parse());
        auto p=OptimalityCheck(d).run(c.expression,4,9);
        if(!p.proven||p.expression.size()!=size_t(c.minimum))std::cerr<<c.expression<<": "<<p.reason<<" LB="<<p.lowerBound<<" result="<<p.expression<<" profiles="<<p.profiles<<"\n";
        require(p.proven);require(p.expression.size()==size_t(c.minimum));require(p.lowerBound==size_t(c.minimum));
        require(compare(d,compileRaw(Parser(p.expression,false).parse())).equal);
    }
    DFA d=compile(Parser("[01]*",false).parse());
    require(!OptimalityCheck(d).run("[01]*",0).proven);
    require(!OptimalityCheck(d).run("0",1).proven);
    auto limited=OptimalityCheck(d).run("[01]*",1,3);require(!limited.proven);require(limited.lowerBound<=5);
    OptimalityCheck small(d);small.profileLimit=2;require(!small.run("[01]*",1).proven);
    interrupted=true;require(!OptimalityCheck(d).run("[01]*",1).proven);interrupted=false;
    bool invalid=false;try{FiniteObservation missing({"","01"});}catch(const std::runtime_error&){invalid=true;}require(invalid);
    std::cout<<"OK: "<<checks<<" independent finite-observation and optimality checks.\n";
}
