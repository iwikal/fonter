#version 400 core

out vec4 FragColor;

uniform isamplerBuffer u_points;
uniform isamplerBuffer endpoints;
uniform uint num_contours;

dvec3 point(int j)
{
    return dvec3(texelFetch(u_points, j).rgb) / dvec3(1024.0, 1024.0, 1);
}

bool on_curve(dvec3 point)
{
    return point.z >= 0.5;
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

/**
 * Approximate a root of the cubic at^3 + bt^2 + ct + d,
 * with two different starting points t in parallel.
 */
dvec2 newton_rhapson_cubic(dvec2 t, double a, double b, double c, double d)
{
    // ten iterations seems to work well enough for now,
    // more doesn't seem to improve anything
    for (int i = 0; i < 10; i++)
    {
        dvec2 t2 = t * t;
        dvec2 t3 = t2 * t;
        dvec2 f = (a * t3) + (b * t2) + (c * t) + (d);
        dvec2 dfdt = (3 * a * t2) + (2 * b * t) + (c);
        t -= f / dfdt;
    }

    return t;
}

dvec2 min_dist_bezier(dvec2 pos, dvec2 start, dvec2 control, dvec2 end)
{
    dvec2 aA = start - 2 * control + end;
    dvec2 bB = control - start;

    double a = dot(aA, aA);
    double b = 3 * dot(aA, bB);
    double c = 2 * dot(bB, bB) + dot(aA, start) - dot(aA, pos);
    double d = dot(bB, start) - dot(bB, pos);

    dvec2 root = newton_rhapson_cubic(dvec2(0, 1), a, b, c, d);
    dvec2 t = clamp(root, 0, 1);
    dmat2 curve_point = dmat3x2(aA, 2 * bB, start)
            * transpose(dmat3x2(t * t, t, dvec2(1)));
    dmat2 dist_vec = curve_point - dmat2(pos, pos);
    dvec2 dist = dvec2(dot(dist_vec[0], dist_vec[0]),
            dot(dist_vec[1], dist_vec[1]));

    int i = int(dist.x > dist.y);
    double min_dist = dist[i];
    double min_factor = t[i];
    dvec2 nearest_point = curve_point[i];

    dvec2 direction = 2 * (aA * min_factor + bB);
    dvec2 nearest_vec = pos - nearest_point;

    double norm = (direction.x * nearest_vec.y - direction.y * nearest_vec.x);
    double ortho_sq = norm * norm / dot(direction, direction);

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

    // TODO: maybe rewrite so we're looping over all points in a single loop,
    // meaning the number of iterations is based on a uniform, which is
    // supposedly better for the GPU according to Apple
    for (int c = 0; c < num_contours; c++)
    {
        int start_contour = c == 0 ? 0 : endpoint(c - 1) + 1;
        int end_contour = endpoint(c) + 1;
        int len = end_contour - start_contour;
        for (int j = start_contour; j < end_contour; j++)
        {
            int i = j;
            dvec3 a = point(i);
            dvec3 b = point(++i < end_contour ? i : i - len);
            dvec3 c = point(++i < end_contour ? i : i - len);

            dvec2 result;
            if (on_curve(b))
            {
                if (!on_curve(a)) continue;
                result = min_dist_straight(pos, a.xy, b.xy);
            }
            else
            {
                if (!on_curve(a)) a = (a + b) / 2;
                if (!on_curve(c)) c = (b + c) / 2;
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

    // TODO: de-magic the magic number
    min_dist = sqrt(abs(min_dist)) * sign(min_dist) * 256;
    vec3 foreground = vec3(0);
    vec3 background = vec3(1);
    if (true)
    {
        // mixing gives some primitive AA.
        FragColor.rgb = mix(foreground, background, vec3(min_dist));
    }
    else
    {
        FragColor.rgb = mix(foreground, background, vec3(abs(min_dist)));
    }
    FragColor.a = 1.0;
}
