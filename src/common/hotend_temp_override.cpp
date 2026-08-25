#include "hotend_temp_override.hpp"

#include <optional>
#include <marlin_server.hpp>
#include <utils/storage/strong_index_array.hpp>

namespace hotend_temp_override {

namespace {
    using OptionalTemps = StrongIndexArray<std::optional<int16_t>, PhysicalToolIndex::count, PhysicalToolIndex, PhysicalToolIndex::to_raw_static>;
    using OverrideActiveFlags = StrongIndexArray<bool, PhysicalToolIndex::count, PhysicalToolIndex, PhysicalToolIndex::to_raw_static>;

    // RAM-only: per-print bookkeeping, not worth persisting across resets.

    // Active user override per tool, if any.
    OptionalTemps user_override {};

    // The tool's settled target_temp, as last confirmed by an M104 reaffirming an M109 C-wait.
    OptionalTemps settled_temp_of_last_visit {};

    // Whether the override currently wins over gcode's requests, for the rest of this tool's
    // ongoing visit.
    OverrideActiveFlags override_active {};

    // Pending M109 'C' value per tool, waiting to be matched by the M104 that reaffirms it.
    OptionalTemps pending_early_return_temp {};
} // namespace

void note_user_override(PhysicalToolIndex tool, int16_t temp) {
    user_override[tool] = temp;
    override_active[tool] = true;
}

void note_early_return_temp(PhysicalToolIndex tool, int16_t temp) {
    pending_early_return_temp[tool] = temp;
}

int16_t resolve_gcode_target_temp(PhysicalToolIndex tool, int16_t requested_temp) {
    if (pending_early_return_temp[tool] == requested_temp) {
        // This M104 reaffirms the value just waited for via M109 C - the tool's real, settled
        // target_temp for this visit (unlike the earlier ramp steps, never clamped/derived).
        pending_early_return_temp[tool] = std::nullopt;

        if (settled_temp_of_last_visit[tool] == requested_temp) {
            // Same settled target as the tool's previous visit; keep any active override.
            override_active[tool] = user_override[tool].has_value();
        } else {
            // Gcode's own intent for this tool actually changed; the override no longer applies.
            user_override[tool] = std::nullopt;
            override_active[tool] = false;
        }
        settled_temp_of_last_visit[tool] = requested_temp;
    }

    // Restrict actually substituting the override to normal, unpaused printing. Preheat, filament
    // load/unload, calibration, nozzle-cleaning recovery, etc. all route their own M104/M109
    // requests through this same code, but they're deliberate, in-the-moment temperature choices
    // that a leftover print override must never silently clobber.
    const bool override_may_apply = marlin_server::is_printing() && !marlin_server::printer_paused_extended();

    if (override_may_apply && override_active[tool] && user_override[tool].has_value()) {
        return *user_override[tool];
    }
    return requested_temp;
}

void reset() {
    user_override.fill(std::nullopt);
    settled_temp_of_last_visit.fill(std::nullopt);
    override_active.fill(false);
    pending_early_return_temp.fill(std::nullopt);
}

} // namespace hotend_temp_override
