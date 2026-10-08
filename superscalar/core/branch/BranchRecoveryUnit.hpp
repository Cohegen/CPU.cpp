#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "BranchResult.hpp"

#if __has_include("superscalar/core/dispatch/ReOrderBuffer.hpp")
#include "superscalar/core/dispatch/ReOrderBuffer.hpp"
#elif __has_include("dispatch/ReOrderBuffer.hpp")
#include "dispatch/ReOrderBuffer.hpp"
#elif __has_include("ReOrderBuffer.hpp")
#include "ReOrderBuffer.hpp"
#else
#include "../dispatch/ReOrderBuffer.hpp"
#endif

#if __has_include("superscalar/core/issue/IssueQueue.hpp")
#include "superscalar/core/issue/IssueQueue.hpp"
#elif __has_include("issue/IssueQueue.hpp")
#include "issue/IssueQueue.hpp"
#elif __has_include("IssueQueue.hpp")
#include "IssueQueue.hpp"
#else
#include "../issue/IssueQueue.hpp"
#endif

#if __has_include("superscalar/core/rename/RegisterAliasTable.hpp")
#include "superscalar/core/rename/RegisterAliasTable.hpp"
#elif __has_include("rename/RegisterAliasTable.hpp")
#include "rename/RegisterAliasTable.hpp"
#elif __has_include("RegisterAliasTable.hpp")
#include "RegisterAliasTable.hpp"
#else
#include "../rename/RegisterAliasTable.hpp"
#endif

#if __has_include("superscalar/core/rename/PhysicalRegisterFreeList.hpp")
#include "superscalar/core/rename/PhysicalRegisterFreeList.hpp"
#elif __has_include("rename/PhysicalRegisterFreeList.hpp")
#include "rename/PhysicalRegisterFreeList.hpp"
#elif __has_include("PhysicalRegisterFreeList.hpp")
#include "PhysicalRegisterFreeList.hpp"
#else
#include "../rename/PhysicalRegisterFreeList.hpp"
#endif

namespace cpu
{

// ─────────────────────────────────────────────────────────────────────────────
// Recovery control signals exposed to the frontend and fetch unit.
// ─────────────────────────────────────────────────────────────────────────────
struct RecoveryControl
{
    // True when a misprediction was detected and state was rolled back.
    bool recover{false};

    // True when the fetch/decode pipeline register must be flushed.
    bool fetch_decode_flush{false};

    // True when the fetch unit PC must be redirected.
    bool fetch_redirect{false};

    // The correct PC to redirect the fetch unit to (valid only when
    // fetch_redirect is true).
    std::uint32_t redirect_pc{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// BranchRecoveryUnit
//
// Stateless coordinator that, on a confirmed misprediction, drives:
//   1. ROB squash  – removes all speculative entries younger than the branch.
//   2. RAT restore – pops the checkpoint taken at dispatch time.
//   3. FreeList    – reclaims every physical register allocated by the
//                    squashed instructions.
//   4. IssueQueue  – removes every squashed ROB entry.
//   5. Frontend    – signals a fetch-decode flush and a PC redirect.
//
// On a correctly-predicted branch it is a no-op; none of the structures are
// touched and all control flags remain false.
// ─────────────────────────────────────────────────────────────────────────────
class BranchRecoveryUnit
{
public:
    // -------------------------------------------------------------------------
    // recover
    //
    // Primary entry point.  Call once per cycle after the branch execution
    // unit has produced a BranchResult.
    //
    // Template parameters allow the ROB and IssueQueue capacities to vary
    // independently of this unit.
    // -------------------------------------------------------------------------
    template <std::size_t ROBCapacity, std::size_t IQCapacity>
    void recover(
        const BranchResult&              result,
        ReOrderBuffer<ROBCapacity>&      rob,
        RegisterAliasTable&              rat,
        PhysicalRegisterFreeList&        free_list,
        IssueQueue<IQCapacity>&          issue_queue
    ) noexcept
    {
        // Reset control signals before every evaluation so that a no-op call
        // cannot carry stale state from a previous misprediction.
        control_ = RecoveryControl{};

        // Nothing to do if the result is invalid or there was no misprediction.
        if (!result.valid || !result.mispredicted)
        {
            return;
        }

        // ── 1. ROB squash ────────────────────────────────────────────────────
        //
        // squash_younger_than returns the ROBEntry objects that were removed,
        // in program order.  The branch instruction itself is kept; it must
        // retire normally so the RAT old_physical_rd can be reclaimed.
        const std::vector<ROBEntry> squashed =
            rob.squash_younger_than(result.rob_index);

        // ── 2. RAT restore ───────────────────────────────────────────────────
        //
        // Pop the checkpoint that was saved when the branch was dispatched.
        rat.restore();

        // ── 3. FreeList – reclaim physical registers ──────────────────────────
        //
        // Every squashed instruction that wrote a physical destination register
        // allocated a new physical register from the free list.  Return those
        // registers now so they can be reused.
        for (const ROBEntry& entry : squashed)
        {
            if (entry.register_write)
            {
                free_list.release(entry.physical_rd);
            }
        }

        // ── 4. IssueQueue squash ─────────────────────────────────────────────
        //
        // Remove every entry whose ROB index matches a squashed instruction.
        // Issued-but-not-yet-committed instructions are still speculative and
        // must be cancelled.
        issue_queue.squash(squashed);

        // ── 5. Frontend control signals ───────────────────────────────────────
        control_.recover           = true;
        control_.fetch_decode_flush = true;
        control_.fetch_redirect    = true;
        control_.redirect_pc       = result.redirect_pc;
    }

    // -------------------------------------------------------------------------
    // control
    //
    // Returns the recovery control signals produced by the most recent call to
    // recover().  Valid for the remainder of the current cycle.
    // -------------------------------------------------------------------------
    [[nodiscard]]
    const RecoveryControl& control() const noexcept
    {
        return control_;
    }

private:
    RecoveryControl control_{};
};

} // namespace cpu
