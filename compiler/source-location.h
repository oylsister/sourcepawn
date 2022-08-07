// vim: set ts=4 sw=4 tw=99 et:
// 
// Copyright (C) 2022 AlliedModders LLC
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

#include <stdint.h>

// An encoded referece to a location in a source file. We keep this structure
// as small as feasible since our average script can have hundreds of thousands
// of source locations.
class SourceLocation
{
  public:
   friend class MacroLexer;
   friend class SourceFile;
   friend class SourceManager;
   friend struct LREntry;
   friend struct Macro;

   static const uint32_t kInMacro = 0x80000000;

   static SourceLocation Make(uint32_t sourceId, uint32_t offset, uint32_t macro_bit) {
       SourceLocation loc;
       loc.id_ = sourceId + offset;
       loc.id_ |= macro_bit;
       return loc;
   }

    SourceLocation()
     : id_(0)
    {
    }

    bool IsSet() const {
        return id_ != 0;
    }
    bool operator ==(const SourceLocation& other) {
        return id_ == other.id_;
    }
    bool operator !=(const SourceLocation& other) {
        return id_ != other.id_;
    }

    bool IsInMacro() const {
        return !!(id_ & kInMacro);
    }

    uint32_t id() const {
        return id_;
    }

  private:
    uint32_t offset() const {
        return id_ & ~kInMacro;
    }

  protected:
    explicit SourceLocation(uint32_t id)
      : id_(id)
    {}

  private:
    uint32_t id_;
};

struct SourceRange
{
    SourceLocation start;
    SourceLocation end;

    SourceRange()
    {}
    SourceRange(const SourceLocation& start, const SourceLocation& end)
      : start(start),
        end(end)
    {}
};
