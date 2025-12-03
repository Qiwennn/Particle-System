#include "ParticleSystem.h"
#include "PrimeEngine/Scene/MeshInstance.h"
#include "PrimeEngine/Scene/SceneNode.h"
#include "PrimeEngine/Lua/LuaEnvironment.h"                    
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"  
#include "PrimeEngine/Scene/MeshManager.h"
#include "PrimeEngine/Scene/RootSceneNode.h"
#include "PrimeEngine/Scene/CameraManager.h"
#include "PrimeEngine/Scene/CameraSceneNode.h"
#include "PrimeEngine/Geometry/MeshCPU/MeshCPU.h"
#include "PrimeEngine/Geometry/PositionBufferCPU/PositionBufferCPU.h"
#include "PrimeEngine/Geometry/IndexBufferCPU/IndexBufferCPU.h"
#include "PrimeEngine/Geometry/TexCoordBufferCPU/TexCoordBufferCPU.h"
#include "PrimeEngine/Geometry/ColorBufferCPU.h"
#include "PrimeEngine/Geometry/NormalBufferCPU/NormalBufferCPU.h"
#include "PrimeEngine/Geometry/MaterialCPU/MaterialSetCPU.h"
#include "PrimeEngine/Render/IRenderer.h"

namespace PE {
namespace Components {

PE_IMPLEMENT_CLASS1(ParticleSystem, Mesh);

ParticleSystem::ParticleSystem(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself)
    :Mesh(context, arena, hMyself)
{
    m_offset = Matrix4x4();
    m_offset.setPos(Vector3(.0, .0, .0));
    m_arena = arena;
    m_pContext = &context;
    m_loaded = false;
    m_hasTexture = false;
    m_hasColor = false;
}

void ParticleSystem::addDefaultComponents()
{

    Mesh::addDefaultComponents();
    PE_REGISTER_EVENT_HANDLER(Events::Event_GATHER_DRAWCALLS, ParticleSystem::do_GATHER_DRAWCALLS);
    PE_REGISTER_EVENT_HANDLER(Events::Event_UPDATE, ParticleSystem::do_UPDATE);

}


// ParticleSystemCPU implementation
ParticleSystemCPU::ParticleSystemCPU(PE::GameContext &context, PE::MemoryArena arena, Particle particle)
    : m_particleTemplate(particle)
{
    m_arena = arena;
    m_pContext = &context;
}

void ParticleSystem::createParticleSystem(Particle pTemplate)
{
    PEINFO("=== createParticleSystem called ===\n");

    if (!m_hParticleSystemCPU.isValid())
    {
        m_hParticleSystemCPU = Handle("PARTICLESYSTEMCPU", sizeof(ParticleSystemCPU));
        new (m_hParticleSystemCPU) ParticleSystemCPU(*m_pContext, m_arena, pTemplate);
    }

    ParticleSystemCPU& psysCPU = *m_hParticleSystemCPU.getObject<ParticleSystemCPU>();

    Matrix4x4 particleBase = m_offset;

    Vector3 pos = particleBase.getPos();

    m_hasTexture = strlen(pTemplate.m_texture) > 0;
    m_hasColor = pTemplate.color.m_x != 0 && pTemplate.color.m_y != 0 && pTemplate.color.m_z != 0;
    m_hasColor = true;

    psysCPU.create(particleBase);
    PEINFO("=== createParticleSystem SUCCESS ===\n");
}

void ParticleSystemCPU::create(const Matrix4x4& base)
{
    m_base = Matrix4x4(base);
    createParticleBuffer();
}

void ParticleSystemCPU::createParticleBuffer()
{
    m_hParticleBufferCPU = Handle("PARTICLE_BUFFER_CPU", sizeof(ParticleBufferCPU<ParticleCPU>));
    ParticleBufferCPU<ParticleCPU>* ppbcpu = new(m_hParticleBufferCPU) ParticleBufferCPU<ParticleCPU>(*m_pContext, m_arena);

    const PrimitiveTypes::Int32 maxParticleSize = (PrimitiveTypes::Int32)(m_particleTemplate.m_duration * m_particleTemplate.m_rate);
    ppbcpu->m_values.reset(maxParticleSize);

    int initialParticleCount = m_particleTemplate.m_rate;

    float spawnRadius = 0.5f;

    for (int i = 0; i < initialParticleCount; ++i)
    {
        ParticleCPU newParticle;

        Vector3 basePos = m_base.getPos();

        // randomly
        float r = (rand() / (float)RAND_MAX); // [0,1)
        float theta = (rand() / (float)RAND_MAX) * 2.0f * PrimitiveTypes::Constants::c_Pi_F32;
        float yOffset = ((rand() / (float)RAND_MAX) * 0.5f); // 0 ~ 0.5

        basePos.m_x += cosf(theta) * spawnRadius * r;
        basePos.m_z += sinf(theta) * spawnRadius * r;
        basePos.m_y += yOffset;

        newParticle.m_base = m_base;
        newParticle.m_base.setPos(basePos);

        newParticle.m_size = m_particleTemplate.m_size;
        newParticle.m_duration = m_particleTemplate.m_duration;

        // give random ages
        newParticle.m_age = (rand() / (float)RAND_MAX) * m_particleTemplate.m_duration;

        newParticle.velocity = generateVelocity();

        ppbcpu->m_values.add(newParticle);
    }

    m_hMaterialSetCPU = Handle("MATERIAL_SET_CPU", sizeof(MaterialSetCPU));
    MaterialSetCPU* pmscpu = new(m_hMaterialSetCPU) MaterialSetCPU(*m_pContext, m_arena);
    pmscpu->createSetWithOneTexturedMaterial(m_particleTemplate.m_texture, "Default");

    m_pastTime = 0.0f;
}


void ParticleSystemCPU::updateParticleBuffer(PrimitiveTypes::Float32 time)
{
    static int callCount = 0;
    if (callCount % 60 == 0)
    {
        PEINFO("updateParticleBuffer called, time=%.4f, call#%d\n", time, callCount);
    }
    callCount++;

    ParticleBufferCPU<ParticleCPU>* ppbcpu;
    if (!m_hParticleBufferCPU.isValid())
    {
        m_hParticleBufferCPU = Handle("PARTICLE_BUFFER_CPU", sizeof(ParticleBufferCPU<ParticleCPU>));
        ppbcpu = new(m_hParticleBufferCPU) ParticleBufferCPU<ParticleCPU>(*m_pContext, m_arena);
        const PrimitiveTypes::Int32 maxParticleSize = m_particleTemplate.m_duration * m_particleTemplate.m_rate;
        ppbcpu->m_values.reset(maxParticleSize);
    }
    else
    {
        ppbcpu = m_hParticleBufferCPU.getObject<ParticleBufferCPU<ParticleCPU>>();
    }

    m_pastTime += time;
    int totalParticles = m_pastTime * m_particleTemplate.m_rate;


    // update current particles
    for (int j = 0; j < ppbcpu->m_values.m_size; j++)
    {
        ppbcpu->m_values[j].m_age += time;

        if (ppbcpu->m_values[j].m_age >= ppbcpu->m_values[j].m_duration)
        {
            ParticleCPU newParticle;

            Vector3 basePos = m_base.getPos();
            float spawnRadius = 0.5f;

            float r = (rand() / (float)RAND_MAX);
            float theta = (rand() / (float)RAND_MAX) * 2.0f * PrimitiveTypes::Constants::c_Pi_F32;
            float yOffset = ((rand() / (float)RAND_MAX) * 0.5f);

            basePos.m_x += cosf(theta) * spawnRadius * r;
            basePos.m_z += sinf(theta) * spawnRadius * r;
            basePos.m_y += yOffset;

            newParticle.m_base = m_base;
            newParticle.m_base.setPos(basePos);

            newParticle.m_size = m_particleTemplate.m_size;
            newParticle.m_age = 0.0f;
            newParticle.m_duration = m_particleTemplate.m_duration;
            newParticle.velocity = generateVelocity();

            ppbcpu->m_values[j] = newParticle;
        }


        else
        {
            ParticleCPU& p = ppbcpu->m_values[j];

            float t = p.m_age / p.m_duration;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;

            Vector3 curPos = p.m_base.getPos();

            const float moveScale = 0.02f;

            // follow velocity drifting down
            Vector3 drift = p.velocity * (m_particleTemplate.m_speed * time * moveScale);

            // swirl
            float swirlStrength = 0.1f;   
            float swirlSpeed = 1.0f;   
            float phase = swirlSpeed * p.m_age + j * 0.37f;

            Vector3 swirl(cosf(phase), 0.0f, sinf(phase));
            swirl *= swirlStrength * time * moveScale;

            curPos += drift + swirl;
            p.m_base.setPos(curPos);

            // 3) pulse slightly
            float baseSizeX = m_particleTemplate.m_size.m_x;
            float baseSizeY = m_particleTemplate.m_size.m_y;
            float sizePulse = 0.05f * sinf(p.m_age * 2.0f);  

            p.m_size = Vector2(
                baseSizeX * (1.0f + sizePulse),
                baseSizeY * (1.0f + sizePulse)
            );
        }


    }

    // add new particles
    if (m_particleTemplate.m_looping && totalParticles > ppbcpu->m_values.m_size)
    {
        int maxSize = m_particleTemplate.m_duration * m_particleTemplate.m_rate;
        if (ppbcpu->m_values.m_size < maxSize)
        {
            PrimitiveTypes::Int16 partCount = totalParticles - ppbcpu->m_values.m_size;
            if (partCount + ppbcpu->m_values.m_size > maxSize)
                partCount = maxSize - ppbcpu->m_values.m_size;

            for (int i = 0; i < partCount; ++i)
            {
                ParticleCPU newParticle;

                // —— random position —— //
                Vector3 basePos = m_base.getPos();
                float spawnRadius = 0.5f;  // same as create

                float r = (rand() / (float)RAND_MAX); // [0,1)
                float theta = (rand() / (float)RAND_MAX) * 2.0f * PrimitiveTypes::Constants::c_Pi_F32;
                float yOffset = ((rand() / (float)RAND_MAX) * 0.5f);   

                basePos.m_x += cosf(theta) * spawnRadius * r;
                basePos.m_z += sinf(theta) * spawnRadius * r;
                basePos.m_y += yOffset;

                newParticle.m_base = m_base;
                newParticle.m_base.setPos(basePos);

                newParticle.m_size = m_particleTemplate.m_size;
                newParticle.m_age = 0.0f;
                newParticle.m_duration = m_particleTemplate.m_duration;
                newParticle.velocity = generateVelocity();

                ppbcpu->m_values.add(newParticle);
            }


        }
    }

    // let particle face the camera
    for (int i = 0; i < ppbcpu->m_values.m_size; i++)
    {
        Components::CameraSceneNode* pCam = Components::CameraManager::Instance()->getActiveCamera()->getCamSceneNode();
        Vector3 cameraFront = pCam->m_worldTransform.getN();
        Vector3 cameraRight = pCam->m_worldTransform.getU();
        Vector3 cameraUp = pCam->m_worldTransform.getV();

        ppbcpu->m_values[i].m_base.setN(cameraFront);
        ppbcpu->m_values[i].m_base.setU(cameraRight);
        ppbcpu->m_values[i].m_base.setV(cameraUp);
    }
}
Vector3 ParticleSystemCPU::generateVelocity()
{
    // random horizontal component [-1, 1]
    float rx = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    float rz = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;

    // main direction：down
    float vy = -0.1f - ((rand() / (float)RAND_MAX) * 0.1f); // [-0.1, -0.2]

    Vector3 dir(rx, vy, rz);
    dir.normalize();  

    return dir;
}



void ParticleSystem::loadParticle_needsRC(int& threadOwnershipMask)
{
    static bool firstCall = true;
    if (firstCall)
    {
        firstCall = false;
    }

    MeshCPU* mcpu;
    if (!m_meshCPU.isValid())
    {
        m_meshCPU = Handle("MeshCPU SpriteMesh", sizeof(MeshCPU));
        mcpu = new (m_meshCPU) MeshCPU(*m_pContext, m_arena);
    }
    else
    {
        mcpu = m_meshCPU.getObject<MeshCPU>();
    }

    if (!m_loaded)
    {
        mcpu->createEmptyMesh();
        // mcpu.createBillboardMeshWithColorTexture("cobble2_color.dds", "Default", 32, 32, SamplerState_NoMips_NoMinTex);
    }

    mcpu->m_manualBufferManagement = true;
    ParticleSystemCPU* psysCPU = m_hParticleSystemCPU.getObject<ParticleSystemCPU>();

    ParticleBufferCPU<ParticleCPU>* ppb = psysCPU->m_hParticleBufferCPU.getObject<ParticleBufferCPU<ParticleCPU>>();
    PrimitiveTypes::Int16 particleCount = ppb->m_values.m_size;

    // print particle count
    if (firstCall)
    {
        PEINFO("=== loadParticle_needsRC first call ===\n");
        PEINFO("Particle count: %d\n", particleCount);
        firstCall = false;
    }

    static int lastCount = -1;
    if (particleCount != lastCount)
    {
        PEINFO("Particle count changed: %d -> %d\n", lastCount, particleCount);
        lastCount = particleCount;
    }

    PositionBufferCPU* pvB = mcpu->m_hPositionBufferCPU.getObject<PositionBufferCPU>();
    IndexBufferCPU* pIB = mcpu->m_hIndexBufferCPU.getObject<IndexBufferCPU>();

    ColorBufferCPU* pCB;
    TexCoordBufferCPU* pTCB;
    NormalBufferCPU* pNB;
    MaterialSetCPU* msCPU;

    pvB->m_values.reset(particleCount * 4 * 3); // 4 verts * (x,y,z)
    pIB->m_values.reset(particleCount * 6); // 2 tris

    pIB->m_indexRanges[0].m_start = 0;
    pIB->m_indexRanges[0].m_end = particleCount * 6 - 1;
    pIB->m_indexRanges[0].m_minVertIndex = 0;
    pIB->m_indexRanges[0].m_maxVertIndex = particleCount * 4 - 1;

    pIB->m_minVertexIndex = pIB->m_indexRanges[0].m_minVertIndex;
    pIB->m_maxVertexIndex = pIB->m_indexRanges[0].m_maxVertIndex;

    if (m_hasTexture)
    {
        pTCB = mcpu->m_hTexCoordBufferCPU.getObject<TexCoordBufferCPU>();
        pTCB->m_values.reset(particleCount * 4 * 2);

        pNB = mcpu->m_hNormalBufferCPU.getObject<NormalBufferCPU>();
        pNB->m_values.reset(particleCount * 4 * 3);
    }

    if (m_hasColor)
    {
        pCB = mcpu->m_hColorBufferCPU.getObject<ColorBufferCPU>();
        pCB->m_values.reset(particleCount * 4 * 3);
    }

    for (int i = 0; i < particleCount; i++)
    {
        Matrix4x4 topLeftTransform = ppb->m_values[i].m_base;
        Matrix4x4 topRightTransform = ppb->m_values[i].m_base;
        Matrix4x4 bottomLeftTransform = ppb->m_values[i].m_base;
        Matrix4x4 bottomRightTransform = ppb->m_values[i].m_base;
        Vector2 curSize = ppb->m_values[i].m_size;

        topLeftTransform.moveLeft(curSize.m_x / 2.f);
        topLeftTransform.moveUp(curSize.m_y / 2.f);

        topRightTransform.moveRight(curSize.m_x / 2.f);
        topRightTransform.moveUp(curSize.m_y / 2.f);

        bottomLeftTransform.moveLeft(curSize.m_x / 2.f);
        bottomLeftTransform.moveDown(curSize.m_y / 2.f);

        bottomRightTransform.moveRight(curSize.m_x / 2.f);
        bottomRightTransform.moveDown(curSize.m_y / 2.f);

        pvB->m_values.add(topLeftTransform.getPos().m_x, topLeftTransform.getPos().m_y, topLeftTransform.getPos().m_z); // top lef
        pvB->m_values.add(topRightTransform.getPos().m_x, topRightTransform.getPos().m_y, topRightTransform.getPos().m_z); // top right
        pvB->m_values.add(bottomRightTransform.getPos().m_x, bottomRightTransform.getPos().m_y, bottomRightTransform.getPos().m_z);
        pvB->m_values.add(bottomLeftTransform.getPos().m_x, bottomLeftTransform.getPos().m_y, bottomLeftTransform.getPos().m_z);

        pIB->m_values.add(i * 4 + 0, i * 4 + 1, i * 4 + 2);
        pIB->m_values.add(i * 4 + 2, i * 4 + 3, i * 4 + 0);

        if (m_hasColor)
        {
            ParticleCPU& p = ppb->m_values[i];

            float t = p.m_age / p.m_duration;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;

            // particle goes from dark to bright
            float brightness;
            if (t < 0.2f)
            {
                brightness = t / 0.2f;           // 0 → 1
            }
            else if (t > 0.7f)
            {
                brightness = (1.0f - t) / 0.3f;  // 1 → 0
            }
            else
            {
                brightness = 1.0f;
            }
            if (brightness < 0.0f) brightness = 0.0f;

            Vector3 baseColor = psysCPU->m_particleTemplate.color; // (0.9,0.95,0.8)

            float r = baseColor.m_x * brightness;
            float g = baseColor.m_y * brightness;
            float b = baseColor.m_z * brightness;

            pCB->m_values.add(r, g, b);
            pCB->m_values.add(r, g, b);
            pCB->m_values.add(r, g, b);
            pCB->m_values.add(r, g, b);
        }




        if (m_hasTexture)
        {
            pTCB->m_values.add(0, 0); // top left
            pTCB->m_values.add(1, 0); // top right
            pTCB->m_values.add(1, 1);
            pTCB->m_values.add(0, 1);

            pNB->m_values.add(0, 0, 0);
            pNB->m_values.add(0, 0, 0);
            pNB->m_values.add(0, 0, 0);
            pNB->m_values.add(0, 0, 0);
        }
    }

    if (m_hasColor)
    {
        ColorBufferCPU* pCBCheck = mcpu->m_hColorBufferCPU.getObject<ColorBufferCPU>();

        PEINFO("ColorBuffer floats: %d\n", pCBCheck->m_values.m_size);
        if (pCBCheck->m_values.m_size >= 3)
        {
            PEINFO("First color in buffer: (%.2f, %.2f, %.2f)\n",
                pCBCheck->m_values[0],
                pCBCheck->m_values[1],
                pCBCheck->m_values[2]);
        }
    }

    if (!m_loaded)
    {
        // first time creating gpu mesh
        if (m_hasTexture)
        {
            msCPU = mcpu->m_hMaterialSetCPU.getObject<MaterialSetCPU>();
            memcpy(msCPU, psysCPU->m_hMaterialSetCPU.getObject<MaterialSetCPU>(), sizeof(msCPU));
        }
        loadFromMeshCPU_needsRC(*mcpu, threadOwnershipMask);

        const char* techName = "ColoredMinimalMesh_Tech";
        if (techName && m_hasColor)
        {
            Handle hEffect = EffectManager::Instance()->getEffectHandle(techName);

            for (unsigned int imat = 0; imat < m_effects.m_size; imat++)
            {
                if (m_effects[imat].m_size)
                    m_effects[imat][0] = hEffect;
            }
        }
        m_loaded = true;
    }
    else
    {
        updateGeoFromMeshCPU_needsRC(*mcpu, threadOwnershipMask);
    }
}

void ParticleSystem::do_UPDATE(Events::Event* pEvt)
{
    static int count = 0;
    count++;

    Events::Event_UPDATE* updateEvt = (Events::Event_UPDATE*)(pEvt);
    float dt = updateEvt->m_frameTime / 1000.0f;

    ParticleSystemCPU* psysCPU = m_hParticleSystemCPU.getObject<ParticleSystemCPU>();
    psysCPU->updateParticleBuffer(dt);
}

void ParticleSystem::do_GATHER_DRAWCALLS(PE::Events::Event* pEvt)
{
    static int count = 0;
    if (count == 0) PEINFO("do_GATHER_DRAWCALLS called\n");
    count++;

    // update
    {
        const float dt = 1.0f / 60.0f;

        ParticleSystemCPU* psysCPU = m_hParticleSystemCPU.getObject<ParticleSystemCPU>();
        if (psysCPU)
        {
            psysCPU->updateParticleBuffer(dt);
        }
    }


    Events::Event_GATHER_DRAWCALLS* gatherEvt = (Events::Event_GATHER_DRAWCALLS*)(pEvt);

    // get RenderContext
    m_pContext->getGPUScreen()->AcquireRenderContextOwnership(gatherEvt->m_threadOwnershipMask);

    loadParticle_needsRC(gatherEvt->m_threadOwnershipMask);

    //  release RenderContext
    m_pContext->getGPUScreen()->ReleaseRenderContextOwnership(gatherEvt->m_threadOwnershipMask);
}

} // namespace Components
} // namespace PE
