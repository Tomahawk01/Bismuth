#pragma once

#include "core_physics_types.h"
#include "identifiers/bhandle.h"
#include "physics_types.h"

struct bphysics_system_state;

BAPI b8 bphysics_system_initialize(u64* memory_requirement, struct bphysics_system_state* state, bphysics_system_config* config);
BAPI void bphysics_system_shutdown(struct bphysics_system_state* state);

BAPI b8 bphysics_system_fixed_update(struct bphysics_system_state* state, f64 fixed_update_time);

BAPI b8 bphysics_world_create(struct bphysics_system_state* state, bname name, vec3 gravity, bphysics_world* out_world);
BAPI void bphysics_world_destroy(struct bphysics_system_state* state, bphysics_world* world);

BAPI b8 bphysics_set_world(struct bphysics_system_state* state, bphysics_world* world);

BAPI b8 bphysics_world_add_body(struct bphysics_system_state* state, bphysics_world* world, bhandle body);
BAPI b8 bphysics_world_remove_body(struct bphysics_system_state* state, bphysics_world* world, bhandle body);

BAPI b8 bphysics_body_create_sphere(struct bphysics_system_state* state, bname name, vec3 position, f32 radius, f32 mass, f32 inertia, bhandle* out_body);
BAPI b8 bphysics_body_create_rectangle(struct bphysics_system_state* state, bname name, vec3 position, vec3 half_extents, f32 mass, f32 inertia, bhandle* out_body);
BAPI b8 bphysics_body_create_mesh(struct bphysics_system_state* state, bname name, vec3 position, u32 triangle_count, triangle_3d* tris, f32 mass, f32 inertia, bhandle* out_body);

BAPI void bphysics_body_destroy(struct bphysics_system_state* state, bhandle* body);

BAPI b8 bphysics_body_position_set(struct bphysics_system_state* state, bhandle body, vec3 position);
BAPI b8 bphysics_body_rotation_set(struct bphysics_system_state* state, bhandle body, quat rotation);

BAPI b8 bphysics_body_rotate(struct bphysics_system_state* state, bhandle body, quat rotation);

BAPI b8 bphysics_body_apply_velocity(struct bphysics_system_state* state, bhandle body, vec3 velocity);

BAPI b8 bphysics_body_set_force(struct bphysics_system_state* state, bhandle body, vec3 force);

BAPI b8 bphysics_body_orientation_get(struct bphysics_system_state* state, bhandle body, mat4* out_orientation);
BAPI b8 bphysics_body_velocity_get(struct bphysics_system_state* state, bhandle body, vec3* out_velocity);

BAPI b8 bphysics_body_apply_impulse(struct bphysics_system_state* state, bhandle body, vec3 point, vec3 force);
