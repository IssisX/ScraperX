#pragma once
#include "sim/grappling_hook_geometry.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::test {
// Observation only. Real contact/rope/load and dwell remain independent gates.
// Evaluate the actual bar in hook coordinates, not an orientation-blind cup box.
inline bool retained_bar_in_crook(JPH::Vec3 center, JPH::Vec3 axis,
                                  float radius, float half_length,
                                  float collar_inner_half_span, float contact_slop) {
    if (!std::isfinite(center.LengthSq()) || !std::isfinite(axis.LengthSq()) ||
        !axis.IsNormalized(1.e-4F) || std::abs(axis.GetZ())<1.e-5F) return false;
    const auto &layout=sim::grappling_hook_layout();
    const JPH::Vec3 cavity(0,.10F,0);
    float mouth_y=std::numeric_limits<float>::max();
    for (const auto &part:layout.parts) for (const auto &p:part.polygon) {
        const auto delta=p-cavity;
        if (std::abs(delta.LengthSq()-.0025F)<1.e-7F) mouth_y=std::min(mouth_y,p.GetY());
        // All hook steel stays between the actual collar inner faces.
        for (float z:{part.z_low,part.z_high})
            if (std::abs((JPH::Vec3(p.GetX(),p.GetY(),z)-center).Dot(axis))>
                collar_inner_half_span) return false;
    }
    if (!std::isfinite(mouth_y)) return false;
    const float tilt=std::sqrt(std::max(0.0F,1-axis.GetZ()*axis.GetZ()));
    unsigned inner_faces=0;
    for (const auto &part:layout.parts) {
        for (std::size_t i=0;i<part.polygon.size();++i) {
            const auto a=part.polygon[i]-cavity;
            const auto b=part.polygon[(i+1)%part.polygon.size()]-cavity;
            if (std::abs(a.LengthSq()-.0025F)>1.e-7F ||
                std::abs(b.LengthSq()-.0025F)>1.e-7F) continue;
            ++inner_faces;
            const auto normal=(a+b).Normalized();
            for (float z:{part.z_low,part.z_high}) {
                const float t=(z-center.GetZ())/axis.GetZ();
                if (std::abs(t)+radius*tilt/std::abs(axis.GetZ())>half_length) return false;
                const auto cross=center+t*axis;
                const float projected=normal.Dot(axis)/axis.GetZ();
                const float extent=radius*std::sqrt(1+projected*projected);
                // Account for the existing solver's contact tolerance, without
                // altering it. A closed steel face still bounds every finger.
                if ((cross-cavity).Dot(normal)+extent>a.Dot(normal)+contact_slop) return false;
                // Require the entire section inward of the actual open tips.
                // Penetration tolerance never permits escape through the mouth.
                const float y_extent=radius*std::sqrt(1+std::pow(axis.GetY()/axis.GetZ(),2));
                if (cross.GetY()-y_extent<mouth_y) return false;
            }
        }
    }
    return inner_faces==48;
}
// Observation only: every square corner crosses every physical finger plane.
inline bool retained_square_in_crook(JPH::Vec3 center,JPH::Vec3 axis,JPH::Vec3 u,JPH::Vec3 v, float half_width, float half_length, float contact_slop){
 if(!std::isfinite(center.LengthSq()) || !axis.IsNormalized(1.e-4F) ||
    !u.IsNormalized(1.e-4F) || !v.IsNormalized(1.e-4F) ||
    std::abs(axis.Dot(u))>1.e-4F || std::abs(axis.Dot(v))>1.e-4F || std::abs(u.Dot(v))>1.e-4F ||
    !std::isfinite(half_width) || half_width<=0 || !std::isfinite(half_length) || half_length<=0 ||
    !std::isfinite(contact_slop) || contact_slop<0 || std::abs(axis.GetZ())<1.e-5F)return false;
 const JPH::Vec3 cavity(0,.1F,0);const float mouth=.1F-.05F/std::sqrt(2.F);
 unsigned faces=0;
 for(const auto&part:sim::grappling_hook_layout().parts){
  for(size_t i=0;i<part.polygon.size();++i){
   const auto a=part.polygon[i]-cavity,b=part.polygon[(i+1)%part.polygon.size()]-cavity;
   if(std::abs(a.LengthSq()-.0025F)>1.e-7F||std::abs(b.LengthSq()-.0025F)>1.e-7F)continue;
   ++faces;const auto n=(a+b).Normalized();
   for(float z:{part.z_low,part.z_high})for(float su:{-half_width,half_width})for(float sv:{-half_width,half_width}){
    const auto corner=center+su*u+sv*v;const float t=(z-corner.GetZ())/axis.GetZ();
    if(std::abs(t)>half_length)return false;
    const auto p=corner+t*axis;
    if(p.GetY()<mouth||(p-cavity).Dot(n)>a.Dot(n)+contact_slop)return false;
   }
  }
 }
 return faces==48;
}
} // namespace scraperx::test
