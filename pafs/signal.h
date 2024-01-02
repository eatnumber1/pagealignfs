#ifndef PAFS_SIGNAL_H_
#define PAFS_SIGNAL_H_

#include <string>
#include <functional>
#include <utility>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <sched.h>
#include <span>
#include <pthread.h>
#include <fcntl.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "pafs/fd.h"
#include "pafs/mount.h"

namespace pafs {

// Mask a set of signals (ala pthread_sigmask) and unmask them at destruction.
class ScopedSignalMask {
 public:
  ScopedSignalMask() = default;
  static absl::StatusOr<ScopedSignalMask> Create(int how, const sigset_t &set);

  ScopedSignalMask(ScopedSignalMask &&);
  ScopedSignalMask(const ScopedSignalMask &) = delete;
  ScopedSignalMask &operator=(ScopedSignalMask &&);
  ScopedSignalMask &operator=(const ScopedSignalMask &) = delete;

  ~ScopedSignalMask();

 private:
  ScopedSignalMask(sigset_t oldset);

  bool valid_ = false;
  sigset_t oldset_;
};

}  // namespace pafs

#endif  // PAFS_SIGNAL_H_
