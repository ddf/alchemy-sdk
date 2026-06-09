/**
 * @file alchemy/surface/presets.h
 * @brief alchemy::Presets — opt-in preset save/load store with managed
 *        Serializable components.
 *
 * Save / Load walks an ordered list of Serializable* (registered via
 * Manage()) and concatenates their bytes into a single flash slot.
 * The registration order IS the on-flash byte layout.
 *
 * The user does not normally write a Capture/Apply pair: every framework
 * surface they already constructed (Pager, ParamLock<N>, Settings) is a
 * Serializable, and any extra app state can ride along by inheriting
 * Serializable and calling Manage() on it.
 *
 * Slot-level schema gating: every slot is stamped with the XOR of every
 * managed component's SchemaHash().  On Load, a mismatch is treated as
 * an empty slot rather than risking a misaligned restore.  This means
 * adding/removing a managed component, or resizing one (e.g. moving
 * from ParamLock<6> to ParamLock<12>) invalidates existing slots — they
 * disappear instead of corrupting your state.
 *
 * App extras: if you have state outside the standard surfaces, derive
 * a tiny struct from alchemy::Serializable that wraps it, and Manage()
 * the struct.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "alchemy/control/preset_store.h"
#include "alchemy/hw/alchemy_lab_layout.h"  /* PresetFlashOps, kPresetFlashBase, etc. */
#include "alchemy/surface/serializable.h"

namespace daisy { class QSPIHandle; }

namespace alchemy {

/* ── Flash blob sizing ───────────────────────────────────────────────── */

/**
 * Bytes available per slot for the concatenated managed-component payload.
 *
 * Computed from the Alchemy Lab preset region constants minus the on-flash
 * record header (20 B) and our own blob preamble (8 B).  In practice a
 * full Pager + ParamLock<16> + Settings load uses ~17 KiB; we cap a few
 * KiB below the physical maximum to leave room for future surfaces.
 *
 */
constexpr size_t kPresetBlobCapacity =
    static_cast<size_t>(kPresetSectorSize) * kPresetSectorsPerSide
    - 32u;  /* RecordHeader (20) + blob preamble (8) + 4-byte safety margin */

/** Maximum number of Serializables that can be registered via Manage(). */
constexpr uint8_t kPresetMaxManaged = 8u;

/* ── Presets ────────────────────────────────────────────────────────── */

class Presets
{
  public:
    static constexpr uint8_t kNumSlots = kPresetNumSlots;

    /**
     * @param qspi  Daisy QSPI handle.  Not used until Init().
     */
    explicit Presets(daisy::QSPIHandle& qspi);

    /**
     * Register a component for save/load.  Order matters — it defines
     * the on-flash byte layout and contributes to the slot schema hash.
     *
     * Call before Init() (or any Save/Load).  Up to kPresetMaxManaged
     * components; excess registrations are silently dropped.
     */
    void Manage(Serializable& s);

    /**
     * Scan slot headers and prepare the store for Save/Load.  Must be
     * called exactly once, after hw.Init() has set up the QSPI XIP
     * mapping.  Safe to call from main() before StartAudio().
     */
    void Init();

    /** Capture every managed component and write to flash @p slot. */
    bool Save(uint8_t slot);

    /** Restore @p slot's payload into every managed component. */
    bool Load(uint8_t slot);

    /** Restore slot 0 if valid; convenience for boot-time auto-load. */
    bool BootLoad();

    /**
     * True if @p slot holds a CRC-verified record whose schema hash
     * matches the currently-managed set.
     */
    bool HasValid(uint8_t slot) const;

  private:
    /* On-flash payload: schema-hash + length-prefixed byte stream. */
    struct PresetBlob
    {
        uint32_t schema_hash;
        uint16_t length;
        uint16_t reserved;
        uint8_t  bytes[kPresetBlobCapacity];
    } __attribute__((packed));
    static_assert(sizeof(PresetBlob)
                  == 8u + kPresetBlobCapacity,
                  "PresetBlob layout drift");

    /** XOR of every managed component's SchemaHash(). */
    uint32_t SchemaHash() const;

    /** Sum of every managed component's SerializedSize(). */
    size_t   ManagedSize() const;

    daisy::QSPIHandle*                qspi_;
    PresetStore<PresetBlob, kNumSlots> store_;
    Serializable*                     managed_[kPresetMaxManaged] = {};
    uint8_t                           num_managed_ = 0;
};

} // namespace alchemy
