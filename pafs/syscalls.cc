#include "pafs/syscalls.h"

#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <span>
#include <sys/statvfs.h>
#include <sys/xattr.h>
#include <signal.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/file.h>

#include "absl/time/time.h"
#include "pafs/status.h"
#include "absl/log/log.h"
#include "absl/log/check.h"

namespace pafs {
namespace syscalls {

absl::Status close(pafs::FileDescriptor fd) {
  // Failure of close is not recoverable... we must leak the fd.
  int fd_i = std::move(fd).Release();
  errno = 0;
  ::close(fd_i);
  return ErrnoToStatus(errno, absl::StrCat("close(", fd_i, ")"));
}

absl::StatusOr<pafs::FileDescriptor> open(
    const char *pathname, int flags, mode_t mode) {
  int fd = ::open(pathname, flags, mode);
  if (fd == -1) {
    return ErrnoToStatus(errno, absl::StrCat("open('", pathname, "')"));
  }
  return pafs::FileDescriptor(fd);
}

absl::StatusOr<pafs::FileDescriptor> openat(
    int dirfd, const char *pathname, int flags, mode_t mode) {
  int fd = ::openat(dirfd, pathname, flags, mode);
  if (fd == -1) {
    return ErrnoToStatus(errno, absl::StrCat("openat(", dirfd, ", '", pathname, "')"));
  }
  return pafs::FileDescriptor(fd);
}

absl::StatusOr<size_t> read(int fd, void *buf, size_t count) {
  ssize_t nb = ::read(fd, buf, count);
  if (nb == -1) {
    return ErrnoToStatus(errno, absl::StrCat("read(", fd, ")"));
  }
  return nb;
}

absl::StatusOr<pafs::Mount> mount(
    const char *source, std::string target, const char *filesystemtype,
    unsigned long mountflags, const void *data) {
  if (::mount(source, target.c_str(), filesystemtype, mountflags, data) == -1) {
    return ErrnoToStatus(errno, "mount");
  }
  return Mount(std::move(target));
}

absl::StatusOr<struct stat> stat(const char *pathname) {
  struct stat statbuf;
  if (::stat(pathname, &statbuf) == -1) {
    return ErrnoToStatus(errno, absl::StrCat("stat(", pathname, ")"));
  }
  return statbuf;
}

absl::StatusOr<struct stat> lstat(const char *pathname) {
  struct stat statbuf;
  if (::lstat(pathname, &statbuf) == -1) {
    return ErrnoToStatus(errno, absl::StrCat("lstat(", pathname, ")"));
  }
  return statbuf;
}

absl::StatusOr<struct stat> fstatat(int fd, const char *path, int flag) {
  struct stat statbuf;
  if (::fstatat(fd, path, &statbuf, flag) == -1) {
    return ErrnoToStatus(errno, absl::StrCat("fstatat(", fd, ", ", path, ")"));
  }
  return statbuf;
}

absl::Status umount(pafs::Mount mount, int flags) {
  if (const std::string &target = mount.GetTarget();
      ::umount2(target.c_str(), flags) == -1) {
    return ErrnoToStatus(errno, absl::StrCat("umount2(", target, ")"));
  }
  std::move(mount).Release();
  return absl::OkStatus();
}

absl::Status sigaction(
    int signum, const struct sigaction *act, struct sigaction *oldact) {
  if (int rc = ::sigaction(signum, act, oldact); rc != 0) {
    return ErrnoToStatus(errno, "sigaction");
  }
  return absl::OkStatus();
}

absl::StatusOr<pafs::FileDescriptor> signalfd(const sigset_t &mask, int flags) {
  int fd = ::signalfd(/*fd=*/-1, &mask, flags);
  if (fd == -1) return ErrnoToStatus(errno, "signalfd");
  return pafs::FileDescriptor(fd);
}

absl::Status signalfd(int fd, const sigset_t &mask, int flags) {
  errno = 0;
  ::signalfd(fd, &mask, flags);
  return ErrnoToStatus(errno, "signalfd");
}

absl::Status sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  errno = 0;
  ::sigprocmask(how, set, oldset);
  return ErrnoToStatus(errno, "sigprocmask");
}

absl::Status pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset) {
  return ErrnoToStatus(
      ::pthread_sigmask(how, set, oldset), "pthread_sigmask");
}

absl::Status pthread_setschedparam(
    pthread_t thread, int policy, const struct sched_param &param) {
  return ErrnoToStatus(
      ::pthread_setschedparam(thread, policy, &param), "pthread_setschedparam");
}

absl::StatusOr<std::reference_wrapper<DIR>> fdopendir(FileDescriptor fd) {
  DIR *ret = ::fdopendir(std::move(fd).Release());
  if (ret == nullptr) return ErrnoToStatus(errno, "fdopendir");
  return *ret;
}

absl::Status closedir(DIR &dir) {
  errno = 0;
  ::closedir(&dir);
  return ErrnoToStatus(errno, "closedir");
}

absl::StatusOr<struct dirent *> readdir(DIR &dir) {
  errno = 0;
  struct dirent *ent = ::readdir(&dir);
  if (ent == nullptr && errno != 0) return ErrnoToStatus(errno, "readdir");
  return ent;
}

absl::StatusOr<long> telldir(DIR &dir) {
  long loc = ::telldir(&dir);
  if (loc == -1) return ErrnoToStatus(errno, "telldir");
  return loc;
}

absl::Status seekdir(DIR &dir, long loc) {
  ::seekdir(&dir, loc);
  return absl::OkStatus();
}

absl::Status fchmod(int fd, mode_t mode) {
  if (::fchmod(fd, mode) == -1) {
    return ErrnoToStatus(errno, "fchmod");
  }
  return absl::OkStatus();
}

absl::Status fchownat(int fd, std::string_view path, uid_t owner, gid_t group, int flag) {
  int rc = ::fchownat(fd, std::string(path).c_str(), owner, group, flag);
  if (rc == -1) return ErrnoToStatus(errno, "fchownat");
  return absl::OkStatus();
}

absl::Status ftruncate(int fd, off_t length) {
  int rc = ::ftruncate(fd, length);
  if (rc == -1) return ErrnoToStatus(errno, "ftruncate");
  return absl::OkStatus();
}

absl::Status futimens(int fd, const struct timespec times[2]) {
  int rc = ::futimens(fd, times);
  if (rc == -1) return ErrnoToStatus(errno, "futimens");
  return absl::OkStatus();
}

absl::StatusOr<ssize_t> readlinkat(int dirfd, std::string_view pathname, std::span<char> buf) {
  ssize_t rc = ::readlinkat(dirfd, std::string(pathname).c_str(), buf.data(), buf.size());
  if (rc == -1) return ErrnoToStatus(errno, "readlinkat");
  return rc;
}

absl::Status mknodat(int dirfd, std::string_view pathname, mode_t mode, dev_t dev) {
  int rc = ::mknodat(dirfd, std::string(pathname).c_str(), mode, dev);
  if (rc == -1) return ErrnoToStatus(errno, "mknodat");
  return absl::OkStatus();
}

absl::Status mkdirat(int dirfd, std::string_view pathname, mode_t mode) {
  int rc = ::mkdirat(dirfd, std::string(pathname).c_str(), mode);
  if (rc == -1) return ErrnoToStatus(errno, "mkdirat");
  return absl::OkStatus();
}

absl::Status unlinkat(int dirfd, std::string_view pathname, int flags) {
  int rc = ::unlinkat(dirfd, std::string(pathname).c_str(), flags);
  if (rc == -1) return ErrnoToStatus(errno, "unlinkat");
  return absl::OkStatus();
}

absl::Status symlinkat(
    std::string_view target, int newdirfd, std::string_view linkpath) {
  int rc = ::symlinkat(
      std::string(target).c_str(), newdirfd, std::string(linkpath).c_str());
  if (rc == -1) return ErrnoToStatus(errno, "symlinkat");
  return absl::OkStatus();
}

absl::Status renameat(
    int olddirfd, std::string_view oldpath,
    int newdirfd, std::string_view newpath) {
  return renameat2(olddirfd, oldpath, newdirfd, newpath, /*flags=*/0);
}

absl::Status renameat2(
    int olddirfd, std::string_view oldpath,
    int newdirfd, std::string_view newpath,
    unsigned int flags) {
  int rc = ::renameat2(
      olddirfd, std::string(oldpath).c_str(),
      newdirfd, std::string(newpath).c_str(),
      flags);
  if (rc == -1) return ErrnoToStatus(errno, "renameat2");
  return absl::OkStatus();
}

absl::Status linkat(
    int olddirfd, std::string_view oldpath,
    int newdirfd, std::string_view newpath,
    int flags) {
  int rc = ::linkat(
      olddirfd, std::string(oldpath).c_str(),
      newdirfd, std::string(newpath).c_str(),
      flags);
  if (rc == -1) return ErrnoToStatus(errno, "linkat");
  return absl::OkStatus();
}

absl::StatusOr<FileDescriptor> dup(int oldfd) {
  int fd = ::dup(oldfd);
  if (fd == -1) return ErrnoToStatus(errno, "dup");
  return FileDescriptor(fd);
}

absl::Status fsync(int fd) {
  int rc = ::fsync(fd);
  if (rc == -1) return ErrnoToStatus(errno, "fsync");
  return absl::OkStatus();
}

absl::Status fdatasync(int fd) {
  int rc = ::fdatasync(fd);
  if (rc == -1) return ErrnoToStatus(errno, "fdatasync");
  return absl::OkStatus();
}

absl::StatusOr<int> dirfd(DIR &dirp) {
  int fd = ::dirfd(&dirp);
  if (fd == -1) return ErrnoToStatus(errno, "dirfd");
  return fd;
}

absl::StatusOr<struct statvfs> fstatvfs(int fd) {
  struct statvfs buf;
  int rc = ::fstatvfs(fd, &buf);
  if (rc == -1) return ErrnoToStatus(errno, "fstatvfs");
  return std::move(buf);
}

absl::Status fsetxattr(
    int fd, std::string_view name, std::span<const char> value, int flags) {
  int rc = ::fsetxattr(
      fd, std::string(name).c_str(), value.data(), value.size(), flags);
  if (rc == -1) return ErrnoToStatus(errno, "fsetxattr");
  return absl::OkStatus();
}

absl::Status setxattr(
    std::string_view path, std::string_view name, std::span<const char> value,
    int flags) {
  int rc = ::setxattr(
      std::string(path).c_str(), std::string(name).c_str(), value.data(),
      value.size(), flags);
  if (rc == -1) return ErrnoToStatus(errno, "setxattr");
  return absl::OkStatus();
}

absl::StatusOr<size_t> getxattr(
    std::string_view path, std::string_view name, std::span<char> value) {
  ssize_t nb = ::getxattr(
      std::string(path).c_str(), std::string(name).c_str(), value.data(),
      value.size());
  if (nb == -1) return ErrnoToStatus(errno, "getxattr");
  return static_cast<size_t>(nb);
}

absl::StatusOr<std::vector<char>> getxattr(
    std::string_view path, std::string_view name, size_t maxsize) {
  std::vector<char> val(maxsize);
  ASSIGN_OR_RETURN(size_t nb, getxattr(path, name, val));
  CHECK_LE(nb, maxsize);
  val.resize(nb);
  return val;
}

absl::StatusOr<size_t> listxattr(std::string_view path, std::span<char> list) {
  ssize_t nb = ::listxattr(
      std::string(path).c_str(), list.data(), list.size());
  if (nb == -1) return ErrnoToStatus(errno, "listxattr");
  return static_cast<size_t>(nb);
}

absl::Status removexattr(std::string_view path, std::string_view name) {
  int rc = ::removexattr(std::string(path).c_str(), std::string(name).c_str());
  if (rc == -1) return ErrnoToStatus(errno, "removexattr");
  return absl::OkStatus();
}

absl::Status access(std::string_view pathname, int mode) {
  int rc = ::access(std::string(pathname).c_str(), mode);
  if (rc == -1) return ErrnoToStatus(errno, "access");
  return absl::OkStatus();
}

absl::Status faccessat(
    int dirfd, std::string_view pathname, int mode, int flags) {
  int rc = ::faccessat(dirfd, std::string(pathname).c_str(), mode, flags);
  if (rc == -1) return ErrnoToStatus(errno, "faccessat");
  return absl::OkStatus();
}

absl::Status flock(int fd, int operation) {
  int rc = ::flock(fd, operation);
  if (rc == -1) return ErrnoToStatus(errno, "flock");
  return absl::OkStatus();
}

absl::Status fallocate(int fd, int mode, off_t offset, off_t len) {
  int rc = ::fallocate(fd, mode, offset, len);
  if (rc == -1) return ErrnoToStatus(errno, "fallocate");
  return absl::OkStatus();
}

absl::StatusOr<size_t> copy_file_range(
    int fd_in, off_t *off_in,
    int fd_out, off_t *off_out,
    size_t len, unsigned int flags) {
  ssize_t nb = ::copy_file_range(fd_in, off_in, fd_out, off_out, len, flags);
  if (nb == -1) return ErrnoToStatus(errno, "copy_file_range");
  return static_cast<size_t>(nb);
}

absl::StatusOr<off_t> lseek(int fd, off_t offset, int whence) {
  off_t rc = ::lseek(fd, offset, whence);
  if (rc == static_cast<off_t>(-1)) return ErrnoToStatus(errno, "lseek");
  return rc;
}

absl::StatusOr<nfds_t> poll(std::span<pollfd> fds, absl::Duration timeout) {
  // TODO convert this to use ppoll instead.

  int64_t timeout_ms64 = absl::ToInt64Milliseconds(timeout);
  if (timeout_ms64 > INT_MAX || timeout_ms64 < INT_MIN
      || (timeout_ms64 == 0 && timeout != absl::ZeroDuration())) {
    return ErrnoToStatus(
        EINVAL,
        absl::StrCat(
          "Tried to convert ", timeout, " to nonzero int milliseconds"));
  }
  auto timeout_ms = static_cast<int>(timeout_ms64);

  int nelems = ::poll(fds.data(), fds.size(), timeout_ms);
  if (nelems == -1) return ErrnoToStatus(errno, "poll");
  return static_cast<nfds_t>(nelems);
}

}  // namespace syscalls
}  // namespace pafs
