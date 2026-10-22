#include "../src/optimize.hpp"
#include <cassert>
namespace rx{std::atomic<bool> interrupted{false};}
using namespace rx;
static int checks=0;
static void require(bool p){if(!p)throw std::runtime_error("context-loop check failed at "+std::to_string(checks));++checks;}
int main(){
 std::mt19937 random(928173);Budget overall(120);Options options;options.quiet=true;options.seconds=30;Optimizer grammar(options);
 int improvements=0;
 // Independent random transition tables, not expression-family instances.
 for(int trial=0;trial<30;++trial){
  DFA d;int n=2+random()%6;d.t.resize(n);d.accept.resize(n);
  for(int p=0;p<n;++p){d.accept[p]=random()%2;d.t[p].fill(n-1);for(int c=0;c<2+trial%2;++c)d.t[p][c]=random()%n;}
  d=minimize(d);R seed=grammar.eliminate(grammar.graph(d),trial%3);if(seed->k==EMPTY)continue;
  ContextLoopSearch search(d,overall,{},[](R r){return structural(r);},[&](R r,const std::string&){require(compare(d,compileRaw(Parser(r->s,false).parse())).equal);++improvements;});
  R result=search.run(seed,.035);require(compare(d,compileRaw(Parser(result->s,false).parse())).equal);
 }
 // Independently build prefix/failure automata for many different forbidden
 // words. Relabeling digits and changing period are part of the holdout.
 for(int trial=0;trial<24;++trial){
  std::string digits=trial%2?"67":"12",word;
  for(int j=0;j<2+trial%5;++j)word+=digits[random()%2];
  int m=int(word.size()),dead=m;DFA d;d.t.resize(m+1);d.accept.assign(m+1,true);d.accept[dead]=false;
  for(int p=0;p<=m;++p){d.t[p].fill(dead);if(p==dead)continue;for(char c:digits){std::string s=word.substr(0,p)+c;int k=std::min(int(s.size()),m);while(s.substr(s.size()-k)!=word.substr(0,k))--k;d.t[p][c-'0']=k;}}
  d=minimize(d);R seed=grammar.eliminate(grammar.graph(d),0);
  ContextLoopSearch search(d,overall,{},[](R r){return structural(r);},[&](R r,const std::string&){require(compare(d,compileRaw(Parser(r->s,false).parse())).equal);++improvements;});
  R result=search.run(seed,.15);require(compare(d,compileRaw(Parser(result->s,false).parse())).equal);
 }
 // Transition relations are checked independently by exact NFA execution.
 for(int trial=0;trial<30;++trial){
  R seed=cat({rep(chars(1),0,INF),rep(chars(2),0,INF)});DFA d=compile(seed);ContextLoopSearch search(d,overall,{},[](R r){return r;},[](R,const std::string&){});search.n=int(d.t.size());for(int p=0;p<search.n;++p)search.identity[p]=uint16_t(1u<<p);
  R block=alt({cat({rep(chars(1),random()%2,INF),chars(3)}),rep(cat({chars(2),chars(1)}),0,1)});
  auto action=search.action(block);auto observed=compileRaw(block);
  std::vector<std::string> words{""};for(int depth=0;depth<7;++depth){std::vector<std::string> next;for(const auto&w:words)for(char c:std::string("01"))next.push_back(w+c);words.insert(words.end(),next.begin(),next.end());}
  for(size_t p=0;p<d.t.size();++p){uint16_t reached=0;for(const auto&w:words)if(matches(observed,w)){int q=int(p);for(char c:w)q=d.t[q][c-'0'];reached|=uint16_t(1u<<q);}require(reached==action[p]);}
 }
 std::cout<<"OK: "<<checks<<" independent context-loop checks; "<<improvements<<" proved holdout improvements.\n";
}
