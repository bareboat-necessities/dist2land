#include "archive_extract.h"
#include <archive.h>
#include <archive_entry.h>
#include <stdexcept>
#include <filesystem>
#include <string>

static void throw_arch(const std::string& where, struct archive* a) {
  throw std::runtime_error(where + ": " + (archive_error_string(a) ? archive_error_string(a) : "unknown"));
}

void extract_zip(const std::filesystem::path& zip_file, const std::filesystem::path& out_dir) {
  std::filesystem::create_directories(out_dir);

  struct archive* a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);

  if (archive_read_open_filename(a, zip_file.string().c_str(), 10240) != ARCHIVE_OK)
    throw_arch("archive_read_open_filename", a);

  struct archive* ext = archive_write_disk_new();
  archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS);
  archive_write_disk_set_standard_lookup(ext);

  struct archive_entry* entry;
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    const char* p = archive_entry_pathname(entry);
    if (!p) { archive_read_data_skip(a); continue; }

    auto full = out_dir / std::filesystem::path(p);
    archive_entry_set_pathname(entry, full.string().c_str());

    int r = archive_write_header(ext, entry);
    if (r != ARCHIVE_OK) {
      // Some entries might be directories; keep going if not fatal.
    } else {
      const void* buff;
      size_t size;
      la_int64_t offset;
      while (true) {
        int rr = archive_read_data_block(a, &buff, &size, &offset);
        if (rr == ARCHIVE_EOF) break;
        if (rr != ARCHIVE_OK) throw_arch("archive_read_data_block", a);
        if (archive_write_data_block(ext, buff, size, offset) != ARCHIVE_OK) throw_arch("archive_write_data_block", ext);
      }
    }
    archive_write_finish_entry(ext);
  }

  archive_write_close(ext);
  archive_write_free(ext);
  archive_read_close(a);
  archive_read_free(a);
}
