// vim: set ts=4 sts=4 sw=4 tw=99 et:
//
//  Copyright (c) AlliedModders LLC 2021
//
//  This software is provided "as-is", without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:
//
//  1.  The origin of this software must not be misrepresented; you must not
//      claim that you wrote the original software. If you use this software in
//      a product, an acknowledgment in the product documentation would be
//      appreciated but is not required.
//  2.  Altered source versions must be plainly marked as such, and must not be
//      misrepresented as being the original software.
//  3.  This notice may not be removed or altered from any source distribution.
#include "source-file.h"

#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
# include <io.h>
#else
# include <unistd.h>
#endif

SourceFile::SourceFile()
  : pos_(0)
{
}

bool
SourceFile::Open(const std::string& file_name)
{
    struct stat s;
    if (stat(file_name.c_str(), &s) != 0)
        return false;
    if ((s.st_mode & S_IFDIR) == S_IFDIR)
        return false;

    std::unique_ptr<FILE, decltype(&::fclose)> fp(fopen(file_name.c_str(), "rb"), &::fclose);
    if (!fp)
        return false;

    if (fseek(fp.get(), 0, SEEK_END) != 0)
        return false;
    auto len = ftell(fp.get());
    if (len == -1L)
        return false;
    if (fseek(fp.get(), 0, SEEK_SET) != 0)
        return false;

    data_.resize(len, '\0');
    data_.shrink_to_fit();
    if (fread(&data_[0], data_.size(), 1, fp.get()) != 1)
        return false;

    name_ = file_name;
    return true;
}

int64_t SourceFile::Pos() {
    return pos_;
}

int SourceFile::Eof() {
    return pos_ == data_.size();
}

std::pair<uint32_t, uint32_t> SourceFile::GetLineAndCol(const SourceLocation &loc) {
    assert(loc.offset() >= location_id_);

    uint32_t pos = loc.offset() - location_id_;
    if (line_offsets_.empty() || pos < line_offsets_[0])
        return {(uint32_t)0, pos};
    if (pos >= line_offsets_.back())
        return {(uint32_t)line_offsets_.size(), (uint32_t)0};

    uint32_t lower = 0;
    uint32_t upper = line_offsets_.size();
    while (lower < upper) {
        uint32_t index = (lower + upper) / 2;
        uint32_t line_start = line_offsets_[index];
        if (pos < line_start) {
            upper = index;
            continue;
        }

        // The range should be (start, end].
        uint32_t line_end = (index < line_offsets_.size() - 1)
                            ? line_offsets_[index + 1]
                            : data_.size();
        if (pos >= line_end) {
            lower = index + 1;
            continue;
        }

        // Either the id is the first character of a line, or before the first
        // character of the next line, or it should be the terminal offset.
        assert(pos >= line_start && pos < line_end);
        return {index + 1, pos - line_start};
    }

    assert(false);
    return {(uint32_t)line_offsets_.size(), (uint32_t)0};
}

