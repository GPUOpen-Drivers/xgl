#version 450

layout(location = 0) in float  inColor;
layout(location = 0) out float outColor;

float func(float a)
{
    for (int i = 0; i < 6; i++)
    {
        a = -a;
        for (int j = 0; j < 4; j++)
        {
            a = -a;
            if (i == 1)
            {
                return a;
            }
        }
        if (i == 4)
        {
            return 1.0;
        }
    }
    return 1.0;
}

void main()
{
    outColor = func(inColor);
}