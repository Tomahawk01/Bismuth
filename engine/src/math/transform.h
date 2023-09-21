#include "math_types.h"

BAPI transform transform_create();

BAPI transform transform_from_position(vec3 position);

BAPI transform transform_from_rotation(quat rotation);

BAPI transform transform_from_position_rotation(vec3 position, quat rotation);

BAPI transform transform_from_position_rotation_scale(vec3 position, quat rotation, vec3 scale);

BAPI transform* transform_get_parent(transform* t);

BAPI void transform_set_parent(transform* t, transform* parent);

BAPI vec3 transform_get_position(const transform* t);

BAPI void transform_set_position(transform* t, vec3 position);

BAPI void transform_translate(transform* t, vec3 translation);

BAPI quat transform_get_rotation(const transform* t);

BAPI void transform_set_rotation(transform* t, quat rotation);

BAPI void transform_rotate(transform* t, quat rotation);

BAPI vec3 transform_get_scale(const transform* t);

BAPI void transform_set_scale(transform* t, vec3 scale);

BAPI void transform_scale(transform* t, vec3 scale);

BAPI void transform_set_position_rotation(transform* t, vec3 position, quat rotation);

BAPI void transform_set_position_rotation_scale(transform* t, vec3 position, quat rotation, vec3 scale);

BAPI void transform_translate_rotate(transform* t, vec3 translation, quat rotation);

BAPI mat4 transform_get_local(transform* t);

BAPI mat4 transform_get_world(transform* t);
