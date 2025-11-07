// float diffuse(const Material mat, const vec3 lightDir, const vec3 normal)
// {
//   if(mat.diffuseR > 0.01) {
//     // Lambertian
//     float dotNL = max(dot(normal, lightDir), 0.);
//     //vec3  c     = diffuseR * dotNL;
//     return mat.diffuseR * dotNL;
//   }
//   else
//     return 0.;
// }

// float specular(const Material mat, const vec3 viewDir, const vec3 lightDir, const vec3 normal)
// {
//   if(mat.specularR > 0.01)
//   {
//     vec3 reflectionDir = reflect(lightDir, normal);
//     // Overlap of the reflection direction with the primary ray direction (view direction)
//     float reflectionOverlap = dot(viewDir, reflectionDir);
//     // Specular factor computed as in the first approximation in wikipedia:
//     // https://en.wikipedia.org/wiki/Phong_reflection_model#Concepts
//     float specularFactor = (reflectionOverlap > 0.f)
//         ? mat.specularR * pow(reflectionOverlap * reflectionOverlap,
//                               mat.shininessN)
//         : 0.f;
//     return specularFactor;
//   }
//   else
//     return 0.;
// }

#define print_val(message, val, valMin, valMax) if(val < valMin || val > valMax){ \
       debugPrintfEXT(message, val); \
     }

float D_GGX(const float NoH, const float a) {
    const float a2 = a * a;
    const float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

vec3 F_Schlick(const float u, const vec3 f0) {
     const float u11 = 1. - u;
     const float u12 = u11 * u11; // u1^2
     const float u14 = u12 * u12;
     const float u15 = u14 * u11;
     return f0 + (vec3(1.) - f0) * u15;
}

float V_SmithGGXCorrelated(const float NoV, const float NoL, const float a) {
    const float a2 = a * a;
    const float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    const float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

float Fd_Lambert() {
    return 1.0 / PI;
}
