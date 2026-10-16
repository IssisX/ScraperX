#include <algorithm>
#include <cmath>
#include <cstdio>
__attribute__((noinline)) float current(float f,float h,float lambda) {
 const float cap=f*h;
 return std::max(0.f,cap-std::abs(lambda))*h;
}
__attribute__((noinline)) float rounded(float f,float h,float lambda) {
 const volatile float cap=f*h;
 return std::max(0.f,cap-std::abs(lambda))*h;
}
int main(){volatile float f=22000,h=1.f/360;const float lambda=f*h;const float a=current(f,h,lambda),b=rounded(f,h,lambda);std::printf("cap=%.12g current=%.12g explicit_round=%.12g\n",lambda,a,b);return a>0&&b==0?0:1;}
