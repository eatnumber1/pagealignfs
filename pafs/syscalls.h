#ifndef PAFS_SYSCALLS_H_
#define PAFS_SYSCALLS_H_

#include <poll.h>
#include <string>
#include <functional>
#include <utility>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/signalfd.h>
#include <sys/xattr.h>
#include <signal.h>
#include <sched.h>
#include <span>
#include <pthread.h>
#include <fcntl.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "pafs/fd.h"
#include "pafs/mount.h"
#include "pafs/status.h"

namespace pafs {
namespace syscalls {

absl::Status close(pafs::FileDescriptor fd);

absl::StatusOr<pafs::FileDescriptor> open(
    const char *pathname, int flags, mode_t mode = 0);
absl::StatusOr<pafs::FileDescriptor> openat(
    int dirfd, const char *pathname, int flags, mode_t mode = 0);

absl::StatusOr<size_t> read(int fd, void *buf, size_t count);

absl::StatusOr<pafs::Mount> mount(
    const char *source, std::string target, const char *filesystemtype,
    unsigned long mountflags = 0, const void *data = nullptr);

absl::Status umount(pafs::Mount mount, int flags = 0);

absl::StatusOr<struct stat> stat(const char *pathname);
absl::StatusOr<struct stat> lstat(const char *pathname);

absl::StatusOr<struct stat> fstatat(int fd, const char *path, int flag = 0);

absl::Status sigaction(
    int signum,
    const struct sigaction *act = nullptr,
    struct sigaction *oldact = nullptr);

absl::StatusOr<pafs::FileDescriptor> signalfd(
    const sigset_t &mask, int flags = 0);
absl::Status signalfd(int fd, const sigset_t &mask, int flags = 0);

absl::Status sigprocmask(
    int how, const sigset_t *set, sigset_t *oldset = nullptr);

absl::Status pthread_sigmask(
    int how, const sigset_t *set, sigset_t *oldset = nullptr);

absl::Status pthread_setschedparam(
    pthread_t thread, int policy, const struct sched_param &param);

absl::StatusOr<int> ioctl(int fd, unsigned long request, auto... args) {
  errno = 0;
  int ret = ::ioctl(fd, request, std::forward<decltype(args)>(args)...);
  if (errno != 0) {
    return absl::ErrnoToStatus(errno, absl::StrCat("ioctl(", fd, ", ", request, ")"));
  }
  return ret;
}

absl::StatusOr<std::reference_wrapper<DIR>> fdopendir(FileDescriptor fd);
absl::Status closedir(DIR &dir);

absl::StatusOr<struct dirent *> readdir(DIR &dir);
absl::StatusOr<long> telldir(DIR &dir);
absl::Status seekdir(DIR &dir, long loc);

absl::Status fchmod(int fd, mode_t mode);
absl::Status fchownat(int fd, std::string_view path, uid_t owner, gid_t group, int flag = 0);
absl::Status ftruncate(int fd, off_t length);
absl::Status futimens(int fd, const struct timespec times[2]);

absl::StatusOr<ssize_t> readlinkat(int dirfd, std::string_view pathname, std::span<char> buf);

absl::Status mknodat(int dirfd, std::string_view pathname, mode_t mode, dev_t dev);
absl::Status mkdirat(int dirfd, std::string_view pathname, mode_t mode);
absl::Status unlinkat(int dirfd, std::string_view pathname, int flags = 0);
absl::Status symlinkat(
    std::string_view target, int newdirfd, std::string_view linkpath);

absl::Status renameat(
    int olddirfd, std::string_view oldpath,
    int newdirfd, std::string_view newpath);
absl::Status renameat2(
    int olddirfd, std::string_view oldpath,
    int newdirfd, std::string_view newpath,
    unsigned int flags);

absl::Status linkat(
    int olddirfd, std::string_view oldpath,
    int newdirfd, std::string_view newpath,
    int flags = 0);

absl::StatusOr<FileDescriptor> dup(int oldfd);

absl::Status fsync(int fd);
absl::Status fdatasync(int fd);

absl::StatusOr<int> dirfd(DIR &dirp);

absl::StatusOr<struct statvfs> fstatvfs(int fd);

absl::Status setxattr(
    std::string_view path, std::string_view name, std::span<const char> value,
    int flags = 0);
absl::Status fsetxattr(
    int fd, std::string_view name, std::span<const char> value, int flags = 0);

absl::StatusOr<size_t> getxattr(
    std::string_view path, std::string_view name, std::span<char> value);
absl::StatusOr<std::vector<char>> getxattr(
    std::string_view path, std::string_view name, size_t maxsize);

absl::StatusOr<size_t> listxattr(std::string_view path, std::span<char> list);

absl::Status removexattr(std::string_view path, std::string_view name);

absl::Status access(std::string_view pathname, int mode);
absl::Status faccessat(
    int dirfd, std::string_view pathname, int mode, int flags = 0);

absl::Status flock(int fd, int operation);
absl::Status fallocate(int fd, int mode, off_t offset, off_t len);

absl::StatusOr<size_t> copy_file_range(
    int fd_in, off_t *off_in,
    int fd_out, off_t *off_out,
    size_t len, unsigned int flags = 0);

absl::StatusOr<off_t> lseek(int fd, off_t offset, int whence);

absl::StatusOr<nfds_t> poll(std::span<pollfd> fds, absl::Duration timeout);

template <typename... Arg>
absl::StatusOr<int> fcntl(int fd, int cmd, Arg... args) {
  int rc = ::fcntl(fd, cmd, std::forward<Arg>(args)...);
  if (rc == -1) return ErrnoToStatus(errno, "fcntl");
  return rc;
}

}  // namespace syscalls
}  // namespace pafs

#endif  // PAFS_SYSCALLS_H_
