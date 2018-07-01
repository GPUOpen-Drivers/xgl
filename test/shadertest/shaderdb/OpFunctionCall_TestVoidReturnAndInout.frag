#version 450

layout(location = 0) out vec4 fragColor;

void func(void);

void main()
{
    func();
}

void func(void)
{
    fragColor = vec4(0.5);
}