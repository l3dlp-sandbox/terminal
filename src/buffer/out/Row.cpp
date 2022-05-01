// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "Row.hpp"
#include "textBuffer.hpp"
#include "../types/inc/convert.hpp"

// Routine Description:
// - constructor
// Arguments:
// - rowWidth - the width of the row, cell elements
// - fillAttribute - the default text attribute
// Return Value:
// - constructed object
ROW::ROW(wchar_t* buffer, uint16_t* indices, const unsigned short rowWidth, const TextAttribute& fillAttribute) :
    _chars{ buffer },
    _charsCapacity{ rowWidth },
    _indices{ indices },
    _indicesCount{ rowWidth },
    _lineRendition{ LineRendition::SingleWidth },
    _wrapForced{ false },
    _doubleBytePadded{ false }
{
    Reset(fillAttribute);
}

// Routine Description:
// - Sets all properties of the ROW to default values
// Arguments:
// - Attr - The default attribute (color) to fill
// Return Value:
// - <none>
bool ROW::Reset(const TextAttribute& Attr)
{
    wmemset(_chars, UNICODE_SPACE, _indicesCount);
    std::iota(_indices, _indices + _indicesCount + 1, static_cast<uint16_t>(0));

    _attr = { gsl::narrow_cast<uint16_t>(_indicesCount), Attr };
    _lineRendition = LineRendition::SingleWidth;
    _wrapForced = false;
    _doubleBytePadded = false;

    return true;
}

// Routine Description:
// - clears char data in column in row
// Arguments:
// - column - 0-indexed column index
// Return Value:
// - <none>
void ROW::ClearColumn(const size_t column)
{
    THROW_HR_IF(E_INVALIDARG, column >= size());
    ClearCell(column);
}

// Routine Description:
// - writes cell data to the row
// Arguments:
// - it - custom console iterator to use for seeking input data. bool() false when it becomes invalid while seeking.
// - index - column in row to start writing at
// - wrap - change the wrap flag if we hit the end of the row while writing and there's still more data in the iterator.
// - limitRight - right inclusive column ID for the last write in this row. (optional, will just write to the end of row if nullopt)
// Return Value:
// - iterator to first cell that was not written to this row.
OutputCellIterator ROW::WriteCells(OutputCellIterator it, const size_t index, const std::optional<bool> wrap, std::optional<size_t> limitRight)
{
    THROW_HR_IF(E_INVALIDARG, index >= size());
    THROW_HR_IF(E_INVALIDARG, limitRight.value_or(0) >= size());

    // If we're given a right-side column limit, use it. Otherwise, the write limit is the final column index available in the char row.
    const auto finalColumnInRow = limitRight.value_or(size() - 1);

    auto currentColor = it->TextAttr();
    uint16_t colorUses = 0;
    auto colorStarts = gsl::narrow_cast<uint16_t>(index);
    auto currentIndex = colorStarts;

    while (it && currentIndex <= finalColumnInRow)
    {
        // Fill the color if the behavior isn't set to keeping the current color.
        if (it->TextAttrBehavior() != TextAttributeBehavior::Current)
        {
            // If the color of this cell is the same as the run we're currently on,
            // just increment the counter.
            if (currentColor == it->TextAttr())
            {
                ++colorUses;
            }
            else
            {
                // Otherwise, commit this color into the run and save off the new one.
                // Now commit the new color runs into the attr row.
                Replace(colorStarts, currentIndex, currentColor);
                currentColor = it->TextAttr();
                colorUses = 1;
                colorStarts = currentIndex;
            }
        }

        // Fill the text if the behavior isn't set to saying there's only a color stored in this iterator.
        if (it->TextAttrBehavior() != TextAttributeBehavior::StoredOnly)
        {
            const auto fillingLastColumn = currentIndex == finalColumnInRow;

            // TODO: MSFT: 19452170 - We need to ensure when writing any trailing byte that the one to the left
            // is a matching leading byte. Likewise, if we're writing a leading byte, we need to make sure we still have space in this loop
            // for the trailing byte coming up before writing it.

            // If we're trying to fill the first cell with a trailing byte, pad it out instead by clearing it.
            // Don't increment iterator. We'll advance the index and try again with this value on the next round through the loop.
            if (currentIndex == 0 && it->DbcsAttr().IsTrailing())
            {
                ClearCell(currentIndex);
            }
            // If we're trying to fill the last cell with a leading byte, pad it out instead by clearing it.
            // Don't increment iterator. We'll exit because we couldn't write a lead at the end of a line.
            else if (fillingLastColumn && it->DbcsAttr().IsLeading())
            {
                ClearCell(currentIndex);
                SetDoubleBytePadded(true);
            }
            // Otherwise, copy the data given and increment the iterator.
            else
            {
                Replace(currentIndex, it->DbcsAttr(), it->Chars());
                ++it;
            }

            // If we're asked to (un)set the wrap status and we just filled the last column with some text...
            // NOTE:
            //  - wrap = std::nullopt    --> don't change the wrap value
            //  - wrap = true            --> we're filling cells as a steam, consider this a wrap
            //  - wrap = false           --> we're filling cells as a block, unwrap
            if (wrap.has_value() && fillingLastColumn)
            {
                // set wrap status on the row to parameter's value.
                SetWrapForced(*wrap);
            }
        }
        else
        {
            ++it;
        }

        // Move to the next cell for the next time through the loop.
        ++currentIndex;
    }

    // Now commit the final color into the attr row
    if (colorUses)
    {
        Replace(colorStarts, currentIndex, currentColor);
    }

    return it;
}
