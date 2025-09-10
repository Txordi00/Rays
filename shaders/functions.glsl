vec3 diffuse(const vec3 diffuseR, const vec3 lightDir, const vec3 normal)
{
  // Lambertian
  float dotNL = max(dot(normal, lightDir), 0.);
  vec3  c     = diffuseR * dotNL;
  return c;
}
