#version 450
#extension  GL_EXT_device_group : enable

void main()
{
    gl_Position = vec4(gl_DeviceIndex * 0.2, 0, 0, 1);
}