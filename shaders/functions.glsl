// struct Material
// {
//     // Color
//     vec3 color;
//     // specular reflectiveness
//     float specularR;
//     // Diffuse reflectiveness
//     float diffuseR;
//     // Ambient reflectiveness
//     float ambientR;
//     // Shininess factor N.
//     // From the approximation in https://en.wikipedia.org/wiki/Phong_reflection_model#Concepts
//     // with beta=1
//     int shininessN;
//     // Reflectiveness
//     float reflectiveness;
//     // Refractiveness
//     float refractiveness;
//     // Refractive index. n_2 in https://en.wikipedia.org/wiki/Snell's_law
//     float refractiveIndex;
// };

float diffuse(const Material mat, const vec3 lightDir, const vec3 normal)
{
  // Lambertian
  float dotNL = max(dot(normal, lightDir), 0.);
  //vec3  c     = diffuseR * dotNL;
  return mat.diffuseR * dotNL;
}

float specular(const Material mat, const vec3 viewDir, const vec3 lightDir, const vec3 normal)
{
  vec3 reflectionDir = reflect(lightDir, normal);
  // Overlap of the reflection direction with the primary ray direction (view direction)
  float reflectionOverlap = dot(viewDir, reflectionDir);
  // Specular factor computed as in the first approximation in wikipedia:
  // https://en.wikipedia.org/wiki/Phong_reflection_model#Concepts
  float specularFactor = (reflectionOverlap > 0.f)
                             ? pow(reflectionOverlap * reflectionOverlap,
                                        mat.shininessN)
                             : 0.f;
  return specularFactor;
}

#define printVal(message, val, valMin, valMax) if(val < valMin || val > valMax){ \
       debugPrintfEXT(message, val); \
     }

// void printVal(const float val, const float valMin, const float valMax)
// {
//   if(val < valMin || val > valMax)
//     debugPrintfEXT(text + "%f, ", val);
// }
