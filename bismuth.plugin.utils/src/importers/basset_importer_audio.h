#pragma once

#include "defines.h"

struct basset;
struct basset_importer;

BAPI b8 basset_importer_audio_import(const struct basset_importer* self, u64 data_size, const void* data, void* params, struct basset* out_asset);
