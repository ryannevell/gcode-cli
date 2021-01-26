/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "gcode-line-reader.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

GCodeLineReader::GCodeLineReader(int fd, size_t buffer_size,
                                 bool remove_comments)
    : fd_(fd), buffer_size_(buffer_size), buffer_(new char[buffer_size]),
      remove_comments_(remove_comments),
      data_begin_(buffer_), data_end_(buffer_) {
}

GCodeLineReader::~GCodeLineReader() {
    close(fd_);
    delete [] buffer_;
}

bool GCodeLineReader::Refill() {
    data_begin_ = buffer_;
    data_end_ = data_begin_;
    if (eof_) return false;
    if (!remainder_.empty()) {
        // get remainder to the beginning of the buffer.
        memmove(data_begin_, remainder_.data(), remainder_.size());
        data_end_ += remainder_.size();
    }
    ssize_t r = read(fd_, data_end_, buffer_size_ - remainder_.size());
    if (r > 0) {
        data_end_ += r;
    } else {
        eof_ = true;
        if (r < 0) perror("Reading input");
        if (!remainder_.empty()) {
            // Close remainder with a newline
            *data_end_++ = '\n';
        }
    }
    remainder_ = {};
    return data_end_ > data_begin_;
}

std::vector<std::string_view> GCodeLineReader::ReadNextLines(size_t n) {
    std::vector<std::string_view> result;
    if (data_begin_ >= data_end_ && !Refill())
        return result;
    const char *const end = data_end_;
    char *end_line;
    while ((end_line = (char*)memchr(data_begin_, '\n', end-data_begin_))) {
        auto line = MakeCommentFreeLine(data_begin_, end_line);
        if (!line.empty()) result.push_back(line);
        data_begin_ = end_line + 1;
        if (result.size() >= n) {
            return result;
        }
    }
    remainder_ = std::string_view(data_begin_, end - data_begin_);
    data_begin_ = data_end_;  // consume all.
    return result;
}

// Note, 'last' points to the last character (typically the newline), not the
// last character + 1 as one would assume in iterators.
std::string_view GCodeLineReader::MakeCommentFreeLine(char *first, char *last) {
    if (remove_comments_) {
        char *start_of_comment = (char*) memchr(first, ';', last - first + 1);
        if (start_of_comment) last = start_of_comment - 1;
    }
    while (first <= last && isspace(*first)) first++;
    while (last >= first && isspace(*last)) last--;  // also removing newline
    if (last < first) return {};
    *++last = '\n';  // Fresh newline behind resulting new last.
    return std::string_view(first, last - first + 1);
}
