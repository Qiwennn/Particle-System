# Particle System

# Demo Video
![ezgif com-crop](https://github.com/user-attachments/assets/9c21a994-4183-4a0e-bae5-4d9f36fb01a2)


# What’s Implemented

# 1) Particle creation and initial distribution

- Where: `ParticleSystemCPU::createParticleBuffer()` in `ParticleSystem.cpp`.

- What:

  - Store each particle's position, velocity, size, age, and lifetime in a CPU-side buffer.

  - Spawn particles around the emitter with randomized positions and ages, creating an already-active cloud.

  - Use a template to configure the emission rate, speed, lifetime, color, and whether particles loop.

# 2) Lifetime update and motion

- Where: `ParticleSystemCPU::updateParticleBuffer()`.

- What:

  - Advance each particle's age and position, move it using its velocity and a small horizontal swirl, and vary its size over time.

  - Respawn particles at the emitter when their lifetime ends; add particles up to the configured buffer capacity when looping is enabled.

# 3) Camera-facing particles (billboards)

- Where: end of `ParticleSystemCPU::updateParticleBuffer()`.

- What:

  - Read the active camera's right, up, and forward directions.

  - Align each particle's orientation with those directions so its quad faces the camera as the viewpoint moves.

# 4) Mesh generation and rendering

- Where: `ParticleSystem::loadParticle_needsRC()` and `ParticleSystem::do_GATHER_DRAWCALLS()`.

- What:

  - Build one quad per particle using four vertices and two triangles, writing positions and indices into the CPU mesh buffers.

  - Change vertex brightness over the particle's lifetime, producing the yellow spore-like effect in the demo.

  - Upload the mesh on first use, then update its geometry for subsequent frames through the engine's drawing path.


