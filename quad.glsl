#version 400 core

void main()
{
    vec2 positions[4] = vec2[4](
            vec2(-1.0, -1.0),
            vec2(1.0, -1.0),
            vec2(-1.0, 1.0),
            vec2(1.0, 1.0)
        );

    gl_Position = vec4(positions[gl_VertexID], 0, 1);
}
