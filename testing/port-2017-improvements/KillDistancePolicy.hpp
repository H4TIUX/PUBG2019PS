#pragma once
#include <cmath>
namespace KillDistancePolicy {
inline bool Metres(float ax,float ay,float az,float bx,float by,float bz,float& out) {
    if (!std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(az)||
        !std::isfinite(bx)||!std::isfinite(by)||!std::isfinite(bz)) return false;
    const double dx=double(ax)-bx,dy=double(ay)-by,dz=double(az)-bz;
    const double centimetres=std::sqrt(dx*dx+dy*dy+dz*dz);
    if (!std::isfinite(centimetres)||centimetres>10000000.0) return false;
    out=float(centimetres/100.0); return true;
}
}
