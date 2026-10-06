#pragma once
// Offline CPU intervals for the captured shader's MAD/DIV arithmetic.
// Microsoft floating-point rules permit unfused MAD steps with 1 ULP error,
// and DIV as a 1-ULP reciprocal followed by a 0.5-ULP multiply:
// https://learn.microsoft.com/en-us/windows/win32/direct3d11/floating-point-rules
// Enclosing the rounded CPU value by two float neighbors covers a 1-ULP
// operation; one neighbor covers a 0.5-ULP operation. These conservative,
// operation-derived bounds are applied BEFORE packed-format quantization.
namespace dof_depth_oracle {
struct Interval {double low{},high{};};
inline Interval enclose(double v,unsigned steps){
 const float q=static_cast<float>(v);dof_fixture::require(std::isfinite(q),"finite arithmetic oracle");
 float low=q,high=q;
 for(unsigned i=0;i<steps;++i){low=std::nextafter(low,-std::numeric_limits<float>::infinity());high=std::nextafter(high,std::numeric_limits<float>::infinity());}
 return {low,high};
}
inline Interval mad(float a,float b,float c){
 const auto product=enclose(double(a)*b,2);
 return {enclose(product.low+c,2).low,enclose(product.high+c,2).high};
}
inline Interval divide(float numerator,Interval denominator){
 dof_fixture::require(numerator>0&&denominator.low>0,"positive division oracle");
 const auto low=enclose(1./denominator.high,2).low,high=enclose(1./denominator.low,2).high;
 return {enclose(double(numerator)*low,1).low,enclose(double(numerator)*high,1).high};
}
inline Interval absolute(Interval v){
 return {v.low<=0&&v.high>=0?0.:(std::min)(std::abs(v.low),std::abs(v.high)),(std::max)(std::abs(v.low),std::abs(v.high))};
}
inline bool packed(float actual,Interval expected,unsigned channel){
 if(channel>2||!std::isfinite(actual)||actual<0||!std::isfinite(expected.low)||!std::isfinite(expected.high)||expected.low>expected.high)return false;
 const auto shift=channel==0?0:channel==1?11:22;const unsigned mask=channel==2?1023:2047;
 const auto encode=[&](float v){return (dof_formats::pack({v,v,v,1})>>shift)&mask;};
 const float lo=(std::max)(0.F,static_cast<float>(expected.low)),hi=static_cast<float>(expected.high);
 if(!std::isfinite(hi)||hi<lo)return false;
 const auto qlo=dof_formats::quantize({lo,lo,lo,1})[channel],qhi=dof_formats::quantize({hi,hi,hi,1})[channel];
 const auto low=encode(lo)-(qlo>(std::max)(0.,expected.low)?1U:0U),high=encode(hi)+(qhi<expected.high?1U:0U);
 return dof_formats::quantize({actual,actual,actual,1})[channel]==actual&&encode(actual)>=low&&encode(actual)<=high;
}
}
