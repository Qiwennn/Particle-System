#ifndef _PE_PARTICLE_SYSTEM_H_
#define _PE_PARTICLE_SYSTEM_H_

#include "Mesh.h"
#include "PrimeEngine/APIAbstraction/Effect/EffectManager.h"
#include "PrimeEngine/Scene/SceneNode.h"
#include "PrimeEngine/Utils/Array/Array.h"
#include "PrimeEngine/Math/Vector3.h"
#include "PrimeEngine/Math/Matrix4x4.h"

namespace PE {

struct Vector2
{
    float m_x, m_y;
    Vector2() : m_x(0.0f), m_y(0.0f) {}
    Vector2(float x, float y) : m_x(x), m_y(y) {}
};

namespace Components {

enum Shape { Cone, Sphere };

struct ParticleCPU
{
    Matrix4x4 m_base;
    Vector2 m_size;
    float m_age;
    float m_duration;
    Vector3 velocity;
    
    ParticleCPU()
        : m_base(), m_size(0.1f, 0.1f), m_age(0.0f), m_duration(1.0f), velocity() {}
};

template<typename T>
struct ParticleBufferCPU : public PEAllocatableAndDefragmentable
{
    Array<T> m_values;
    ParticleBufferCPU(PE::GameContext &context, PE::MemoryArena arena)
        : m_values(context, arena) {}
};

struct Particle
{
    PrimitiveTypes::Int16 m_rate;
    PrimitiveTypes::Float32 m_speed;
    PrimitiveTypes::Float32 m_duration;
    PrimitiveTypes::Bool m_looping;
    Vector2 m_size;
    Shape m_shape;
    const char* m_texture;
    Vector3 color;
    
    Particle()
        : m_rate(80)                         
        , m_speed(0.05f)                     
        , m_duration(8.0f)                   
        , m_looping(true)
        , m_size(0.02f, 0.02f)              
        , m_shape(Sphere)                   
        , m_texture("")                     
        , color(0.9f, 0.95f, 0.8f)          
    {
    }


};

struct ParticleSystemCPU : public PE::PEAllocatableAndDefragmentable
{
    ParticleSystemCPU(PE::GameContext &context, PE::MemoryArena arena, Particle particle);
    
    virtual void create(const Matrix4x4& base);
    virtual void createParticleBuffer();
    Vector3 generateVelocity();
    void updateParticleBuffer(PrimitiveTypes::Float32 time);
    
    Handle m_hParticleBufferCPU;
    Handle m_hMaterialSetCPU;
    Matrix4x4 m_base;
    const Particle m_particleTemplate;
    PrimitiveTypes::Float32 m_pastTime;
    PE::MemoryArena m_arena;
    PE::GameContext *m_pContext;
};

struct ParticleSystem : public Mesh
{
    PE_DECLARE_CLASS(ParticleSystem);

    ParticleSystem(PE::GameContext& context, PE::MemoryArena arena, Handle hMyself);

    virtual ~ParticleSystem() {}

    virtual void addDefaultComponents();
    void createParticleSystem(Particle pTemplate);
    virtual void loadParticle_needsRC(int &threadOwnershipMask);

    PE_DECLARE_IMPLEMENT_EVENT_HANDLER_WRAPPER(do_GATHER_DRAWCALLS);
    virtual void do_GATHER_DRAWCALLS(Events::Event *pEvt);

    PE_DECLARE_IMPLEMENT_EVENT_HANDLER_WRAPPER(do_UPDATE);
    virtual void do_UPDATE(Events::Event *pEvt);

    Handle m_hParticleSystemCPU;
    Handle m_meshCPU;
    Matrix4x4 m_offset;
    PrimitiveTypes::Bool m_loaded;
    PrimitiveTypes::Bool m_hasTexture;
    PrimitiveTypes::Bool m_hasColor;
    PE::MemoryArena m_arena;
    PE::GameContext *m_pContext;
};

}; // namespace Components
}; // namespace PE

#endif
