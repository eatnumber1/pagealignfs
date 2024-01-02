#include "pafs/dir.h"

#include <sys/types.h>
#include <dirent.h>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/status/status.h"
#include "pafs/syscalls.h"
#include "pafs/status.h"
#include "absl/log/log.h"
#include "pafs/fd.h"

namespace pafs {

Directory::Directory(DIR *dir) : dir_(dir) {}

Directory::~Directory() {
  if (dir_ == nullptr) return;
  // Note that a failure to close is not recoverable.
  if (absl::Status st = syscalls::closedir(*dir_); !st.ok()) {
    LOG(ERROR) << st;
  }
}

absl::StatusOr<Directory> Directory::Create(FileDescriptor dirfd) {
  ASSIGN_OR_RETURN(DIR &dir, syscalls::fdopendir(std::move(dirfd)));
  return Directory(&dir);
}

DIR &Directory::operator*() const { return *dir_; }

DIR *Directory::Release() && {
  DIR *dir = dir_;
  dir_ = nullptr;
  return dir;
}

Directory::Directory(Directory &&o) : Directory() {
  *this = std::move(o);
}

Directory &Directory::operator=(Directory &&o) {
  using std::swap;
  swap(dir_, o.dir_);
  return *this;
}

}  // namespace pafs
