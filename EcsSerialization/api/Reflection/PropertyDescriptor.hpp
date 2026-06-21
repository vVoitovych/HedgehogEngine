#pragma once

#include "TypeTag.hpp"
#include "PropertyFlags.hpp"

namespace Reflection
{
    using FieldAccessor = void*(*)(void*);

    struct PropertyDescriptor
    {
        TypeTag            type;
        const char*        name;
        FieldAccessor      accessor;
        PropertyFlags      flags;
        float              sliderMin;
        float              sliderMax;
        bool(*guiOverride)(void* comp, const PropertyDescriptor& prop) = nullptr;
        const char* const* enumLabels     = nullptr;
        int                enumLabelCount = 0;
    };

    template<typename T>
    T* FieldPtr(void* component, const PropertyDescriptor& desc)
    {
        return static_cast<T*>(desc.accessor(component));
    }
}
