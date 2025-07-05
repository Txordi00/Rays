#include "include/CImg.h"
#include <glm/glm.hpp>

using namespace cimg_library;

struct Sphere
{
    // center
    glm::vec3 c;
    //radius
    float r;
    // color
    glm::vec4 color;
    Sphere(const glm::vec3 &c, const float &r, const glm::vec4 &color)
        : c{c}
        , r{r}
        , color{color}
    {}

    // o - origin, d - direction, da - (output) distance to the first intersection,
    // db - (output) distance to the second intersection, i.e. a = o + da*v, b = o + db*v
    // d needs to be normalized
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

        // Should be equivalent to cp = oc - op. Vector from c to the projection p
        glm::vec3 p = o + op;
        glm::vec3 cp = c - p;

        // L2 norm of cp
        float cpN = glm::length(cp);
        // If the projection is out of the sphere there is not intersection
        if (cpN > r)
            return false;
        // Probably use epsilons here, this is when the intersection is on the border
        // of the sphere
        else if (cpN == r) {
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
    CImg<float> img(640, 480, 1, 3);
    cimg_forXY(img, x, y)
    {
        img(x, y, 0, 0) = (float) x / (float) img.width();
        img(x, y, 0, 1) = (float) y / (float) img.height();
        img(x, y, 0, 2) = (float) (x + y) / (float) (img.width() + img.height());
    }
    img.display("crt");
    img *= 255.f;
    img.save("crt.png");

    return 0;
}
