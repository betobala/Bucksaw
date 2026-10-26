#include "../src/Saw.hpp"
#include <cstdio>
using namespace bucksaw;
int main(){
  int fails=0; float prev=-1;
  for(int i=0;i<=20;i++){float y=detuneCurve(i/20.f); printf("%.2f->%.4f ",i/20.f,y); if(y<prev-1e-3f){printf("NONMONO ");fails++;} prev=y;}
  printf("\n");
  // morph=1 should be a square: values cluster at +-A
  float fs=48000; Voice v; for(int i=0;i<WAVES;i++) v.phase[i]=0.1f;
  int mid=0,n=0; float pk=0, sumL=0;
  for(int s=0;s<48000;s++){float l=0,r=0; v.process(110,0,1,1/fs,l,r); float y=l+r; pk=std::max(pk,std::fabs(y)); if(std::fabs(y)<0.5f*7) mid++; n++; sumL+=y;}
  printf("square: peak %.2f, frac mid %.3f, dc %.3f\n",pk,mid/(float)n,sumL/n); if(mid/(float)n>0.03f) fails++;
  // level of one voice, detuned saw, random phases
  uint32_t seed=1; v.randomize(seed); double e=0; float p2=0;
  for(int s=0;s<48000;s++){float l=0,r=0; v.process(110,1,0,1/fs,l,r); l*=OUT_GAIN; r*=OUT_GAIN; e+=l*l; p2=std::max(p2,std::fabs(l));}
  printf("L rms %.2f V peak %.2f V\n",std::sqrt(e/48000),p2);
  printf(fails?"FAILED\n":"ALL OK\n"); return fails;}
