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
#include "source-manager.h"

#include <amtl/am-arithmetic.h>
#include "errors.h"
#include "lexer.h"

SourceManager::SourceManager(CompileContext& cc)
  : cc_(cc)
{
}

std::shared_ptr<SourceFile> SourceManager::Open(const std::string& path,
                                                const SourceLocation& from)
{
    auto file = std::make_shared<SourceFile>();
    if (!file->Open(path))
	      return nullptr;

    size_t loc_index;
    if (!TrackExtents(file->size(), &loc_index)) {
        report(from, 422);
        return nullptr;
    }

    if (opened_files_.size() >= UINT_MAX) {
        report(from, 422);
        return nullptr;
    }

    file->set_sources_index(opened_files_.size());
    opened_files_.emplace_back(file);

    file->set_location_index(loc_index);
    file->set_location_id(locations_[loc_index].id);
    locations_[loc_index].InitFile(from, file);
    return file;
}

bool SourceManager::TrackExtents(uint32_t length, size_t* index) {
    // We allocate an extra 2 so we can refer to the end-of-file position without
    // colling with the next range.
    uint32_t next_source_id;
    if (!ke::TryUint32Add(next_source_id_, length, &next_source_id) ||
        !ke::TryUint32Add(next_source_id, 2, &next_source_id) ||
        next_source_id > INT_MAX)
    {
        return false;
    }

    *index = locations_.size();

    LREntry tracker;
    tracker.id = next_source_id_;
    locations_.push_back(tracker);

    next_source_id_ = next_source_id;
    return true;
}

LREntry SourceManager::NewLocationRangeEntryForMacro(const SourceLocation& from, uint32_t size) {
    size_t index;
    if (!TrackExtents(size, &index)) {
        report(from, 422);
        return LREntry{};
    }

    locations_[index].InitMacro(from, size);
    return locations_[index];
}

LREntry SourceManager::GetLocationRangeEntryForFile(const std::shared_ptr<SourceFile>& file) {
    return locations_[file->location_index()];
}

SourceLocation SourceManager::Normalize(const SourceLocation& loc) {
    SourceLocation iter = loc;
    while (iter.IsInMacro()) {
        auto loc_index = FindLocation(iter);
        if (!loc_index)
            return {};
        iter = locations_[*loc_index].parent();
    }
    return iter;
}

FileLocation SourceManager::GetFileLoc(const SourceLocation& loc) {
    auto file_loc = Normalize(loc);

    auto loc_index = FindLocation(file_loc);
    if (!loc_index)
        return {};

    auto file = locations_[loc_index.get()].file();
    auto p = file->GetLineAndCol(loc);

    FileLocation where;
    where.file = file;
    where.line = p.first;
    where.col = p.second;
    return where;
}

ke::Maybe<size_t> SourceManager::FindLocation(const SourceLocation& loc) {
    if (!loc.IsSet())
        return {};

    if (last_lookup_ && locations_[last_lookup_.get()].owns(loc))
        return last_lookup_;

    // We should not allocate ids >= the next id.
    assert(loc.offset() < next_source_id_);

    // Binary search.
    size_t lower = 0;
    size_t upper = locations_.size();
    while (lower < upper) {
        size_t index = (lower + upper) / 2;

        LREntry& range = locations_[index];
        if (loc.offset() < range.id) {
            upper = index;
        } else if (loc.offset() > range.id + range.length() + 1) {
            // Note +1 for the terminal offset.
            lower = index + 1;
        } else {
            assert(range.owns(loc));

            // Update cache.
            last_lookup_ = ke::Some(index);
            return last_lookup_;
        }
    }

    // What happened?
    assert(false);
    return {};
}
