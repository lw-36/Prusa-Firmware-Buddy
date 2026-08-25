/// @file
#pragma once

#include <cstdint>
#include <tool_index.hpp>

/// Lets a user-set nozzle target temperature (Tune menu) survive a toolchange back to that tool,
/// while still respecting a genuine change in what the gcode asks for that tool.
///
/// A toolchange's per-tool cleanup/preheat sequence issues several different M104/M109 S-values in
/// a row (e.g. a retraction-safe temp, an overshoot, then the real target), so comparing every
/// individual request would mistake that ramp for a real change. Instead, the real, settled target
/// temperature for a tool is identified structurally: PrusaSlicer's toolchange script always does
/// `M109 C<target_temp>` (an early-return wait that doesn't change the target) immediately before
/// `M104 S<target_temp>`. Only the M104 that reaffirms the value just waited for via M109's C
/// parameter is compared against the tool's previous visit and gates the override; the earlier ramp
/// steps (which, unlike target_temp, can be clamped/derived and so aren't reliably comparable) pass
/// through unaffected.
namespace hotend_temp_override {

/// Call when a user directly sets @p tool's target temperature (e.g. from the Tune menu).
void note_user_override(PhysicalToolIndex tool, int16_t temp);

/// Call from M109's early-return ('C') handling whenever it's given for @p tool, regardless of
/// whether the same command also carries an S/R target-temp.
void note_early_return_temp(PhysicalToolIndex tool, int16_t temp);

/// Call from gcode-driven temperature setters (M104/M109's S/R) with the value they were given,
/// instead of using that value directly.
/// @return requested_temp, or an active override for @p tool that's still in effect (see above).
int16_t resolve_gcode_target_temp(PhysicalToolIndex tool, int16_t requested_temp);

/// Forgets all overrides and gcode-request history. Call once at the start of every print.
void reset();

} // namespace hotend_temp_override
