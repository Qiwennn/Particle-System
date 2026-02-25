# CSCI 522 Milestone — Particle System

# Demo Video
![ezgif com-crop](https://github.com/user-attachments/assets/9c21a994-4183-4a0e-bae5-4d9f36fb01a2)


# What’s Implemented

# 1) Particle system component
- Where: `ParticleSystem.h/.cpp` (`struct ParticleSystem : public Mesh`), `ParticleSystem::addDefaultComponents()`.
- What:
  - Adds a Mesh-derived `ParticleSystem` component with its own `m_offset`, texture/color flags, and engine context.
  - Registers handlers for `Event_UPDATE` and `Event_GATHER_DRAWCALLS` so the system can simulate and render each frame.

# 2) CPU particle template and buffer
- Where: `ParticleSystem.h` (`Particle`, `ParticleCPU`, `ParticleBufferCPU`), `ParticleSystemCPU` constructor and `createParticleBuffer()`.
- What:
  - Defines a configurable `Particle` template (rate, speed, duration, looping, size, shape, texture, color).
  - Allocates a CPU-side `ParticleBufferCPU<ParticleCPU>` sized as `duration * rate`.
  - Stores per-particle transform, size, age, duration, and velocity for simulation.

# 3) Spawn pattern and initial distribution
- Where: `ParticleSystemCPU::createParticleBuffer()`.
- What:
  - Spawns an initial batch of particles using `m_particleTemplate.m_rate`.
  - Positions particles in a small disc around the emitter base with random horizontal offset and slight positive Y offset.
  - Assigns random initial age and a downward-biased, normalized velocity via `generateVelocity()` so the cloud looks already “alive”.

# 4) Lifetime update, motion, and size “breathing”
- Where: `ParticleSystemCPU::updateParticleBuffer()`.
- What:
  - Increments particle age; when age exceeds duration, respawns the particle near the emitter with fresh position, age, and velocity.
  - Applies drift along the particle velocity, plus a small horizontal swirl term based on age and index to avoid rigid motion.
  - Modulates particle size slightly over time (breathing effect) using a sine function on age while keeping a base size from the template.
  - If looping is enabled and total desired particle count exceeds current count, adds new particles up to the max capacity.

# 5) Camera-facing billboards
- Where: end of `ParticleSystemCPU::updateParticleBuffer()`.
- What:
  - Queries the active camera from `CameraManager::Instance()`.
  - For each particle, overwrites its basis (U/V/N) with the camera’s right/up/front vectors so every particle quad faces the camera.

# 6) Mesh rebuild and color over lifetime
- Where: `ParticleSystem::loadParticle_needsRC()`.
- What:
  - Uses the CPU particle buffer to rebuild per-frame quad geometry:
    - For each particle, computes four corner transforms (top-left/right, bottom-left/right) based on its billboard transform and current size.
    - Fills `PositionBufferCPU` (4 vertices per particle) and `IndexBufferCPU` (2 triangles per particle).
  - If color is enabled, computes a brightness factor from normalized lifetime:
    - Dark to bright at birth, stays bright in the middle, then gradually darkens near the end.
    - Multiplies brightness by the template color and writes per-vertex RGB into `ColorBufferCPU`.
  - Optionally sets up texture coordinates and normals if a texture is used.
  - On first load, uploads the mesh to GPU and switches to a colored effect (`ColoredMinimalMesh_Tech`) when color is present; afterward, only updates geo from CPU.

# 7) Event-driven simulation and rendering
- Where: `ParticleSystem::do_UPDATE()`, `ParticleSystem::do_GATHER_DRAWCALLS()`.
- What:
  - `do_UPDATE()`:
    - Reads `dt` from `Event_UPDATE`, logs it, and calls `ParticleSystemCPU::updateParticleBuffer(dt)` once per frame.
  - `do_GATHER_DRAWCALLS()`:
    - Performs a simple fixed-step update (`dt = 1/60`) as a safeguard, then acquires render context ownership.
    - Calls `loadParticle_needsRC()` to rebuild and upload mesh data, then releases the render context.

# 8) Game-side initialization and scene wiring
- Where: `ClientCharacterControlGame.cpp` (particle system initialization block).
- What:
  - Allocates a `ParticleSystem` handle, sets `m_offset` position to `(0, 2, 0)`, and calls `addDefaultComponents()`.
  - Configures a `Particle` template:
    - `m_rate = 50`, `m_speed = 10.0f`, `m_duration = 5.0f`, `m_looping = true`.
    - Small spherical quads (`m_size = (0.03, 0.03)`) with no texture and yellow color `(1, 1, 0)`.
  - Registers the `ParticleSystem` as a mesh asset via `MeshManager::registerAsset()`.
  - Creates a `MeshInstance`, initializes it from the registered asset, and adds it to the `RootSceneNode`.
  - Calls `createParticleSystem(pTemplate)` on the particle system so the CPU simulation starts running at that world position.


