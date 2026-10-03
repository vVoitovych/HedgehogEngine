#include "AxisGizmo.hpp"

namespace Editor
{
    std::array<ProjectedAxis, 3> ProjectWorldAxes(const HM::Matrix4x4& view)
    {
        std::array<ProjectedAxis, 3> axes;
        for (size_t axis = 0; axis < axes.size(); ++axis)
        {
            const HM::Vector4& column = view[axis];
            axes[axis] = { column[0], -column[1], column[2] };
        }
        return axes;
    }
}
