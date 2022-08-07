// vim: set ts=4 sw=4 tw=99 et:
//
// Copyright (C) 2022 David Anderson
//
// This file is part of SourcePawn.
//
// SourcePawn is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// SourcePawn is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// SourcePawn. If not, see http://www.gnu.org/licenses/.
#pragma once

#include <limits.h>

#include <memory>

#include <amtl/am-maybe.h>
#include "stl/stl-unordered-map.h"
#include "stl/stl-vector.h"
#include "source-file.h"
#include "source-location.h"

class CompileContext;

// An LREntry is created each time we register a range of locations (it is
// short for LocationRangeEntry). For a file, an LREntry covers each character
// in the file, including a position for EOF. For macros, it covers the number
// of characters in its token stream, with a position for EOF.
struct LREntry
{
    // Starting id for this source range.
    uint32_t id;

  private:
    // If we're creating a range from an #include, this is the location in the
    // parent file we were #included from, if any.
    //
    // If we're creating a range for macro insertion, this is where we started
    // inserting tokens.
    SourceLocation parent_;

    // If we included from a file, this is where we included.
    std::shared_ptr<SourceFile> file_;

    // If a macro, this holds the size of the macro text, and the value of
    // SourceLocation::kMacroBit.
    uint32_t macro_size_ = 0;
    uint32_t macro_bit_ = 0;

  public:
    LREntry()
     : id(0)
    {}

    bool valid() const { return id != 0; }

    void InitFile(const SourceLocation& parent, std::shared_ptr<SourceFile> file) {
        this->parent_ = parent;
        this->file_ = std::move(file);
    }
    void InitMacro(const SourceLocation& parent, size_t size) {
        this->parent_ = parent;
        this->macro_size_ = size;
        this->macro_bit_ = SourceLocation::kInMacro;
    }

    std::shared_ptr<SourceFile> file() const { return file_; }
    const SourceLocation& parent() const { return parent_; }
    uint32_t length() const { return file_->size(); }

    bool owns(const SourceLocation& loc) const {
        if (loc.offset() >= id && loc.offset() <= id + length())
            return true;
        return false;
    }

    SourceLocation MakeLoc(uint32_t offset) const {
        assert(!file_ || offset <= file_->size());
        assert(!macro_bit_ || offset <= macro_size_);
        return SourceLocation::Make(id, offset, macro_bit_);
    }
};

struct FileLocation {
    std::shared_ptr<SourceFile> file;
    unsigned line = 0;
    unsigned col = 0;
};

class SourceManager final
{
  public:
    explicit SourceManager(CompileContext& cc);

    std::shared_ptr<SourceFile> Open(const std::string& path, const SourceLocation& from);

    LREntry GetLocationRangeEntryForFile(const std::shared_ptr<SourceFile>& file);
    LREntry NewLocationRangeEntryForMacro(const SourceLocation& from, uint32_t size);
    FileLocation GetFileLoc(const SourceLocation& loc);

    // Return the nearest source file containing a SourceLocation. This converts
    // any macro-based location into a file-based location.
    SourceLocation Normalize(const SourceLocation& loc);

    const tr::vector<std::shared_ptr<SourceFile>>& opened_files() const {
        return opened_files_;
    }

  private:
    bool TrackExtents(uint32_t length, size_t* index);
    ke::Maybe<size_t> FindLocation(const SourceLocation& loc);

  private:
    CompileContext& cc_;
    tr::vector<std::shared_ptr<SourceFile>> opened_files_;
    tr::vector<LREntry> locations_;
    ke::Maybe<size_t> last_lookup_;

    // Source ids start from 1. The source file id is 1 + len(source) + 1. This
    // lets us store source locations as a single integer, as we can always
    // bisect to a particular file, and from there, to a line number and column.
    uint32_t next_source_id_ = 1;
};
