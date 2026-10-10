#version 450

// Marks the selection's pixels in the mask the SelectionOutline pass reads.

layout(location = 0) out float outMask;

void main()
{
    outMask = 1.0f;
}
