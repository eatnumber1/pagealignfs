#ifndef PAFS_INODE_H_
#define PAFS_INODE_H_

#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <optional>
#include <ostream>

#include "absl/functional/any_invocable.h"
#include "absl/container/flat_hash_map.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "pafs/fd.h"
#include "pafs/inode.h"
#include "pafs/syscalls.h"
#include "pafs/fuse.h"

namespace pafs {

class Inode {
 public:
  static absl::StatusOr<Inode> Create(
      std::string_view path, int parent_fd = AT_FDCWD);

  absl::StatusOr<struct stat> Stat() const;

  ino_t GetNumber() const;
  dev_t GetSourceDevice() const;
  int GetFD() const;
  absl::StatusOr<int> GetGeneration() const;

  void AddPollHandle(FusePollHandle handle);
  absl::Status NotifyPollEvent() const;

  friend std::ostream &operator<<(std::ostream &stream, const Inode &inode);

 private:
  Inode(FileDescriptor fd, ino_t num, dev_t src_dev_num);

  FileDescriptor fd_;
  ino_t num_ = 0;
  dev_t src_dev_num_ = 0;
  mutable std::optional<absl::StatusOr<int>> generation_;
  FusePollHandle poll_handle_;
};

class InodeCache {
 public:
  InodeCache() = default;

  // Insert captures `this` in the returned shared_ptr, so no moves or deletes.
  InodeCache(InodeCache &&) = delete;
  InodeCache(const InodeCache &) = delete;
  InodeCache &operator=(InodeCache &&) = delete;
  InodeCache &operator=(const InodeCache &) = delete;

  // Creates an Inode if it doesn't already exist and inserts it into the cache,
  // returning the resulting Inode.
  //
  // The returned pointer is never null.
  //
  // All shared_ptrs returned by this method must be destroyed before destroying
  // this InodeCache.
  absl::StatusOr<std::shared_ptr<Inode>> Insert(Inode inode);

  // Increment the reference count of a cached inode ntimes.
  //
  // No need to call this if using a shared_ptr returned by Insert above.
  absl::Status Ref(const Inode &inode, uint64_t ntimes = 1);
  // Decrement the reference count of a cached inode ntimes.
  //
  // No need to call this if using a shared_ptr returned by Insert above.
  absl::Status Unref(const Inode &inode, uint64_t ntimes = 1);

 private:
  absl::flat_hash_map<
    std::pair<dev_t, ino_t>,
    std::unique_ptr<std::pair</*refcnt=*/uint64_t, Inode>>> inodes_;
};

}  // namespace pafs

#endif  // PAFS_INODE_H_
