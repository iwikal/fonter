#version 400 core

out vec4 FragColor;

uniform isamplerBuffer u_points;
uniform isamplerBuffer endpoints;
uniform uint num_contours;

dvec3 point(int j)
{
    return dvec3(texelFetch(u_points, j).rgb) / dvec3(1024.0, 1024.0, 1);
}

int endpoint(int i)
{
    return texelFetch(endpoints, i).r;
}

dvec2 min_dist_straight(dvec2 pos, dvec2 start, dvec2 end)
{
    dvec2 b = end - start;
    dvec2 c = pos - start;

    double t = clamp(dot(b, c) / dot(b, b), 0, 1);
    dvec2 p = b * clamp(t, 0, 1);
    dvec2 q = c - p;
    double dist = dot(q, q);

    double norm = (b.x * q.y - b.y * q.x);
    double ortho_sq = norm * norm / dot(b, b);

    return dvec2(dist * sign(norm), ortho_sq);
}

double newton_rhapson_cubic(double t, double a, double b, double c, double d)
{
    double da = 3 * a;
    double db = 2 * b;
    double dc = c;

    for (int i = 0; i < 10; i++)
    {
        double t2 = t * t;
        double t3 = t2 * t;
        double f = a * t3 + b * t2 + c * t + d;
        double dfdt = 3 * a * t2 + 2 * a * t + c;
        t -= f / dfdt;
    }

    return t;
}

bool on_curve(dvec3 point)
{
    return point.z >= 0.5;
}

dvec2 min_dist_bezier(dvec2 pos, dvec2 start, dvec2 control, dvec2 end)
{
    dvec2 aA = start - 2 * control + end;
    dvec2 bB = control - start;

    double a = dot(aA, aA);
    double b = 3 * dot(aA, bB);
    double c = 2 * dot(bB, bB) + dot(aA, start) - dot(aA, pos);
    double d = dot(bB, start) - dot(bB, pos);

    double min_dist = 1.0 / 0.0;
    double min_factor;
    dvec2 nearest_point;
    for (int i = 0; i < 2; i++)
    {
        double root = newton_rhapson_cubic(i, a, b, c, d);
        double t = clamp(root, 0, 1);
        dvec2 curve_point = (aA * t * t) + (2 * bB * t) + (start);
        double dist = dot(curve_point - pos, curve_point - pos);
        if (dist < min_dist)
        {
            min_dist = dist;
            min_factor = t;
            nearest_point = curve_point;
        }
    }

    dvec2 direction = 2 * (aA * min_factor + bB);
    dvec2 nearest_vec = pos - nearest_point;

    double norm = (direction.x * nearest_vec.y - direction.y * nearest_vec.x);
    double ortho_sq = dot(norm, norm) / dot(b, b);

    return dvec2(min_dist * sign(norm), ortho_sq);
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
        int end_contour = endpoint(c) + 1;
        int len = end_contour - start_contour;
        for (int j = start_contour; j < end_contour; j++)
        {
            dvec3 a = point(j);
            dvec3 b = point(j + 1 < end_contour ? j + 1 : j + 1 - len);
            dvec3 c = point(j + 2 < end_contour ? j + 2 : j + 2 - len);
            dvec2 result;
            if (on_curve(a) && on_curve(b))
                result = min_dist_straight(pos, a.xy, b.xy);
            else if (!on_curve(a) && on_curve(b))
                continue;
            else
            {
                if (!on_curve(a)) a = dvec3((a.xy + b.xy) / 2, 1);
                if (!on_curve(c)) c = dvec3((b.xy + c.xy) / 2, 1);
                result = min_dist_bezier(pos, a.xy, b.xy, c.xy);
            }

            double dist = result.x;
            double ortho_sq = result.y;
            double diff = abs(min_dist) - abs(dist);
            double err = 0.00000000001;
            if (diff > err || abs(diff) <= err && ortho_sq > best_ortho)
            {
                min_dist = dist;
                best_ortho = ortho_sq;
            }
        }
    }

    min_dist = sqrt(abs(min_dist)) * sign(min_dist);
    vec3 foreground = vec3(0);
    vec3 background = vec3(1);
    FragColor.rgb = vec3(min_dist > 0 ? background : foreground);
    FragColor.a = 1.0;
}
