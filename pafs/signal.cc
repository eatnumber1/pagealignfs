#include "pafs/signal.h"

#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <span>
#include <signal.h>
#include <dirent.h>
#include <sys/mount.h>

#include "pafs/syscalls.h"
#include "pafs/status.h"
#include "absl/log/log.h"

namespace pafs {

absl::StatusOr<ScopedSignalMask> ScopedSignalMask::Create(
    int how, const sigset_t &set) {
  sigset_t oldset;
  RETURN_IF_ERROR(syscalls::pthread_sigmask(how, &set, &oldset));
  return ScopedSignalMask(std::move(oldset));
}

ScopedSignalMask::ScopedSignalMask(ScopedSignalMask &&o)
    : ScopedSignalMask() {
  *this = std::move(o);
}

ScopedSignalMask &ScopedSignalMask::operator=(ScopedSignalMask &&o) {
  using std::swap;
  swap(valid_, o.valid_);
  swap(oldset_, o.oldset_);
  return *this;
}

ScopedSignalMask::ScopedSignalMask(sigset_t oldset)
    : valid_(true), oldset_(std::move(oldset)) {}

ScopedSignalMask::~ScopedSignalMask() {
  if (!valid_) return;
  absl::Status st = syscalls::pthread_sigmask(SIG_SETMASK, &oldset_);
  LOG_IF(WARNING, !st.ok()) << "Failed to restore signal disposition " << st;
}

}  // namespace pafs
