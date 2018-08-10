#version 310 es
precision highp float;

precision highp int;


layout(location = 0) in highp vec4 dEQP_Position;

layout(location = 1) in int in0;

layout(location = 0) flat out uint v_out0;

bool out0;



void main()

{
    // OpINotEqual is used when convert int to bool, and sign flag mismatch in the comparation
    out0 = bool(in0);
    gl_Position = dEQP_Position;

    v_out0 = uint(out0);


}