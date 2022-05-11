// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    template<typename T>
    static T extractValueSet(const Windows::Foundation::Collections::ValueSet& set, const std::wstring_view& key, const T& fallback)
    {
        return winrt::unbox_value_or<T>(set.TryLookup(key).try_as<Windows::Foundation::IPropertyValue>(), fallback);
    }

    static til::CoordType extractValueSetCoord(const Windows::Foundation::Collections::ValueSet& set, const std::wstring_view& key, const til::CoordType fallback)
    {
        return gsl::narrow<til::CoordType>(extractValueSet<uint32_t>(set, key, fallback));
    }
}
