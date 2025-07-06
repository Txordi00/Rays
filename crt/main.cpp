#define cimg_use_openmp
#include "include/CImg.h"
#include <chrono>
#include <glm/glm.hpp>
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

struct Sphere
{
    // center
    glm::vec3 c;
    //radius
    float r;
    // color
    glm::vec3 color;

    Sphere(const glm::vec3 &c, const float &r, const glm::vec3 &color)
        : c{c}
        , r{r}
        , color{color}
    {}

    // o - origin, d - direction, da - (output) distance to the first intersection,
    // db - (output) distance to the second intersection, i.e. a = o + da*d, b = o + db*d
    // d needs to be inputed normalized for performance reasons.
    // Returns true + da or da+db if intersects, false if not
    bool ray_intersects(const glm::vec3 &o, const glm::vec3 &d, float &da, float &db)
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
            da = opN;
            return true;
            // Handle the cases where the line defined by d intersects the sphere two times
        } else {
            // Norm of oc
            float ocN = glm::length(oc);
            // Norm of the vector from the first intersection a to the projection p
            float apN = glm::sqrt(r * r - cpN);
            // Case where o is outside of the sphere
            if (ocN > r) {
                // The distance to a is the distance from o to p minus apN
                da = opN - apN;
                // Advance 2 units of apN 'till the second intersection
                db = da + 2 * apN;
                // Case where o is inside the sphere
            } else {
                // If it is "before" the centre, return the distance 'till p + the distance
                // 'till from p to a. If it is "after", then erase the distance 'till p.
                da = (glm::dot(d, -cp) > 0) ? opN + apN : apN - opN;
            }
            return true;
        }
    }
};

int main()
{
    glm::vec3 origin{0.f};
    Sphere s{glm::vec3{-1.f, 0.f, 5.f}, 2.f, glm::vec3{0.2, 0.4, 0.2}};
    CImg<float> img(W, H, 1, 3);

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
        float da, db;
        if (s.ray_intersects(origin, d, da, db)) {
            img(j, i, 0, 0) = s.color.x;
            img(j, i, 0, 1) = s.color.y;
            img(j, i, 0, 2) = s.color.z;
        } else {
            img(j, i, 0, 0) = 0.05f;
            img(j, i, 0, 1) = 0.05f;
            img(j, i, 0, 2) = 0.2f;
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    std::cout << dt.count() << " ms." << std::endl;
    img.display("crt");
    // img *= 255.f;
    // img.save("crt.png");

    return 0;
}
