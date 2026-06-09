/**
 * @file surface/presets.cpp
 * @brief alchemy::Presets implementation.
 */

#include "alchemy/surface/presets.h"

#include <cstring>

namespace alchemy {

Presets::Presets(daisy::QSPIHandle& qspi) : qspi_(&qspi) {}

void Presets::Manage(Serializable& s)
{
    if (num_managed_ >= kPresetMaxManaged) return;
    managed_[num_managed_++] = &s;
}

void Presets::Init()
{
    store_.Init(PresetFlashOps(qspi_),
                kPresetFlashBase, kPresetSectorSize, kPresetSectorsPerSide);
}

uint32_t Presets::SchemaHash() const
{
    uint32_t h = 0xA1C8E51Du;  /* salt — distinguishes "no Manage() at all" */
    for (uint8_t i = 0; i < num_managed_; i++)
    {
        /* Mix position into the hash so reordering invalidates. */
        h ^= managed_[i]->SchemaHash()
           ^ (static_cast<uint32_t>(i) * 0x9E3779B1u);
    }
    return h;
}

size_t Presets::ManagedSize() const
{
    size_t total = 0;
    for (uint8_t i = 0; i < num_managed_; i++)
        total += managed_[i]->SerializedSize();
    return total;
}

bool Presets::Save(uint8_t slot)
{
    const size_t total = ManagedSize();
    if (total > kPresetBlobCapacity) return false;

    PresetBlob blob{};
    blob.schema_hash = SchemaHash();
    blob.length      = static_cast<uint16_t>(total);
    blob.reserved    = 0u;

    uint8_t* cursor = blob.bytes;
    for (uint8_t i = 0; i < num_managed_; i++)
    {
        managed_[i]->Serialize(cursor);
        cursor += managed_[i]->SerializedSize();
    }
    /* Trailing bytes are zero — the on-flash CRC covers them but the
     * length field above tells Load() how many bytes to consume. */

    return store_.Save(slot, blob);
}

bool Presets::Load(uint8_t slot)
{
    if (!HasValid(slot)) return false;

    PresetBlob blob{};
    if (!store_.Load(slot, blob)) return false;
    if (blob.schema_hash != SchemaHash()) return false;
    if (blob.length      != ManagedSize()) return false;

    const uint8_t* cursor = blob.bytes;
    for (uint8_t i = 0; i < num_managed_; i++)
    {
        if (!managed_[i]->Deserialize(cursor)) return false;
        cursor += managed_[i]->SerializedSize();
    }
    return true;
}

bool Presets::BootLoad() { return HasValid(0) && Load(0); }

bool Presets::HasValid(uint8_t slot) const
{
    if (!store_.HasValid(slot)) return false;

    /* Schema-hash gate: peek at the slot's blob and require a match.
     * PresetStore::Load is a const method, but the in-RAM scratch we
     * read into is a stack temporary so this is still safe. */
    PresetBlob blob{};
    if (!store_.Load(slot, blob)) return false;
    return blob.schema_hash == SchemaHash();
}

} // namespace alchemy
