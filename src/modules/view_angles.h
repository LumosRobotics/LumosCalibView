#pragma once

#include "math/math.h"

using namespace lumos;

enum class SnappingAxis
{
    SA_None,
    SA_X,
    SA_Y,
    SA_Z
};

class ViewAngles
{
private:
    double azimuth_;
    double snapped_azimuth_;
    double elevation_;
    double snapped_elevation_;
    double angle_limit_;

    void setSnapAngles();
    double calcElevationSnapAngle() const;
    double calcAzimuthSnapAngle() const;

public:
    ViewAngles();
    ViewAngles(const double azimuth, const double elevation);
    ViewAngles(const double azimuth, const double elevation, const double angle_limit);
    void setAngles(const double azimuth, const double elevation);
    void changeAnglesWithDelta(const double azimuth, const double elevation);
    void setAngleLimit(const double angle_limit);
    double getAzimuth() const;
    double getElevation() const;
    double getSnappedAzimuth() const;
    double getSnappedElevation() const;
    AxisAngle<double> getAngleAxis() const;
    FixedSizeMatrix<double, 3, 3> getRotationMatrix() const;
    AxisAngle<double> getSnappedAngleAxis() const;
    FixedSizeMatrix<double, 3, 3> getSnappedRotationMatrix() const;
    bool isCloseToSnap() const;
    bool bothSnappedBelowAngleLimitAroundZero() const;
    double getAngleLimit() const;

    bool isSnappedAlongX() const;
    bool isSnappedAlongY() const;
    bool isSnappedAlongZ() const;

    bool isSnappedLookingAlongPositiveX() const;
    bool isSnappedLookingAlongPositiveY() const;
    bool isSnappedLookingAlongPositiveZ() const;
    bool isSnappedLookingAlongNegativeX() const;
    bool isSnappedLookingAlongNegativeY() const;
    bool isSnappedLookingAlongNegativeZ() const;

    SnappingAxis getSnappingAxis() const;
};
