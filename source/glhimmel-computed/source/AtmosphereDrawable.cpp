#include <glhimmel-computed/AtmosphereDrawable.h>

#include "glhimmel-computed/ComputedHimmel.h"
#include <glhimmel-base/ScreenAlignedTriangle.h>
#include <glhimmel-computed/AbstractAstronomy.h>
#include <glhimmel-computed/AtmospherePrecompute.h>

#include "shaderfragment/common.h"
#include "shaderfragment/bruneton_common.h"
#include "shaderfragment/pseudo_rand.h"
#include "shaderfragment/dither.h"

#include <osg/Texture2D>
#include <osg/Texture3D>
#include <osg/Depth>
#include <osg/BlendFunc>
#include <osg/Geode>

#include <assert.h>


namespace glHimmel
{

AtmosphereDrawable::AtmosphereDrawable()
:   m_program(new globjects::Program)

,   m_transmittance(NULL)
,   m_irradiance(NULL)
,   m_inscatter(NULL)

,   m_sunScaleFactor(defaultSunScaleFactor())
,   m_exposure(defaultExposure())
,   m_lheurebleue(defaultLHeureBleueColor())
,   m_lheurebleueIntensity(defaultLHeureBleueIntensity())

{
    setupNode(stateSet);
    setupUniforms(stateSet);
    setupShader(stateSet);
    setupTextures(stateSet);

    precompute();

    addDrawable(m_hquad);
}


AtmosphereDrawable::~AtmosphereDrawable()
{
}


void AtmosphereDrawable::update(const Himmel &himmel)
{
    auto sunScale = himmel.astro()->getAngularSunRadius() * m_sunScaleFactor;

    m_program->setUniform("sunScale", sunScale);
    m_program->setUniform("lheurebleueColor", m_lheurebleueColor);
    m_program->setUniform("lheurebleueIntensity", m_lheurebleueIntensity);
    m_program->setUniform("exposure", m_exposure);

    precompute();
}


void AtmosphereDrawable::setupNode(osg::StateSet* stateSet)
{
    osg::Depth* depth = new osg::Depth(osg::Depth::LEQUAL, 1.0, 1.0);    
    stateSet->setAttributeAndModes(depth, osg::StateAttribute::ON);

    osg::BlendFunc *blend  = new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE);
    stateSet->setAttributeAndModes(blend, osg::StateAttribute::ON);
}


void AtmosphereDrawable::setupShader(osg::StateSet* stateSet)
{
    m_vShader->setShaderSource(getVertexShaderSource());

    updateShader(stateSet);

    m_program->addShader(m_vShader);
    m_program->addShader(m_fShader);

    stateSet->setAttributeAndModes(m_program, osg::StateAttribute::ON);
}


void AtmosphereDrawable::updateShader(osg::StateSet*)
{
    std::string fSource(getFragmentShaderSource());
    m_precompute.substituteMacros(fSource);

    m_fShader->setShaderSource(fSource);
}

void AtmosphereDrawable::setupTextures(osg::StateSet* stateSet)
{
    assert(m_precompute);

    m_transmittance = m_precompute.getTransmittanceTexture();
    stateSet->setTextureAttributeAndModes(0, m_transmittance);

    m_irradiance = m_precompute.getIrradianceTexture();
    stateSet->setTextureAttributeAndModes(1, m_irradiance);

    m_inscatter = m_precompute.getInscatterTexture();
    stateSet->setTextureAttributeAndModes(2, m_inscatter);

    stateSet->addUniform(new osg::Uniform("transmitstd::tanceSampler", 0));
    stateSet->addUniform(new osg::Uniform("irradianceSampler", 1));
    stateSet->addUniform(new osg::Uniform("inscatterSampler", 2));
}


void AtmosphereDrawable::precompute()
{   
    if(m_precompute.compute())
        updateShader(getOrCreateStateSet());
}


void AtmosphereDrawable::setSunScaleFactor(const float scale)
{
    m_sunScaleFactor = scale;
}

float AtmosphereDrawable::getSunScaleFactor() const
{
    return m_sunScaleFactor;
}

float AtmosphereDrawable::defaultSunScaleFactor()
{
    return 2.f;
}


void AtmosphereDrawable::setExposure(const float exposure)
{
    m_exposure = exposure;
}

float AtmosphereDrawable::getExposure() const
{
    return m_exposure;
}

float AtmosphereDrawable::defaultExposure()
{
    return 0.22f;
}


void AtmosphereDrawable::setLHeureBleueColor(const glm::vec3 &color)
{
    m_lheurebleueColor = color;

}

glm::vec3 AtmosphereDrawable::getLHeureBleueColor() const
{
    return m_lheurebleueColor;
}

glm::vec3 AtmosphereDrawable::defaultLHeureBleueColor()
{
    return glm::vec3(0.08, 0.3, 1.0);
}


void AtmosphereDrawable::setLHeureBleueIntensity(const float intensity)
{
    m_lheurebleueIntensity = intensity;
}

float AtmosphereDrawable::getLHeureBleueIntensity() const
{
    return m_lheurebleueIntensity;
}

float AtmosphereDrawable::defaultLHeureBleueIntensity()
{
    return 0.5f;
}

void AtmosphereDrawable::setAverageGroundReflectance(const float reflectance)
{
    m_precompute.getModelConfig().avgGroundReflectance = reflectance;
    m_precompute.dirty();
}

void AtmosphereDrawable::setThicknessRayleigh(const float thickness)
{
    m_precompute.getModelConfig().HR = thickness;
    m_precompute.dirty();
}

void AtmosphereDrawable::setScatteringRayleigh(const glm::vec3 &coefficients)
{
    m_precompute.getModelConfig().betaR = coefficients;
    m_precompute.dirty();
}

void AtmosphereDrawable::setThicknessMie(const float thickness)
{
    m_precompute.getModelConfig().HM = thickness;
    m_precompute.dirty();
}

void AtmosphereDrawable::setScatteringMie(const float coefficient)
{
    m_precompute.getModelConfig().betaMSca = glm::vec3(1, 1, 1) * coefficient;
    m_precompute.getModelConfig().betaMEx = m_precompute.getModelConfig().betaMSca / 0.9f;
    m_precompute.dirty();
}

void AtmosphereDrawable::setPhaseG(const float g)
{
    m_precompute.getModelConfig().mieG = g;
    m_precompute.dirty();
}

void AtmosphereDrawable::draw()
{
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_BLEND);

    m_program->use();
    m_screenAlignedTriangle.draw();

    glDepthFunc(GL_LESS);
    glDisable(GL_DEPTH_TEST);
    
    glDisable(GL_BLEND);
}




const std::string AtmosphereDrawable::getVertexShaderSource()
{
    return glsl_version_150()

    +   glsl_quadRetrieveRay()
    +   glsl_quadTransform()

    +   PRAGMA_ONCE(main,

        "out vec4 m_ray;\n"
        "\n"
        "void main(void)\n"
        "{\n"
        "    gl_TexCoord[0] = gl_Vertex * 0.5 + 0.5;\n"
        "\n"
        "    m_ray = quadRetrieveRay();\n"
        "    quadTransform();\n"
        "}");
}


const std::string AtmosphereDrawable::getFragmentShaderSource()
{
    return glsl_version_150()

    +   glsl_cmn_uniform()
    +   
        "uniform vec3 sun;\n"
        "uniform vec3 sunr;\n"

    +   glsl_pseudo_rand()
    +   glsl_dither()

    +   glsl_bruneton_const_RSize()
    +   glsl_bruneton_const_R()
    +   glsl_bruneton_const_M()
    +   glsl_bruneton_const_PI()


    +   "in vec4 m_ray;\n"
        "\n"
        "const float ISun = 100.0;\n"
        "\n"
        //"uniform vec3 c = vec3(cmn[1] + 0.2, 0.0, 0.0);\n"
        "uniform float sunScale;\n"
        "uniform float exposure;\n"
        "\n"
        "uniform vec4 lheurebleue;\n" // rgb and w for instensity
        "\n"

        //"uniform sampler2D reflecstd::tanceSampler;\n" // ground reflecstd::tance texture
        "uniform sampler2D irradianceSampler;\n"    // precomputed skylight irradiance (E table)
        "uniform sampler3D inscatterSampler;\n"     // precomputed inscattered light (S table)
        "\n"

    +   glsl_bruneton_opticalDepth()
    +   glsl_bruneton_transmitstd::tanceUV()
    +   glsl_bruneton_transmitstd::tance()
    +   glsl_bruneton_transmitstd::tanceWithShadow()
    +   glsl_bruneton_analyticTransmitstd::tance()
    +   glsl_bruneton_irradianceUV()
    +   glsl_bruneton_irradiance()
    +   glsl_bruneton_texture4D()
    +   glsl_bruneton_phaseFunctionR()
    +   glsl_bruneton_phaseFunctionM()
    +   glsl_bruneton_mie()
    +   glsl_bruneton_hdr()

    +
        // inscattered light along ray x+tv, when sun in direction s (=S[L]-T(x,x0)S[L]|x0)
        "vec3 inscatter(inout vec3 x, inout float t, vec3 v, vec3 s, out float r, out float mu, out vec3 attenuation) {\n"
        "    vec3 result;\n"
        "    r = length(x);\n"
        "    mu = dot(x, v) / r;\n"
        "    float d = -r * mu - sqrt(r * r * (mu * mu - 1.0) + cmn[2] * cmn[2]);\n"
        "    if (d > 0.0) {\n" // if x in space and ray intersects atmosphere
                // move x to nearest intersection of ray with top atmosphere boundary
        "        x += d * v;\n"
        "        t -= d;\n"
        "        mu = (r * mu + d) / cmn[2];\n"
        "        r = cmn[2];\n"
        "    }\n"
        "    if (r <= cmn[2]) {\n" // if ray intersects atmosphere
        "        float nu = dot(v, s);\n"
        "        float muS = dot(x, s) / r;\n"
        "        float phaseR = phaseFunctionR(nu);\n"
        "        float phaseM = phaseFunctionM(nu);\n"
        "        vec4 inscatter = max(texture4D(inscatterSampler, r, mu, muS, nu), 0.0);\n"
        "        if (t > 0.0) {\n"
        "            vec3 x0 = x + t * v;\n"
        "            float r0 = length(x0);\n"
        "            float rMu0 = dot(x0, v);\n"
        "            float mu0 = rMu0 / r0;\n"
        "            float muS0 = dot(x0, s) / r0;\n"
        //"#ifdef FIX\n"
                    // avoids imprecision problems in transmitstd::tance computations based on textures
        //"            attenuation = analyticTransmitstd::tance(r, mu, t);\n"
        //"#else\n"
        "            attenuation = transmitstd::tance(r, mu, v, x0);\n"
        //"#endif\n"
        "            if (r0 > cmn[1] + 0.01) {\n"
                        // computes S[L]-T(x,x0)S[L]|x0
        "                inscatter = max(inscatter - attenuation.rgbr * texture4D(inscatterSampler, r0, mu0, muS0, nu), 0.0);\n"
        //"#ifdef FIX\n"
        /*                // avoids imprecision problems near horizon by interpolating between two points above and below horizon
        "                const float EPS = 0.004;\n"
        "                float muHoriz = -sqrt(1.0 - (cmn[1] / r) * (cmn[1] / r));\n"
        "                if (abs(mu - muHoriz) < EPS) {\n"
        "                    float a = ((mu - muHoriz) + EPS) / (2.0 * EPS);\n"
        "\n"
        "                    mu = muHoriz - EPS;\n"
        "                    r0 = sqrt(r * r + t * t + 2.0 * r * t * mu);\n"
        "                    mu0 = (r * mu + t) / r0;\n"
        "                    vec4 inScatter0 = texture4D(inscatterSampler, r, mu, muS, nu);\n"
        "                    vec4 inScatter1 = texture4D(inscatterSampler, r0, mu0, muS0, nu);\n"
        "                    vec4 inScatterA = max(inScatter0 - attenuation.rgbr * inScatter1, 0.0);\n"
        "\n"
        "                    mu = muHoriz + EPS;\n"
        "                    r0 = sqrt(r * r + t * t + 2.0 * r * t * mu);\n"
        "                    mu0 = (r * mu + t) / r0;\n"
        "                    inScatter0 = texture4D(inscatterSampler, r, mu, muS, nu);\n"
        "                    inScatter1 = texture4D(inscatterSampler, r0, mu0, muS0, nu);\n"
        "                    vec4 inScatterB = max(inScatter0 - attenuation.rgbr * inScatter1, 0.0);\n"
        "\n"
        "                    inscatter = mix(inScatterA, inScatterB, a);\n"
        "                }\n"
        //"#endif\n"
        */
        "            }\n"
        "        }\n"
        //"#ifdef FIX\n"
                // avoids imprecision problems in Mie scattering when sun is below horizon
        "        inscatter.w *= smoothstep(0.00, 0.02, muS);\n"
        //"#endif\n"
        "        result = max(inscatter.rgb * phaseR + getMie(inscatter) * phaseM, 0.0);\n"
        "    } else {\n" // x in space and ray looking in space
        "        result = vec3(0.0);\n"
        "    }\n"
        "    return result * ISun;\n"
        "}\n"
        "\n"

        // ground radiance at end of ray x+tv, when sun in direction s
        // attenuated bewteen ground and viewer (=R[L0]+R[L*])

        /*        
        "vec3 groundColor(vec3 x, float t, vec3 v, vec3 s, float r, float mu, vec3 attenuation)\n"
        "{\n"
        "    vec3 result;\n"
        "    if (t > 0.0) {\n" // if ray hits ground surface
        "        // ground reflecstd::tance at end of ray, x0\n"
        "        vec3 x0 = x + t * v;\n"
        "        float r0 = length(x0);\n"
        "        vec3 n = x0 / r0;\n"
        //"        vec2 coords = vec2(astd::tan(n.y, n.x), astd::cos(n.z)) * vec2(0.5, 1.0) / PI + vec2(0.5, 0.0);\n"
        //"        vec4 reflecstd::tance = texture2D(reflecstd::tanceSampler, coords) * vec4(0.2, 0.2, 0.2, 1.0);\n"
        "        vec4 reflecstd::tance = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "        if (r0 > cmn[1] + 0.01) {\n"
        "            reflecstd::tance = vec4(0.4, 0.4, 0.4, 0.0);\n"
        "        }\n"
        "\n"
                // direct sun light (radiance) reaching x0
        "        float muS = dot(n, s);\n"
        "        vec3 sunLight = transmitstd::tanceWithShadow(r0, muS);\n"
        "\n"
                // precomputed sky light (irradiance) (=E[L*]) at x0
        "        vec3 groundSkyLight = irradiance(irradianceSampler, r0, muS);\n"
        "\n"
                // light reflected at x0 (=(R[L0] + R[L*]) / T(x, x0))
        "        vec3 groundColor = reflecstd::tance.rgb * (max(muS, 0.0) * sunLight + groundSkyLight) * ISun / PI;\n"
        "\n"
        //"        // water specular color due to sunLight\n"
        //"        if (reflecstd::tance.w > 0.0) {\n"
        //"            vec3 h = normalize(s - v);\n"
        //"            float fresnel = 0.02 + 0.98 * pow(1.0 - dot(-v, h), 5.0);\n"
        //"            float waterBrdf = fresnel * pow(max(dot(h, n), 0.0), 150.0);\n"
        //"            groundColor += reflecstd::tance.w * max(waterBrdf, 0.0) * sunLight * ISun;\n"
        //"        }\n"
        //"\n"
        "        result = attenuation * groundColor;\n" // = R[L0] + R[L*]
        "    } else { // ray looking at the sky\n"
        "        result = vec3(0.0);\n"
        "    }\n"
        "    return result;\n"
        "}\n"
        "\n"
        */

        // direct sun light for ray x+tv, when sun in direction s (=L0)
        "vec3 sunColor(vec3 x, float t, vec3 v, vec3 s, float r, float mu) {\n"
        "    if (t > 0.0) {\n"
        "        return vec3(0.0);\n"
        "    } else {\n"
        "        vec3 transmitstd::tance = r <= cmn[2] ? transmitstd::tanceWithShadow(r, mu) : vec3(1.0);\n" // T(x, xo)
        "        float isun = step(std::cos(sunScale), dot(v, s)) * ISun;\n" // Lsun
        "        return transmitstd::tance * isun;\n" // Eq (9)
        "    }\n"
        "}\n"
        "\n"

        "void main() {\n"
        "    vec3 x = vec3(0.0, 0.0, cmn[1] + cmn[0]);\n"
        "    vec3 ray = normalize(m_ray.xyz);\n"
        "\n"
        "    float r = length(x);\n"
        "    float mu = dot(x, ray) / r;\n"
        "    float t = -r * mu - sqrt(r * r * (mu * mu - 1.0) + cmn[1] * cmn[1]);\n"
        //"\n"
        //"    vec3 g = x - vec3(0.0, 0.0, cmn[1] + 10.0);\n"
        //"    float a = ray.x * ray.x + ray.y * ray.y - ray.z * ray.z;\n"
        //"    float b = 2.0 * (g.x * ray.x + g.y * ray.y - g.z * ray.z);\n"
        //"    float c = g.x * g.x + g.y * g.y - g.z * g.z;\n"
        //"    float d = -(b + sqrt(b * b - 4.0 * a * c)) / (2.0 * a);\n"
        //"    bool cone = d > 0.0 && abs(x.z + d * ray.z - cmn[1]) <= 10.0;\n"
        //"\n"
        //"    if (t > 0.0) {\n"
        //"        if (cone && d < t) {\n"
        //"            t = d;\n"
        //"        }\n"
        //"    } else if (cone) {\n"
        //"        t = d;\n"
        //"    }\n"
        //"\n"
        "    vec3 attenuation;\n"
        "    vec3 inscatterColor = inscatter(x, t, ray, sunr, r, mu, attenuation);\n" // S[L]  - T(x, xs) S[l] | xs"
        //"    vec3 groundColor = groundColor(x, t, ray, sun, r, mu, attenuation);\n"  // R[L0] + R[L*]
        "    vec3 sunColor = sunColor(x, t, ray, sunr, r, mu);\n" // L0
        "\n"

            // l'heure bleue (blaue stunde des ozons)

            // gauss between -12� and +0� sun altitude (Civil & Nautical twilight) 
            // http://en.wikipedia.org/wiki/Twilight
        "    float hb = t > 0.0 ? 0.0 : exp(-pow(sunr.z, 2.0) * 166) + 0.03;\n"     // the +0.03 is for a slight blueish tint at night
        "    vec3 bluehour = lheurebleue.w * lheurebleue.rgb * (dot(ray, sunr) + 1.5) * hb;\n" // * mu (optional..)
        "\n"
        "    gl_FragColor = vec4(HDR(bluehour + sunColor /*+ groundColor*/ + inscatterColor), 1.0)\n"    // Eq (16)
        "        + dither(3, int(cmn[3]));\n"
        "}";
}




#ifdef OSGHIMMEL_EXPOSE_SHADERS

osg::Shader *AtmosphereDrawable::getVertexShader()
{
    return m_vShader;
}
osg::Shader *AtmosphereDrawable::getGeometryShader()
{
    return NULL;
}
osg::Shader *AtmosphereDrawable::getFragmentShader()
{
    return m_fShader;
}

#endif // OSGHIMMEL_EXPOSE_SHADERS

} // namespace glHimmel
