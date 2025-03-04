#version 400 core

uniform ivec2 u_dims;
uniform vec2 u_pos;
uniform float units_per_em;
uniform float u_size;
uniform ivec2 u_bbox_min;
uniform ivec2 u_bbox_max;

dvec2 map(dvec2 value, dvec2 inMin, dvec2 inMax, dvec2 outMin, dvec2 outMax) {
  return outMin + (outMax - outMin) * (value - inMin) / (inMax - inMin);
}

void main()
{
    ivec2 positions[4] = ivec2[4](ivec2(u_bbox_min.x, u_bbox_min.y),
                                  ivec2(u_bbox_max.x, u_bbox_min.y),
                                  ivec2(u_bbox_min.x, u_bbox_max.y),
                                  ivec2(u_bbox_max.x, u_bbox_max.y));

    dvec2 pos = dvec2(positions[gl_VertexID] + u_pos);
    pos *= dvec2(u_size) / dvec2(units_per_em);

    gl_Position = vec4(2.0 * pos / dvec2(u_dims) - 1.0, 0, 1);
}
