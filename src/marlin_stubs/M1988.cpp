#include <marlin_stubs/PrusaGcodeSuite.hpp>

#include <Marlin/src/module/motion.h>
#include <Marlin/src/module/planner.h>
#include <Marlin/src/module/temperature.h>
#include <feature/auto_retract/auto_retract.hpp>
#include <filament.hpp>
#include <logging/log.hpp>
#include <mapi/parking.hpp>
#include <nozzle_cleaner.hpp>
#include <tool/hotend/hotend.hpp>
#include <tool_index.hpp>

LOG_COMPONENT_DEF(NozzleCleaner, logging::Severity::info);

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M1988: Nozzle clean mid-print, returning to the interrupted position
 *
 * Retracts, safely clears the print (never below nozzle_cleaner_approach's height above the
 * tallest point printed so far) and travels to the cleaner, runs a deep clean, then returns to
 * exactly where printing was interrupted and de-retracts.
 *
 *#### Usage
 *
 *    M1988
 */
void PrusaGcodeSuite::M1988() {
    log_info(NozzleCleaner, "M1988: periodic clean starting");

    if (std::holds_alternative<NoTool>(PhysicalToolIndex::currently_selected())) {
        log_info(NozzleCleaner, "M1988: no tool picked, nothing to clean");
        return;
    }

    planner.synchronize(); // flush the look-ahead buffer; current_position is now trustworthy

    const xyze_pos_t resume_position = current_position;

    buddy::auto_retract().maybe_retract_from_nozzle();

    // Safely clears the print (never below 3mm above the tallest point printed so far) and avoids
    // the brush/v-blade area on the way to the cleaner-staging point.
    mapi::park(mapi::get_parking_position(mapi::ParkPosition::nozzle_cleaner_approach));

    if (!nozzle_cleaner::load_and_execute(nozzle_cleaner::Sequence::deep_clean)) {
        log_info(NozzleCleaner, "M1988: deep clean aborted (print stopping?)");
        return; // don't attempt to return to the interrupted position
    }
    nozzle_cleaner::load_and_execute(nozzle_cleaner::Sequence::exit_cleaner);

    // Clear of the cleaner, still above the print, then back down to where printing was
    // interrupted.
    mapi::park(mapi::get_parking_position(mapi::ParkPosition::nozzle_cleaner_exit));
    mapi::park({ .x = resume_position.x, .y = resume_position.y, .z = resume_position.z });

    // Unlike maybe_retract_from_nozzle(), maybe_deretract_to_nozzle() doesn't heat the nozzle
    // itself - it just skips the compensating extrude if too cold, leaving the extruder
    // physically short by the retracted distance. The tool's target can have dropped below
    // extrude-safe during the several seconds a clean takes (e.g. ahead of a tool-offset
    // calibration cooling it down), so ensure it's warm enough here first.
    if (const auto tool = PhysicalToolIndex::currently_selected_opt(); tool.has_value() && thermalManager.tooColdToExtrude(*tool)) {
        Hotend &hotend = Hotend::for_tool(*tool);
        const auto original_temp = hotend.nozzle_target_temp();
        const auto safe_temp = FilamentType::for_current_tool_heuristic().parameters().nozzle_temperature;
        hotend.set_nozzle_target_temp(std::max(original_temp, safe_temp));
        thermalManager.wait_for_hotend(*tool, { .no_wait_for_cooling = true, .early_return_temperature = safe_temp });
        buddy::auto_retract().maybe_deretract_to_nozzle();
        hotend.set_nozzle_target_temp(original_temp);
    } else {
        buddy::auto_retract().maybe_deretract_to_nozzle();
    }

    log_info(NozzleCleaner, "M1988: periodic clean done");
}

/** @}*/
