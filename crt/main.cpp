#define cimg_use_openmp
#include "include/CImg.h"
#include <chrono>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <iostream>
#include <omp.h>

using namespace cimg_library;

// Focal length
const float F = 1.f;
// Field of view
const float FOV = glm::radians(70.f);
// Resolution
const unsigned int W = 1920;
const unsigned int H = 1080;
// Aspect ratio
const float AR = float(W) / float(H);
// Background color
const float BKGCOLOR[3] = {0.f, 0.f, 0.0f};

glm::vec3 normalizeL1(const glm::vec3 &v)
{
    return v / glm::l1Norm(v);
}

struct PointLight
{
    PointLight(const glm::vec3 &position, const float &intensity)
        : position(position)
        , intensity(intensity)
    {}
    glm::vec3 position;
    float intensity;
};

struct Material
{
    // Color
    glm::vec3 color;
    // specular reflectiveness
    float specularR;
    // Diffuse reflectiveness
    float diffuseR;
    // Ambient reflectiveness
    float ambientR;
    // Shininess factor N.
    // From the approximation in https://en.wikipedia.org/wiki/Phong_reflection_model#Concepts
    // with beta=1
    int shininessN;
};

struct Sphere
{
    // center
    glm::vec3 c;
    //radius
    float r;
    // color
    Material material;

    Sphere(const glm::vec3 &c, const float &r, const Material &material)
        : c{c}
        , r{r}
        , material{material}
    {}

    // o - origin, d - direction, da - (output) distance to the first intersection,
    // db - (output) distance to the second intersection, i.e. a = o + da*d, b = o + db*d
    // d needs to be inputed normalized for performance reasons.
    // Returns true + da or da+db if intersects, false if not
    bool ray_intersects(const glm::vec3 &o, const glm::vec3 &d, float &da, float &db) const
    {
        // default values
        da = -1.f;
        db = -1.f;
        // Vector from the origin to the center of the sphere
        glm::vec3 oc = c - o;
        // Vector from the origin to the projection p of oc on d
        glm::vec3 op = d * glm::dot(d, oc);
        float opN = glm::length(op);

        // Vector from c to the projection p
        // glm::vec3 p = o + op;
        // glm::vec3 cp = c - p;
        // Little optimization:
        glm::vec3 cp = oc - op;

        // L2 norm of cp
        float cpN = glm::length(cp);
        // If the projection is out of the sphere there is not intersection
        if (cpN > r)
            return false;
        // Probably use epsilons here, this is when the intersection is on the border
        // of the sphere
        else if (std::abs(cpN - r) < std::numeric_limits<float>::epsilon()) {
            // std::cout << "border" << std::endl;
            da = opN;
            return true;
            // Handle the cases where the line defined by d intersects the sphere two times
        } else {
            // Norm of oc
            float ocN = glm::length(oc);
            // Norm of the vector from the first intersection a to the projection p
            float apN = std::sqrt(r * r - cpN * cpN);
            // Case where o is outside of the sphere
            if (ocN > r) {
                // The distance to a is the distance from o to p minus apN
                da = opN - apN;
                // Advance 2 units of apN 'till the second intersection
                db = da + 2 * apN;
                // Case where o is inside the sphere
            } else {
                // std::cout << "inside" << std::endl;
                // If it is "before" the centre, return the distance 'till p + the distance
                // 'till from p to a. If it is "after", then erase the distance 'till p.
                da = (glm::dot(d, -cp) > 0) ? opN + apN : apN - opN;
            }
            return true;
        }
    }

    // Computes the color (outColor) of a ray if there is an intersection
    bool trace_ray(const glm::vec3 &o,
                   const glm::vec3 &d,
                   const std::vector<PointLight> &pointLights,
                   glm::vec3 &outColor,
                   float &da,
                   float &db)
    {
        if (ray_intersects(o, d, da, db)) {
            // Intersection point of the ray with the sphere
            glm::vec3 a = o + da * d;
            // Normal to the surface of the sphere
            glm::vec3 normal = glm::normalize(a - c);
            // Intensity of diffuse light
            float diffuseIntensity = 0.f;
            for (const PointLight &pl : pointLights) {
                // vector from the ray intersection with the sphere to the point light
                glm::vec3 plaDir = pl.position - a;
                float lightDist = glm::l1Norm(plaDir);
                plaDir /= lightDist;
                float attenuation = 1.f / lightDist;
                // Accumulate intensity wrt to the amount of overlapping of the pointlight
                // direction with the normal (dot product).
                diffuseIntensity += pl.intensity * attenuation
                                    * std::max(0.f, glm::dot(normal, plaDir));
            }
            outColor = material.color * diffuseIntensity;
            return true;
        }
        // No intersection
        return false;
    }
};

int main()
{
    glm::vec3 origin{0.f};
    std::vector<Sphere> spheres;
    Material m1{};
    m1.color = glm::vec3{0.2, 0.4, 0.2};
    Material m2{};
    m2.color = glm::vec3{0.4, 0.2, 0.2};
    spheres.push_back(Sphere{glm::vec3{-1.f, 0.f, 5.f}, 2.f, m1});
    spheres.push_back(Sphere{glm::vec3{0.f, 2.f, 10.f}, 3.f, m2});
    // Sort the vector of spheres by distance to the camera
    std::ranges::sort(spheres, [origin](const Sphere &s0, const Sphere &s1) {
        return glm::length(s0.c - origin) > glm::length(s1.c - origin);
    });

    std::vector<PointLight> pointLights;
    pointLights.push_back(PointLight(glm::vec3(0.f, -10.f, 5.f), 10.f));

    CImg<float> img(W, H, 1, 3);
    // Fill each channel‐slice for setting the background:
    img.get_shared_slice(0).fill(BKGCOLOR[0]); // red
    img.get_shared_slice(1).fill(BKGCOLOR[1]); // green
    img.get_shared_slice(2).fill(BKGCOLOR[2]); // blue

    // I compute these here for performance reasons
    const float Hm1 = float(H) - 1.f;
    const float Wm1 = float(W) - 1.f;
    const float Ft = F * std::tan(FOV / 2.f);
    const float ARFt = AR * Ft;

    auto t0 = std::chrono::high_resolution_clock::now();
#pragma omp parallel for
    cimg_forXY(img, j, i)
    {
        // This is the computation of the ray direction in world coordinates for the center of every pixel
        // I got it by hand. Not difficult!
        float y = (-1.f + 2.f * (float(i) + 0.5f) / Hm1) * Ft;
        float x = (-1.f + 2.f * (float(j) + 0.5f) / Wm1) * ARFt;
        glm::vec3 d = glm::normalize(glm::vec3{x, y, 1.f});
        for (Sphere &s : spheres) {
            float da, db;
            glm::vec3 color;
            // Change the color only if there was an intersection
            if (s.trace_ray(origin, d, pointLights, color, da, db)) {
                img(j, i, 0, 0) = color.x;
                img(j, i, 0, 1) = color.y;
                img(j, i, 0, 2) = color.z;
            }
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    std::cout << dt.count() << " ms." << std::endl;
    img.display("crt");
    img *= 255.f;
    img.save("crt.png");

    return 0;
}
