#pragma once
#include "optimality.hpp"
namespace rx {
// Synthesize a loop inside independently generated prefix/suffix contexts.
// Relations enforce inclusion; finite observations reject missing words;
// only a fresh exact language product is allowed to publish a result.
struct ContextLoopSearch {
    const DFA &target;
    const Budget &global;
    Limits limits;
    std::function<R(R)> refine;
    std::function<void(R,const std::string&)> offer;
    size_t generated=0, proofs=0;
    using Action=std::array<uint16_t,12>;
    int n=0, mask=0;
    Action identity{};
    ContextLoopSearch(const DFA& d,const Budget& b,Limits l,std::function<R(R)> f,
                      std::function<void(R,const std::string&)> o)
        :target(d),global(b),limits(l),refine(std::move(f)),offer(std::move(o)){}
    Action product(const Action&a,const Action&b) const {
        Action result{};
        for(int p=0;p<n;++p)
            for(unsigned bits=a[p];bits;bits&=bits-1)result[p]|=b[__builtin_ctz(bits)];
        return result;
    }
    Action closure(Action a) const {
        for(int p=0;p<n;++p)a[p]|=uint16_t(1u<<p);
        for(int k=0;k<n;++k)for(int p=0;p<n;++p)if(a[p]&(1u<<k))a[p]|=a[k];
        return a;
    }
    std::string signature(const Action&a) const {
        std::string s;
        for(int p=0;p<n;++p){s+=char(a[p]&255);s+=char(a[p]>>8);}
        return s;
    }
    Action action(R r) const {
        if(r->k==EPS)return identity;
        Action result{};
        if(r->k==CHAR){for(int p=0;p<n;++p)for(int c=0;c<10;++c)if(r->mask&(1<<c))result[p]|=uint16_t(1u<<target.t[p][c]);}
        else if(r->k==ALT){for(R c:r->sub){Action a=action(c);for(int p=0;p<n;++p)result[p]|=a[p];}}
        else if(r->k==CAT){result=identity;for(R c:r->sub)result=product(result,action(c));}
        else if(r->k==REP){Action a=action(r->sub[0]),power=identity;for(I i=0;i<r->lo;++i)power=product(power,a);if(r->hi==INF)result=product(power,closure(a));else{result=power;for(I i=r->lo;i<r->hi;++i){power=product(power,a);for(int p=0;p<n;++p)result[p]|=power[p];}}}
        return result;
    }
    R run(R best,double seconds=4) {
        n=int(target.t.size());
        if(n>12||seconds<=0)return best;
        Budget budget(seconds);budget.end=std::min(budget.end,global.end);
        auto check=[&](){global.check();budget.check();};
        try{
            auto inv=OptimalityCheck(target,limits).invariants(budget);mask=inv.first;
            int letters=__builtin_popcount(unsigned(mask));
            if(letters<2||letters>4||!inv.second)return best;
            for(int p=0;p<n;++p)identity[p]=uint16_t(1u<<p);
            std::vector<R> atoms;
            for(int m=mask;m;m=(m-1)&mask)atoms.push_back(chars(m));
            std::vector<R> primitive=atoms;
            for(R r:atoms)for(auto b:std::vector<std::pair<I,I>>{{0,INF},{1,INF},{0,1}})primitive.push_back(rep(r,b.first,b.second));
            std::unordered_map<std::string,std::vector<R>> baseGroups;
            auto insertBase=[&](R r){
                if(r->s.size()>8)return;
                auto& group=baseGroups[signature(action(r))];
                for(R old:group)if(eq(old,r))return;
                group.push_back(r);std::sort(group.begin(),group.end(),better);
                if(group.size()>4)group.resize(4);
            };
            for(R r:primitive)insertBase(r);
            for(R a:primitive)for(R b:primitive){check();insertBase(cat({a,b}));}
            std::vector<R> words=atoms;
            for(int depth=2;depth<=3;++depth){std::vector<R> next;for(R a:words)for(R b:atoms){R r=cat({a,b});next.push_back(r);insertBase(r);}words=std::move(next);}
            std::vector<R> ornaments{eps()};
            for(const auto& g:baseGroups)for(R r:g.second)ornaments.push_back(rep(r,0,1));
            std::sort(ornaments.begin(),ornaments.end(),better);
            ornaments.erase(std::unique(ornaments.begin(),ornaments.end(),[](R a,R b){return eq(a,b);}),ornaments.end());
            if(ornaments.size()>512)ornaments.resize(512);
            std::vector<R> contexts{eps()};
            for(R a:atoms)contexts.push_back(rep(a,0,INF));
            auto loops=contexts;
            for(R a:loops)for(R b:loops)contexts.push_back(cat({a,b}));
            std::sort(contexts.begin(),contexts.end(),better);
            contexts.erase(std::unique(contexts.begin(),contexts.end(),[](R a,R b){return eq(a,b);}),contexts.end());
            std::set<std::string> samples{""};
            auto include=[&](const std::string&w){auto s=samples;if(w.size()>10)return;for(size_t a=0;a<=w.size();++a)for(size_t b=a;b<=w.size();++b)s.insert(w.substr(a,b-a));if(s.size()<=63)samples=std::move(s);};
            std::vector<std::string> access(n);std::vector<bool> reached(n);std::vector<int> todo{target.start};reached[target.start]=true;
            for(size_t i=0;i<todo.size();++i)for(int c=0;c<10;++c)if(mask&(1<<c)){int q=target.t[todo[i]][c];if(!reached[q]){reached[q]=true;access[q]=access[todo[i]]+char('0'+c);todo.push_back(q);}}
            for(int p:todo)include(access[p]);
            std::vector<std::string> frontier{""};
            for(int depth=1;depth<=4;++depth){std::vector<std::string> next;for(const auto&w:frontier)for(int c=0;c<10;++c)if(mask&(1<<c)){next.push_back(w+char('0'+c));include(next.back());}frontier=std::move(next);}
            FiniteObservation obs(samples);uint64_t wanted=0;
            for(size_t i=0;i<obs.words.size();++i)if(matches(target,obs.words[i]))wanted|=1ULL<<i;
            struct Macro{R r;Action a;uint64_t value;};
            auto macro=[&](R r){return Macro{r,action(r),obs.evaluate(r)};};
            std::vector<Macro> sides,centers,frames;
            for(R r:ornaments)sides.push_back(macro(r));
            for(R r:atoms)centers.push_back(macro(r));
            for(R r:contexts)frames.push_back(macro(r));
            struct Body{R r;Action closed;uint64_t value;};
            std::unordered_map<std::string,std::vector<Body>> groups;
            auto generationEnd=Budget::Clock::now()+std::chrono::duration_cast<Budget::Clock::duration>(std::chrono::duration<double>(seconds*.55));
            for(const Macro& a:sides){
                check();
                if(Budget::Clock::now()>=generationEnd)break;
                for(const Macro& b:sides)for(const Macro& c:centers){
                    if(a.r->s.size()+b.r->s.size()+c.r->s.size()+3>=best->s.size())continue;
                    if((++generated&255)==0)check();
                    Action closed=closure(product(product(a.a,c.a),b.a));
                    uint64_t value=obs.repeat(obs.product(obs.product(a.value,c.value),b.value),0,INF);
                    std::string key=signature(closed);
                    for(int i=0;i<8;++i)key+=char(value>>(8*i));
                    R r=cat({a.r,c.r,b.r});
                    auto& group=groups[key];
                    bool duplicate=false;for(const auto& old:group)if(eq(old.r,r))duplicate=true;
                    if(duplicate)continue;
                    group.push_back({r,closed,value});
                    std::sort(group.begin(),group.end(),[](const Body&a,const Body&b){return better(a.r,b.r);});
                    if(group.size()>8)group.resize(8);
                }
            }
            std::vector<Body> bodies;
            for(auto& group:groups)for(auto& body:group.second)bodies.push_back(std::move(body));
            std::sort(bodies.begin(),bodies.end(),[](const Body&a,const Body&b){return better(a.r,b.r);});
            struct Candidate{R r;};std::vector<Candidate> candidates;
            std::unordered_set<std::string> seen;
            uint16_t accepted=0;for(int p=0;p<n;++p)if(target.accept[p])accepted|=uint16_t(1u<<p);
            for(const Body& body:bodies){
                check();R loop=rep(body.r,0,INF);
                if(loop->s.size()>=best->s.size())continue;
                for(const Macro&p:frames){
                    uint16_t states=0;
                    for(unsigned bits=p.a[target.start];bits;bits&=bits-1)states|=body.closed[__builtin_ctz(bits)];
                    for(const Macro&s:frames){
                        if(p.r->s.size()+loop->s.size()+s.r->s.size()>=best->s.size())continue;
                        uint16_t ends=0;for(unsigned bits=states;bits;bits&=bits-1)ends|=s.a[__builtin_ctz(bits)];
                        if(ends&~accepted)continue;
                        if(obs.product(obs.product(p.value,body.value),s.value)!=wanted)continue;
                        R r=cat({p.r,loop,s.r});if(seen.insert(r->s).second)candidates.push_back({r});
                    }
                }
            }
            std::sort(candidates.begin(),candidates.end(),[](const Candidate&a,const Candidate&b){return better(a.r,b.r);});
            for(const auto& c:candidates){
                check();if(c.r->s.size()>=best->s.size())continue;++proofs;
                if(compare(target,compileRaw(Parser(c.r->s,false).parse(),limits,&budget),limits,&budget).equal){
                    R r=refine(c.r);if(better(c.r,r))r=c.r;
                    if(compare(target,compileRaw(Parser(r->s,false).parse(),limits,&budget),limits,&budget).equal){best=r;offer(r,"Kontextuelle Schleifensynthese");}
                }
            }
        }catch(const Limit&){global.check();}
        return best;
    }
};
}
