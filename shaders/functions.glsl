float diffuse(const float diffuseR, const vec3 lightDir, const vec3 normal)
{
  // Lambertian
  float dotNL = max(dot(normal, lightDir), 0.);
  //vec3  c     = diffuseR * dotNL;
  return diffuseR * dotNL;
}

// vec3 computeSpecular(WaveFrontMaterial mat, vec3 viewDir, vec3 lightDir, vec3 normal)
// {
//   if(mat.illum < 2)
//     return vec3(0);

//   // Compute specular only if not in shadow
//   const float kPi        = 3.14159265;
//   const float kShininess = max(mat.shininess, 4.0);

//   // Specular
//   const float kEnergyConservation = (2.0 + kShininess) / (2.0 * kPi);
//   vec3        V                   = normalize(-viewDir);
//   vec3        R                   = reflect(-lightDir, normal);
//   float       specular            = kEnergyConservation * pow(max(dot(V, R), 0.0), kShininess);

//   return vec3(mat.specular * specular);
// }


float specular(const uint shininessN, const vec3 viewDir, const vec3 lightDir, const vec3 normal)
{
  vec3 reflectionDir = reflect(lightDir, normal);
  // Overlap of the reflection direction with the primary ray direction (view direction)
  float reflectionOverlap = dot(viewDir, reflectionDir);
  // Specular factor computed as in the first approximation in wikipedia:
  // https://en.wikipedia.org/wiki/Phong_reflection_model#Concepts
  float specularFactor = (reflectionOverlap > 0.f)
                             ? pow(reflectionOverlap * reflectionOverlap,
                                        shininessN)
                             : 0.f;
  return specularFactor;
}
