#version 400 core

out vec4 FragColor;

uniform isamplerBuffer x_coords;
uniform isamplerBuffer y_coords;
uniform isamplerBuffer endpoints;
uniform uint num_contours;

dvec2 point(int j)
{
    double x = double(texelFetch(x_coords, j).r);
    double y = double(texelFetch(y_coords, j).r);

    return dvec2(x, y) / 1024.0;
}

int endpoint(int i)
{
    return texelFetch(endpoints, i).r;
}

void main()
{
    dvec2 dims = dvec2(800, 640);
    dvec2 pos = 2 * gl_FragCoord.xy / dims - 1;

    vec3 colors[7] = vec3[7](
            vec3(0.0, 0.0, 0.0),
            vec3(0.0, 0.0, 1.0),
            vec3(0.0, 1.0, 0.0),
            vec3(0.0, 0.8, 0.8),
            vec3(1.0, 0.0, 0.0),
            vec3(0.8, 0.0, 0.8),
            vec3(0.8, 0.8, 0.0)
        );

    double min_dist = 1.0 / 0.0;
    double best_ortho = 0;

    for (int c = 0; c < num_contours; c++)
    {
        int start_contour = c == 0 ? 0 : endpoint(c - 1) + 1;
        int end_contour = endpoint(c);
        for (int j = start_contour; j <= end_contour; j++)
        {
            dvec2 a = point(j);
            dvec2 b = point(j == end_contour ? start_contour : j + 1) - a;
            dvec2 c = pos - a;

            double t = clamp(dot(b, c) / dot(b, b), 0, 1);
            dvec2 p = b * clamp(t, 0, 1);
            dvec2 q = c - p;
            double dist = dot(q, q);

            double norm = (b.x * q.y - b.y * q.x);
            double ortho_sq = dot(norm, norm) / dot(b, b);
            double diff = abs(min_dist) - dist;
            bool better = false;
            double err = 0.00000000001;
            better = better || diff > err;
            better = better || abs(diff) <= err && ortho_sq > best_ortho;
            if (better)
            {
                min_dist = dist * sign(norm);
                best_ortho = ortho_sq;
            }
        }
    }

    min_dist = sqrt(abs(min_dist)) * sign(min_dist);
    FragColor.rgb = vec3(min_dist + 0.5);
    FragColor.a = 1.0;
}
