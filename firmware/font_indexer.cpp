#include "font_indexer.hh"
#include "util.hh"

#include <algorithm>
#include <iterator>

// Flag for marking ranges for deletion
static const uint32_t kDeleteThis = std::numeric_limits<uint32_t>::max();

void FontIndexer::indexFace(const uint8_t id, FT_Face face)
{
    FT_ULong  charcode;
    FT_UInt   gindex;

    const uint32_t kInvalid = std::numeric_limits<uint32_t>::max();

    uint32_t start = kInvalid;
    uint32_t previous = kInvalid;

    std::vector<CodepointRange> face_ranges;

    charcode = FT_Get_First_Char( face, &gindex );
    while ( gindex != 0 )
    {
        charcode = FT_Get_Next_Char( face, charcode, &gindex );

        if (previous == kInvalid) {
            // First codepoint in font
            start = charcode;

        } else if (charcode < previous) {
            // Probably the end of cmap marker
            continue;

        } else if ((charcode - previous) > 1) {
            face_ranges.emplace_back(start, previous, id);
            start = charcode;
        }

        previous = charcode;
    }

    // Capture the last range
    face_ranges.emplace_back(start, previous, id);

    // Merge this font's ranges into the global range table
    mergeRanges(face_ranges);
}

/**
 * Perform a binary search to locate the range containing the passed codepoint
 * Returns the ID associated with that codepoint, if any
 */
uint16_t FontIndexer::find(const uint32_t codepoint)
{
    int32_t left = 0;
    int32_t right = m_ranges.size() - 1;

    while (left <= right)
    {
        uint32_t mid = left + (right - left)/2;

        // Test if codepoint is at the mid-point
        const CodepointRange &range = m_ranges.at(mid);

        if (codepoint >= range.start && codepoint <= range.end) {
            return range.id;
        }

        if (left == right) {
            // Reached last option and it didn't match
            break;
        }

        if (range.start < codepoint) {
            // Ignore left half
            left = mid + 1;
        } else {
            // Ignore right half
            right = mid - 1;
        }
    }

    // Failed to find a range with the codepoint
    return FontIndexer::kCodepointNotFound;
}

uint32_t FontIndexer::countCodepoints()
{
    uint32_t sum = 0;

    for (const CodepointRange &range : m_ranges) {
        sum += range.end - range.start;
    }

    return sum;
}

void FontIndexer::mergeRanges(std::vector<CodepointRange> &incoming_ranges)
{
    if (incoming_ranges.empty()) {
        // Nothing to merge
        return;
    }

    if (m_ranges.empty()) {
        // Merging with nothing - just take the incoming list as-is
        m_ranges = std::move(incoming_ranges);
        return;
    }

    auto existing_iter = m_ranges.begin();

    // Adjust incoming ranges to fit into existing pool
    for (CodepointRange &incoming : incoming_ranges) {

        const auto &start = incoming.start;
        const auto &end = incoming.end;

        while (existing_iter != m_ranges.end()) {
            CodepointRange &existing = *existing_iter;

            if (start > existing.end) {
                // Haven't reached any overlaps yet: keep searching existing ranges
                ++existing_iter;
                continue;
            }

            if (end < existing.start) {
                // No further overlap possible with this range
                // Move onto the next incoming range
                break;
            }

            if (start >= existing.start && end <= existing.end) {
                // Incoming range is contained by an existing range
                // The existing range wins, so mark this incoming range as deleted
                incoming.start = kDeleteThis;

                // Move onto the next incoming range
                break;
            }

            if (existing.start >= start && existing.end <= end) {
                // Existing range is a subset of the incoming range
                // New range is larger so it wins: mark the existing range as deleted
                existing.start = kDeleteThis;
                ++existing_iter;

                // Keep checking if the incoming range overlaps with other existing ranges
                continue;
            }

            // Calculate each range size (+1 as ranges are inclusive)
            const uint32_t incoming_size = (end - start) + 1;
            const uint32_t existing_size = (existing.end - existing.start) + 1;

            if (start <= existing.start && end >= existing.start) {
                // Incoming range overlaps and existing range from the left
                // The larger range keeps the overlap
                if (incoming_size > existing_size) {
                    existing.start = incoming.end + 1;
                } else {
                    incoming.end = existing.start - 1;
                }

            } else if (start <= existing.end && end >= existing.end) {
                // Incoming range overlaps and existing range from the right
                // The larger range keeps the overlap
                if (incoming_size > existing_size) {
                    existing.end = incoming.start - 1;
                } else {
                    incoming.start = existing.end + 1;
                }

            }

            // This range may now be redundant or still be overlap with another range
            // Let it run through a second time now that it has been modified
            continue;
        }

        // Early exit if there are no more existing ranges to compare against
        if (existing_iter == m_ranges.end()) {
            break;
        }
    }

    // Move any incoming ranges into the global store
    m_ranges.insert(
        m_ranges.end(),
        std::make_move_iterator(incoming_ranges.begin()),
        std::make_move_iterator(incoming_ranges.end())
    );

    orderRanges();
}

void FontIndexer::compressRanges()
{
    CodepointRange* target = nullptr;

    for (CodepointRange &range : m_ranges) {
        if (target == nullptr || target->id != range.id) {
            target = &range;
            continue;
        }

        // Absorb this into the current range if it's not too far away
        // The distance here is somewhat arbitrary: a smaller numbers means more memory use
        if (range.start - target->end <= 255) {
            target->end = range.end;
            range.start = kDeleteThis;
        }
    }

    // Delete all ranges marked with kDeleteThis
    orderRanges();

    // Re-allocate to actually free up the space from the deleted items
    shrinkContainer(m_ranges);
}

void FontIndexer::orderRanges()
{
    // Sort new ranges into order by start position
    sort(m_ranges.begin(), m_ranges.end(), CodepointRange::compare_starts);

    // Remove any ranges marked for deletion, which are now at the end of the vector after sorting
    for (size_t i = m_ranges.size() - 1; i != 0; --i) {
        if (m_ranges.at(i).start != kDeleteThis) {
            break;
        }

        m_ranges.pop_back();
    }
}