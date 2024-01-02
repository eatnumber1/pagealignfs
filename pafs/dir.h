#ifndef PAFS_DIR_H_
#define PAFS_DIR_H_

#include <sys/types.h>
#include <dirent.h>

#include "absl/status/statusor.h"
#include "absl/status/status.h"
#include "pafs/fd.h"

namespace pafs {

class Directory {
 public:
  explicit Directory(DIR *dir = nullptr);

  static absl::StatusOr<Directory> Create(FileDescriptor dirfd);

  ~Directory();

  DIR &operator*() const;
  DIR *Release() &&;

  Directory(Directory &&);
  Directory(const Directory &) = delete;
  Directory &operator=(Directory &&);
  Directory &operator=(const Directory &) = delete;

 private:
  DIR *dir_ = nullptr;
};

}  // namespace pafs

#endif  // PAFS_DIR_H_
