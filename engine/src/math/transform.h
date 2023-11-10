#include "math_types.h"

BAPI transform transform_create(void);

BAPI transform transform_from_position(vec3 position);

BAPI transform transform_from_rotation(quat rotation);

BAPI transform transform_from_position_rotation(vec3 position, quat rotation);

BAPI transform transform_from_position_rotation_scale(vec3 position, quat rotation, vec3 scale);

BAPI transform* transform_parent_get(transform* t);

BAPI void transform_parent_set(transform* t, transform* parent);

BAPI vec3 transform_position_get(const transform* t);

BAPI void transform_position_set(transform* t, vec3 position);

BAPI void transform_translate(transform* t, vec3 translation);

BAPI quat transform_rotation_get(const transform* t);

BAPI void transform_rotation_set(transform* t, quat rotation);

BAPI void transform_rotate(transform* t, quat rotation);

BAPI vec3 transform_scale_get(const transform* t);

BAPI void transform_scale_set(transform* t, vec3 scale);

BAPI void transform_scale(transform* t, vec3 scale);

BAPI void transform_position_rotation_set(transform* t, vec3 position, quat rotation);

BAPI void transform_position_rotation_scale_set(transform* t, vec3 position, quat rotation, vec3 scale);

BAPI void transform_translate_rotate(transform* t, vec3 translation, quat rotation);

BAPI mat4 transform_local_get(transform* t);

BAPI mat4 transform_world_get(transform* t);
