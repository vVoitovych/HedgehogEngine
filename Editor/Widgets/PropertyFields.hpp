#pragma once

// The inspector's building blocks: two-column property rows (a label on the left, its widget
// filling the right), X/Y/Z vector fields and component headers with an icon, an optional enable
// checkbox and a menu.
namespace Editor
{
    // Starts a table of property rows; every PropertyLabel goes between this and EndPropertyTable.
    // Returns false when the table is not visible, and then EndPropertyTable must not be called.
    bool BeginPropertyTable(const char* id);
    void EndPropertyTable();

    // Starts a row: label in the left column, then moves to the right one with the next item
    // sized to fill it.
    void PropertyLabel(const char* label);

    // Three equal cells, each a muted X, Y or Z before a drag of its value; filling the cell the
    // cursor is in. Returns whether any value changed.
    bool DragVector3Xyz(const char* id, float* values, float speed = 0.1f);

    struct ComponentHeaderOptions
    {
        void* Icon      = nullptr; // drawn before the label; nullptr draws none
        void* MenuIcon  = nullptr; // the menu button's picture
        bool* Enabled   = nullptr; // a checkbox before the icon when set
        bool  Removable = true;    // a menu with Remove component at the header's right end
    };

    struct ComponentHeaderResult
    {
        bool Open            = false; // draw the component's rows
        bool RemoveRequested = false; // Remove component was picked
        bool EnabledChanged  = false; // the checkbox was toggled
    };

    // A framed, collapsible header for a component, open by default. It pushes nothing, so the
    // caller draws its rows when Open and pops nothing.
    ComponentHeaderResult ComponentHeader(const char* label, const ComponentHeaderOptions& options);
}
