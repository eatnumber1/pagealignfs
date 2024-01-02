#include "pafs/inode.h"

#include <memory>
#include <string_view>
#include <utility>
#include <sys/types.h>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <linux/fs.h>

#include "absl/status/statusor.h"
#include "absl/status/status.h"
#include "pafs/status.h"
#include "absl/functional/any_invocable.h"
#include "absl/utility/utility.h"
#include "pafs/fd.h"
#include "pafs/dir.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "pafs/syscalls.h"

namespace pafs {
namespace {

absl::StatusOr<int> GetFileVersionFromPathFD(int fd) {
  ASSIGN_OR_RETURN(
      FileDescriptor myfd,
      syscalls::open(
        absl::StrCat("/proc/self/fd/", fd).c_str(), O_RDONLY | O_CLOEXEC));

  int version = 0;
  RETURN_IF_ERROR(syscalls::ioctl(*myfd, FS_IOC_GETVERSION, &version).status());
  return version;
}

absl::StatusOr<struct stat> StatFD(int fd) {
  return syscalls::fstatat(
      fd, /*path=*/"", AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW);
}

}  // namespace

absl::StatusOr<std::shared_ptr<Inode>>
InodeCache::Insert(Inode inode) {
  auto key = std::make_pair(inode.GetSourceDevice(), inode.GetNumber());
  auto iter = inodes_.find(key);
  if (iter == inodes_.end()) {
    auto value =
      std::make_unique<std::pair<uint64_t, Inode>>(
        /*refcnt=*/UINT64_C(0), std::move(inode));
    iter = inodes_.emplace_hint(iter, key, std::move(value));
  }

  uint64_t &refcnt = iter->second->first;
  Inode &ino = iter->second->second;

  refcnt++;

  return std::shared_ptr<Inode>(&ino, [this](Inode *i) {
    if (i == nullptr) return;
    LOG_IF_ERROR(WARNING, Unref(*i));
  });
}

absl::Status InodeCache::Ref(const Inode &inode, uint64_t ntimes) {
  auto iter = inodes_.find({inode.GetSourceDevice(), inode.GetNumber()});
  if (iter == inodes_.end()) {
    return absl::InternalError(
        absl::StrCat(
          "Was asked to ref inode ", inode.GetNumber(),
          " tracking src device ", inode.GetSourceDevice(),
          " which we don't have an entry for"));
  }

  uint64_t &refcnt = iter->second->first;
  refcnt += ntimes;

  return absl::OkStatus();
}

absl::Status InodeCache::Unref(const Inode &inode, uint64_t ntimes) {
  auto iter = inodes_.find({inode.GetSourceDevice(), inode.GetNumber()});
  if (iter == inodes_.end()) {
    return absl::InternalError(
        absl::StrCat(
          "Was asked to unref inode ", inode.GetNumber(),
          " tracking src device ", inode.GetSourceDevice(),
          " which we don't have an entry for"));
  }

  uint64_t &refcnt = iter->second->first;
  CHECK_GE(refcnt, ntimes) << inode;
  refcnt -= ntimes;
  if (refcnt == 0) inodes_.erase(iter);

  return absl::OkStatus();
}

absl::StatusOr<struct stat> Inode::Stat() const {
  return StatFD(GetFD());
}

absl::StatusOr<Inode> Inode::Create(
    std::string_view path, int parent_fd) {
  ASSIGN_OR_RETURN(
      FileDescriptor fd,
      syscalls::openat(
        parent_fd, std::string(path).c_str(), O_PATH | O_NOFOLLOW | O_CLOEXEC));

  ASSIGN_OR_RETURN(struct stat st, StatFD(*fd));

  return Inode(std::move(fd), st.st_ino, st.st_dev);
}

Inode::Inode(FileDescriptor fd, ino_t num, dev_t src_dev_num)
  : fd_(std::move(fd)), num_(num), src_dev_num_(src_dev_num) {}

absl::StatusOr<int> Inode::GetGeneration() const {
  if (!generation_) generation_ = GetFileVersionFromPathFD(GetFD());
  return *generation_;
}

int Inode::GetFD() const { return *fd_; }
ino_t Inode::GetNumber() const { return num_; }
dev_t Inode::GetSourceDevice() const { return src_dev_num_; }

void Inode::AddPollHandle(FusePollHandle handle) {
  if (!handle) return;
  poll_handle_ = std::move(handle);
}

absl::Status Inode::NotifyPollEvent() const {
  if (!poll_handle_) return absl::OkStatus();
  return poll_handle_.Notify();
}

std::ostream &operator<<(std::ostream &stream, const Inode &inode) {
  std::string generation = "unknown";
  if (inode.generation_) {
    if (inode.generation_->ok()) {
      generation = absl::StrCat(*(*inode.generation_));
    } else {
      generation = inode.generation_->status().ToString();
    }
  }
  return stream
    << "Inode{num_:" << inode.num_ << ", src_dev_num_:" << inode.src_dev_num_
    << ", fd_:" << *inode.fd_ << ", generation_:" << generation << "}";
}

}  // namespace pafs
