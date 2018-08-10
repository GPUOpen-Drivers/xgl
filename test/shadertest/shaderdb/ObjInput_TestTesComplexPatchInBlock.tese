#version 450 core

layout(triangles) in;

struct S
{
    int     x;
    vec4    y;
    float   z[2];
};

layout(location = 2) patch in TheBlock
{
    float blockFa[3];
    S     blockSa[2];
    float blockF;
} teBlock[2];

layout(location = 0) out float f;

void main(void)
{
    float v = 1.3;

    for (int i0 = 0; i0 < 2; ++i0)
    {
        for (int i1 = 0; i1 < 3; ++i1)
        {
            v += teBlock[i0].blockFa[i1];
        }

        for (int i1 = 0; i1 < 2; ++i1)
        {
            v += int(teBlock[i0].blockSa[i1].x);

            v += teBlock[i0].blockSa[i1].y[0];
            v += teBlock[i0].blockSa[i1].y[1];
            v += teBlock[i0].blockSa[i1].y[2];
            v += teBlock[i0].blockSa[i1].y[3];

            for (int i2 = 0; i2 < 2; ++i2)
            {
                v += teBlock[i0].blockSa[i1].z[i2];
            }
        }

        v += teBlock[i0].blockF;
    }

    f = v;
}
