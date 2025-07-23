#define cimg_use_openmp
#include "include/CImg.h"
#include <chrono>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <iostream>
#include <numeric>
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
const glm::vec3 BKGCOLOR = {0.f, 0.f, 0.f};
// Origin
const glm::vec3 ORIGIN = {0.f, 0.f, 0.f};

// Maximum number of light bounces
const unsigned int MAXDEPTH = 3;
// Epsilon
const float EPS = 1.e-5f;

// I am not using it at the moment, but it could speed up things later on
// because the sqrt in the L2 norm is an expensive operation
glm::vec3 normalizeL1(const glm::vec3 &v)
{
    return v / glm::l1Norm(v);
}

// TODO: Add color and a way to draw them
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
    // Reflectiveness
    float reflectiveness;
    // Refractiveness
    float refractiveness;
    // Refractive index. n_2 in https://en.wikipedia.org/wiki/Snell's_law
    float refractiveIndex;
    void normalize_factors()
    {
        float normFactor = specularR + diffuseR + ambientR + reflectiveness + refractiveness;
        specularR /= normFactor;
        diffuseR /= normFactor;
        ambientR /= normFactor;
        reflectiveness /= normFactor;
        refractiveness /= normFactor;
    }
};

struct Sphere
{
    // center
    glm::vec3 c;
    //radius
    float r;
    // color
    Material material;
    glm::vec3 ambientColor;

    Sphere(const glm::vec3 &c, const float &r, const Material &material)
        : c{c}
        , r{r}
        , material{material}
    {
        ambientColor = (material.color + BKGCOLOR) * material.ambientR;
    }

    // o - origin, d - direction, da - (output) distance to the first intersection,
    // db - (output) distance to the second intersection, i.e. a = o + da*d, b = o + db*d
    // d needs to be inputed normalized for performance reasons.
    // Returns true + da or da+db if intersects, false if not
    // Currently, I am not using the second intersection (db)
    bool ray_intersects(const glm::vec3 &o, const glm::vec3 &d, float &da, float &db) const
    {
        // default values
        da = -1.f;
        db = -1.f;
        // Vector from the origin to the center of the sphere
        glm::vec3 oc = c - o;
        // If the intersection is backwards, do not count it
        float dotdoc = glm::dot(d, oc);
        if (dotdoc < 0.f)
            return false;
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
        // Case when the projection is on the border of the sphere
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
                return true;
                // Case where o is inside the sphere. For now I am counting it as a non-intersection
            } else {
                // std::cout << "inside" << std::endl;
                // If it is "before" the centre, return the distance 'till p + the distance
                // 'till from p to a. If it is "after", then erase the distance 'till p.
                // da = (glm::dot(d, -cp) > 0) ? opN + apN : apN - opN;
                return false;
            }
        }
    }
};

// POSSIBLE OPTIMIZATION: Use L1 norm in some places to avoid the slow sqrt()
glm::vec3 trace_ray(const glm::vec3 &o,
                    const glm::vec3 &d,
                    const std::vector<Sphere> &spheres,
                    const std::vector<PointLight> &pointLights,
                    unsigned int depth = 0)
{
    // This is a trick to access the spheres in an increasing order of distance. Only compute it when we are
    // not in the origin, from where they are ordered already in the main() function
    std::vector<size_t> indices(spheres.size());
    std::iota(indices.begin(), indices.end(), 0);
    if (o != ORIGIN)
        std::ranges::stable_sort(indices, [o, spheres](const size_t &i0, const size_t &i1) {
            return glm::length(spheres[i0].c - o) < glm::length(spheres[i1].c - o);
        });

    glm::vec3 outColor{BKGCOLOR};
    for (const size_t &id : indices) {
        const Sphere &s = spheres[id];
        // Depth is used for reflections and refractions
        float da, db;
        if (depth < MAXDEPTH && s.ray_intersects(o, d, da, db)) {
            // Intersection point of the ray with the sphere
            glm::vec3 a = o + da * d;

            // Normal to the surface of the sphere
            glm::vec3 normal = glm::normalize(a - s.c);

            // Intensity of diffuse light
            float diffuseIntensity = 0.f;
            float specularIntensity = 0.f;
            for (const PointLight &pl : pointLights) {
                // vector from the ray intersection with the sphere to the point light
                glm::vec3 plaDir = pl.position - a;
                float lightDist = glm::length(plaDir);
                plaDir /= lightDist;

                // SHADOWS
                // Displace the point through the normal
                // in order to avoid self intersection
                glm::vec3 aDisp = (glm::dot(plaDir, normal) > 0) ? a + EPS * normal
                                                                 : a - EPS * normal;
                // Break the loop over point lights if there is any sphere on the way to the light
                bool inShadow = false;
                for (const Sphere &s : spheres) {
                    // This is done as an optimization and in order to avoid self-intersections.
                    float datmp, dbtmp;
                    // First check: Avoid self-intersection. Third check: Test only from the point a
                    // until the light, and not further
                    if (s.ray_intersects(aDisp, plaDir, datmp, dbtmp) && datmp < lightDist) {
                        inShadow = true;
                        break;
                    }
                }
                // Skip light if we are in a shadow
                if (inShadow)
                    continue;

                // Attenuation factor depending on the distance to the light
                float attenuation = 1.f / lightDist;
                // attenuation = 1.f;

                // DIFFUSE LIGHTING
                // Accumulate intensity wrt to the amount of overlapping of the pointlight
                // direction with the normal (dot product).
                float normalPlaOverlap = glm::dot(normal, plaDir);
                if (s.material.diffuseR > 0.f) {
                    diffuseIntensity += pl.intensity * attenuation
                                        * std::max(0.f, normalPlaOverlap);
                }

                // SPECULAR LIGHTING
                // Vector from a to the light. This is the reflection of plaDir along normal.
                // Can be checked that this is a unitary vector, so no need to normalize
                if (s.material.specularR > 0.f) {
                    glm::vec3 reflectionDir = (-2.f * normalPlaOverlap * normal + plaDir);
                    // Overlap of the reflection direction with the primary ray direction (view direction)
                    float reflectionOverlap = glm::dot(d, reflectionDir);
                    // Specular factor computed as in the first approximation in wikipedia:
                    // https://en.wikipedia.org/wiki/Phong_reflection_model#Concepts
                    float specularFactor = (reflectionOverlap > 0.f)
                                               ? std::pow(reflectionOverlap * reflectionOverlap,
                                                          s.material.shininessN)
                                               : 0.f;
                    specularIntensity += pl.intensity * attenuation * specularFactor;
                }

                // REFLECTIONS
                glm::vec3 reflectionColor{0.f};
                if (s.material.reflectiveness > 0.f) {
                    // Reflection of d along normal
                    glm::vec3 dReflected = (d - 2.f * glm::dot(d, normal) * normal);
                    // if (material.reflectiveness > 0.f)

                    // Trace rays up to maxDepth to get the reflection color
                    reflectionColor = trace_ray(a, dReflected, spheres, pointLights, depth + 1);
                }

                // REFRACTION. Snell's law: https://en.wikipedia.org/wiki/Snell%27s_law#Vector_form
                glm::vec3 refractionColor{0.f};
                if (s.material.refractiveness > 0.f) {
                    float n1 = 1.f;
                    float n2 = s.material.refractiveIndex;
                    float n = n1 / n2;
                    float cos1 = -glm::dot(d, normal);
                    float cos2 = std::sqrt(1.f - n * n * (1.f - cos1 * cos1));
                    glm::vec3 dRefracted = n * d + (n * cos1 - cos2) * normal;
                    // Again displace the intersection over the normal in order to avoid self-intersecion
                    glm::vec3 aDisp = (glm::dot(dRefracted, normal) > 0) ? a + EPS * normal
                                                                         : a - EPS * normal;
                    refractionColor = trace_ray(aDisp, dRefracted, spheres, pointLights, depth + 1);
                }

                // Combination of the different lightings
                outColor = s.ambientColor
                           + s.material.color
                                 * (s.material.diffuseR * diffuseIntensity
                                    + s.material.specularR * specularIntensity)
                           + reflectionColor * s.material.reflectiveness
                           + refractionColor * s.material.refractiveness;
            }
            // Break the loop once an intersection has been found. No need to search for more.
            break;
        }
    }

    return outColor;
}

int main()
{
    std::vector<Sphere> spheres;
    Material m1{};
    m1.color = glm::vec3{0.2, 0.4, 0.2};
    m1.diffuseR = 0.2f;
    m1.specularR = 0.8f;
    m1.shininessN = 4;
    m1.ambientR = 1.f;
    m1.reflectiveness = 0.5f;
    m1.refractiveness = 0.f;
    m1.refractiveIndex = 1.f;
    m1.normalize_factors();
    Material m2{};
    m2.color = glm::vec3{0.4f, 0.2f, 0.2f};
    m2.diffuseR = 0.7f;
    m2.specularR = 0.3f;
    m2.shininessN = 1;
    m2.ambientR = 1.f;
    m2.reflectiveness = 0.f;
    m2.refractiveness = 0.f;
    m2.refractiveIndex = 1.f;
    m2.normalize_factors();
    Material m3{};
    m3.color = glm::vec3{1.f, 1.f, 1.f};
    m3.diffuseR = 0.f;
    m3.specularR = 0.3f;
    m3.shininessN = 5;
    m3.ambientR = 1.f;
    m3.reflectiveness = 0.5f;
    m3.refractiveness = 1.f;
    m3.refractiveIndex = 1.1f;
    m3.normalize_factors();

    spheres.push_back(Sphere{glm::vec3{-2.f, -1.5f, 4.f}, 1.f, m1});
    spheres.push_back(Sphere{glm::vec3{-1.f, -0.5f, 5.f}, 1.f, m2});
    spheres.push_back(Sphere{glm::vec3{1.f, 1.7f, 5.f}, 1.f, m1});
    spheres.push_back(Sphere{glm::vec3{2.f, 0.5f, 6.f}, 1.f, m2});
    spheres.push_back(Sphere{glm::vec3{0.f, 0.3f, 3.f}, 0.7f, m3});

    // Sort the vector of spheres by distance to the camera
    std::ranges::stable_sort(spheres, [](const Sphere &s0, const Sphere &s1) {
        return glm::length(s0.c - ORIGIN) < glm::length(s1.c - ORIGIN);
    });

    std::vector<PointLight> pointLights;
    pointLights.push_back(PointLight(glm::vec3(0.f, -5.f, 1.f), 15.f));
    pointLights.push_back(PointLight(glm::vec3(-3.f, -5.f, 1.f), 15.f));
    pointLights.push_back(PointLight(glm::vec3(3.f, -5.f, 1.f), 15.f));
    pointLights.push_back(PointLight(glm::vec3(0.f, -5.f, 6.f), 15.f));
    pointLights.push_back(PointLight(glm::vec3(-3.f, -5.f, 6.f), 15.f));
    pointLights.push_back(PointLight(glm::vec3(3.f, -5.f, 6.f), 15.f));

    CImg<float> img(W, H, 1, 3);
    // Fill each channel‐slice for setting the background:
    img.get_shared_slice(0).fill(BKGCOLOR.x); // red
    img.get_shared_slice(1).fill(BKGCOLOR.y); // green
    img.get_shared_slice(2).fill(BKGCOLOR.z); // blue

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
        glm::vec3 color = trace_ray(ORIGIN, d, spheres, pointLights);
        img(j, i, 0, 0) = color.x;
        img(j, i, 0, 1) = color.y;
        img(j, i, 0, 2) = color.z;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    std::cout << dt.count() << " ms." << std::endl;
    img.display("crt", false);
    // img.save("crt.png");

    return 0;
}
