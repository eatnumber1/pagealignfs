#ifndef FUSE_USE_VERSION
// Needed by fuse/fuse_lowlevel.h
#define FUSE_USE_VERSION 312
#elif FUSE_USE_VERSION != 312
#error this file is written for fuse 3.12
#endif

#include "pafs/page_align_fs.h"

#include <cstdint>
#include <poll.h>
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <linux/fs.h>

#include "pafs/inode.h"
#include "absl/functional/any_invocable.h"
#include "absl/cleanup/cleanup.h"
#include "absl/utility/utility.h"
#include "pafs/fd.h"
#include "pafs/dir.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "fuse/fuse_lowlevel.h"
#include "pafs/fuse.h"
#include "pafs/syscalls.h"


namespace pafs {

absl::Status PageAlignFS::Init(struct fuse_conn_info &conn) {
  LOG(INFO)
    << "Fuse connection using kernel protocol version " << conn.proto_major
    << "." << conn.proto_minor;
  LOG(INFO) << "Maximum write buffer size is " << conn.max_write;
  // TODO allow setting this  when `-o max_read=<n>` is used.
  LOG(INFO) << "Maximum read buffer size is " << conn.max_read;
  LOG(INFO) << "Maximum readahead is " << conn.max_readahead;
  LOG(INFO) << "Maximum background requests is " << conn.max_background;
  LOG(INFO) << "Congestion threshold is " << conn.congestion_threshold;
  // TODO set FUSE_CAP_EXPORT_SUPPORT
  return absl::OkStatus();
}

// TODO remove this method?
absl::Status PageAlignFS::Destroy() {
  LOG(INFO) << "Destroy()";
  return absl::OkStatus();
}

absl::Status PageAlignFS::Forget(
    FuseRequest &req, fuse_ino_t ino, uint64_t nlookup) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "Forget() ino:" << inode << ", nlookup:" << nlookup;
  return inodes_.Unref(inode, nlookup);
}

absl::Status PageAlignFS::Lookup(
    FuseRequest &req, fuse_ino_t parent, std::string_view name) {
  Inode &parent_ino = GetInode(parent);
  LOG(INFO) << "Lookup() parent:" << parent_ino << ", name:" << name;
  return ReplyWithLookup(req, parent_ino, name);
}

absl::Status PageAlignFS::GetAttr(FuseRequest &req, fuse_ino_t ino) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "GetAttr() ino:" << inode;
  return ReplyWithAttrs(req, inode);
}

absl::Status PageAlignFS::SetAttr(
    FuseRequest &req, fuse_ino_t ino, struct stat &attr, int to_set,
    struct fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "SetAttr() ino:" << inode;

  std::optional<FileDescriptor> myfd;
  int fd = fi.fh;
  if (fd == 0) {
    ASSIGN_OR_RETURN(
        myfd,
        syscalls::open(
          absl::StrCat("/proc/self/fd/", inode.GetFD()).c_str(),
          O_RDONLY | O_CLOEXEC));
    fd = *(*myfd);
  }

  if (to_set & FUSE_SET_ATTR_MODE) {
    RETURN_IF_ERROR(syscalls::fchmod(fd, attr.st_mode));
  }

  if (to_set & (FUSE_SET_ATTR_UID | FUSE_SET_ATTR_GID)) {
    auto uid = static_cast<uid_t>(-1);
    if (to_set & FUSE_SET_ATTR_UID) uid = attr.st_uid;
    auto gid = static_cast<gid_t>(-1);
    if (to_set & FUSE_SET_ATTR_GID) gid = attr.st_gid;

    RETURN_IF_ERROR(
        syscalls::fchownat(
          fd, /*path=*/"", uid, gid, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW));
  }

  if (to_set & FUSE_SET_ATTR_SIZE) {
    RETURN_IF_ERROR(syscalls::ftruncate(fd, attr.st_size));
  }

  if (to_set & (FUSE_SET_ATTR_ATIME | FUSE_SET_ATTR_MTIME)) {
    struct timespec tv[2];

    tv[0].tv_sec = 0;
    tv[1].tv_sec = 0;
    tv[0].tv_nsec = UTIME_OMIT;
    tv[1].tv_nsec = UTIME_OMIT;

    if (to_set & FUSE_SET_ATTR_ATIME_NOW) {
      tv[0].tv_nsec = UTIME_NOW;
    } else if (to_set & FUSE_SET_ATTR_ATIME) {
      tv[0] = attr.st_atim;
    }

    if (to_set & FUSE_SET_ATTR_MTIME_NOW) {
      tv[1].tv_nsec = UTIME_NOW;
    } else if (to_set & FUSE_SET_ATTR_MTIME) {
      tv[1] = attr.st_mtim;
    }

    RETURN_IF_ERROR(syscalls::futimens(fd, tv));
  }

  return ReplyWithAttrs(req, inode);
}

absl::Status PageAlignFS::ReadLink(FuseRequest &req, fuse_ino_t ino) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "ReadLink() ino:" << inode;

  // TODO make this a dynamically growing buffer
  char buf[PATH_MAX + 1];
  ASSIGN_OR_RETURN(
      ssize_t nb,
      syscalls::readlinkat(inode.GetFD(), /*pathname=*/"", {buf, sizeof(buf)}));
  if (nb == sizeof(buf)) return ErrnoToStatus(ENAMETOOLONG, "Path too long");

  return req.ReplyReadLink({buf, static_cast<size_t>(nb)});
}

absl::Status PageAlignFS::OpenDir(
    FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "OpenDir() ino:" << inode;
  ASSIGN_OR_RETURN(
      FileDescriptor dirfd,
      syscalls::openat(
        inode.GetFD(), /*path=*/".", O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
  ASSIGN_OR_RETURN(auto d, Directory::Create(std::move(dirfd)));
  auto *dir = new Directory(std::move(d));
  static_assert(sizeof(fi.fh) >= sizeof(dir));
  fi.fh = reinterpret_cast<decltype(fi.fh)>(dir);
  return req.ReplyOpen(fi);
}

absl::Status PageAlignFS::ReleaseDir(
    FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "ReleaseDir() ino:" << inode;
  delete reinterpret_cast<Directory *>(fi.fh);
  return absl::OkStatus();
}

absl::Status PageAlignFS::ReadDir(
    FuseRequest &req, fuse_ino_t ino, size_t size, off_t off,
    fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO)
    << "ReadDir() ino:" << inode << ", off:" << off << ", size:" << size;
  return ReadDirInternal(req, inode, size, off, fi, /*plus=*/false);
}

absl::Status PageAlignFS::ReadDirPlus(
    FuseRequest &req, fuse_ino_t ino, size_t size, off_t off,
    fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO)
    << "ReadDirPlus() ino:" << inode << ", off:" << off << ", size:" << size;
  return ReadDirInternal(req, inode, size, off, fi, /*plus=*/true);
}

absl::Status PageAlignFS::ForgetMulti(
    FuseRequest &req, std::span<fuse_forget_data> forgets) {
  LOG(INFO) << "ForgetMulti() forgets:" << forgets.size();
  // TODO implement this as a batch call into inodes_
  for (const fuse_forget_data &forget : forgets) {
    RETURN_IF_ERROR(inodes_.Unref(GetInode(forget.ino), forget.nlookup));
  }
  return absl::OkStatus();
}

absl::Status PageAlignFS::Mknod(
    FuseRequest &req, fuse_ino_t parent, std::string_view name, mode_t mode,
    dev_t rdev) {
  Inode &parent_ino = GetInode(parent);
  LOG(INFO)
    << "Mknod() parent:" << parent_ino << ", name:" << name << ", mode:" << mode
    << ", rdev:" << rdev;
  RETURN_IF_ERROR(
      syscalls::mknodat(
        parent_ino.GetFD(), std::string(name).c_str(), mode, rdev));
  return ReplyWithLookup(req, parent_ino, name);
}

absl::Status PageAlignFS::Mkdir(
    FuseRequest &req, fuse_ino_t parent, std::string_view name, mode_t mode) {
  Inode &parent_ino = GetInode(parent);
  LOG(INFO)
    << "Mkdir() parent:" << parent_ino << ", name:" << name << ", mode:"
    << mode;
  RETURN_IF_ERROR(
      syscalls::mkdirat(
        parent_ino.GetFD(), std::string(name).c_str(), mode));
  return ReplyWithLookup(req, parent_ino, name);
}

absl::Status PageAlignFS::Unlink(
    FuseRequest &req, fuse_ino_t ino, std::string_view name) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "Unlink() ino:" << inode << ", name:" << name;
  return syscalls::unlinkat(inode.GetFD(), std::string(name).c_str());
}

absl::Status PageAlignFS::Rmdir(
    FuseRequest &req, fuse_ino_t ino, std::string_view name) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "Rmdir() ino:" << inode << ", name:" << name;
  return syscalls::unlinkat(
        inode.GetFD(), std::string(name).c_str(), AT_REMOVEDIR);
}

absl::Status PageAlignFS::Symlink(
    FuseRequest &req, std::string_view link, fuse_ino_t parent,
    std::string_view name) {
  Inode &parent_ino = GetInode(parent);
  LOG(INFO)
    << "Symlink() link:" << link << ", parent:" << parent_ino << ", name:"
    << name;
  RETURN_IF_ERROR(
      syscalls::symlinkat(
        std::string(link).c_str(), parent_ino.GetFD(),
        std::string(name).c_str()));
  return ReplyWithLookup(req, parent_ino, name);
}

absl::Status PageAlignFS::Rename(
    FuseRequest &req, fuse_ino_t parent, std::string_view name,
    fuse_ino_t newparent, std::string_view newname, unsigned int flags) {
  Inode &parent_ino = GetInode(parent);
  Inode &newparent_ino = GetInode(newparent);
  LOG(INFO)
    << "Rename() parent:" << parent_ino << ", name:" << name << ", newparent:"
    << newparent_ino << ", newname:" << newname << ", flags:" << flags;

  return syscalls::renameat2(
      parent_ino.GetFD(), name,
      newparent_ino.GetFD(), newname,
      flags);
}

absl::Status PageAlignFS::Link(
    FuseRequest &req, fuse_ino_t ino,
    fuse_ino_t newparent, std::string_view newname) {
  Inode &inode = GetInode(ino);
  Inode &newparent_ino = GetInode(newparent);
  LOG(INFO)
    << "Link() ino:" << inode << ", newparent:" << newparent_ino << ", newname:"
    << newname;

  RETURN_IF_ERROR(syscalls::linkat(
      inode.GetFD(), /*oldpath=*/"",
      newparent_ino.GetFD(), newname,
      AT_EMPTY_PATH));

  return ReplyWithLookup(req, newparent_ino, newname);
}

absl::Status PageAlignFS::Open(
    FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "Open() ino:" << inode;

  ASSIGN_OR_RETURN(
      FileDescriptor fd,
      syscalls::open(
        absl::StrCat("/proc/self/fd/", inode.GetFD()).c_str(),
        (fi.flags | O_CLOEXEC) & ~O_NOFOLLOW));

  fi.noflush = (fi.flags & O_ACCMODE) == O_RDONLY;
  fi.parallel_direct_writes = 1;
  fi.fh = *fd;

  RETURN_IF_ERROR(req.ReplyOpen(fi));
  std::move(fd).Release();
  return absl::OkStatus();
}

absl::Status PageAlignFS::Release(
    FuseRequest &, fuse_ino_t ino, fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "Release() ino:" << inode;
  return syscalls::close(FileDescriptor(fi.fh));
}

absl::Status PageAlignFS::Read(
    FuseRequest &req, fuse_ino_t ino, size_t size, off_t off,
    fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "Read() ino:" << inode;
  fuse_bufvec bufv = FUSE_BUFVEC_INIT(size);
  bufv.buf[0].flags = static_cast<fuse_buf_flags>(
      FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
  bufv.buf[0].fd = fi.fh;
  bufv.buf[0].pos = off;
  return req.ReplyData(std::move(bufv));
}

absl::Status PageAlignFS::WriteBuf(
    FuseRequest &req, fuse_ino_t ino, fuse_bufvec &in_buf, off_t off,
    fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  size_t bufsiz = fuse_buf_size(&in_buf);
  LOG(INFO) << "WriteBuf() ino:" << inode << ", bufsiz:" << bufsiz;

  fuse_bufvec out_buf = FUSE_BUFVEC_INIT(bufsiz);
  out_buf.buf[0].flags = static_cast<fuse_buf_flags>(
      FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
  out_buf.buf[0].fd = fi.fh;
  out_buf.buf[0].pos = off;

  ASSIGN_OR_RETURN(size_t nb, FuseBufCopy(out_buf, in_buf));
  absl::Status ret = req.ReplyWrite(nb);
  LOG_IF_ERROR(WARNING, inode.NotifyPollEvent());
  return ret;
}

absl::Status PageAlignFS::Flush(
    FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "Flush() ino:" << inode;

  // Duplicate then close the fd to provide some attempt at providing close-time
  // errors.
  ASSIGN_OR_RETURN(FileDescriptor fd, syscalls::dup(fi.fh));
  return syscalls::close(std::move(fd));
}

absl::Status PageAlignFS::FSync(
    FuseRequest &req, fuse_ino_t ino, bool datasync, fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "FSync() ino:" << inode << ", datasync:" << datasync;

  if (datasync) {
    return syscalls::fdatasync(fi.fh);
  } else {
    return syscalls::fsync(fi.fh);
  }
}

absl::Status PageAlignFS::FSyncDir(
    FuseRequest &req, fuse_ino_t ino, bool datasync, fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "FSyncDir() ino:" << inode << ", datasync:" << datasync;

  auto &dir = *reinterpret_cast<Directory *>(fi.fh);
  ASSIGN_OR_RETURN(int dfd, syscalls::dirfd(*dir));

  if (datasync) {
    return syscalls::fdatasync(dfd);
  } else {
    return syscalls::fsync(dfd);
  }
}

absl::Status PageAlignFS::StatFS(FuseRequest &req, fuse_ino_t ino) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "StatFS() ino:" << inode;
  ASSIGN_OR_RETURN(struct statvfs stbuf, syscalls::fstatvfs(inode.GetFD()));
  return req.ReplyStatFS(std::move(stbuf));
}

absl::Status PageAlignFS::SetXAttr(
    FuseRequest &req, fuse_ino_t ino, std::string_view name,
    std::span<const char> value, int flags) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "SetXAttr() ino:" << inode << ", name:" << name;
  return syscalls::setxattr(
      absl::StrCat("/proc/self/fd/", inode.GetFD()),
      name, value, flags);
}

// TODO: Test that size=0 case works
absl::Status PageAlignFS::GetXAttr(
    FuseRequest &req, fuse_ino_t ino, std::string_view name, size_t size) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "GetXAttr() ino:" << inode << ", name:" << name;
  ASSIGN_OR_RETURN(
      std::vector<char> buf,
      syscalls::getxattr(
        absl::StrCat("/proc/self/fd/", inode.GetFD()), name, /*maxsize=*/size));
  CHECK_LE(buf.size(), size);
  return req.ReplyBuf(buf);
}

// TODO: Test that size=0 case works
absl::Status PageAlignFS::ListXAttr(
    FuseRequest &req, fuse_ino_t ino, size_t size) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "ListXAttr() ino:" << inode << ", size:" << size;
  std::vector<char> buf(size);
  ASSIGN_OR_RETURN(
      size_t nb,
      syscalls::listxattr(
        absl::StrCat("/proc/self/fd/", inode.GetFD()), buf));
  CHECK_LE(buf.size(), size);
  buf.resize(nb);
  return req.ReplyBuf(buf);
}

absl::Status PageAlignFS::RemoveXAttr(
    FuseRequest &req, fuse_ino_t ino, std::string_view name) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "RemoveXAttr() ino:" << inode << ", name:" << name;
  return syscalls::removexattr(
      absl::StrCat("/proc/self/fd/", inode.GetFD()), name);
}

absl::Status PageAlignFS::Access(FuseRequest &req, fuse_ino_t ino, int mask) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "Access() ino:" << inode;
  // faccessat doesn't support AT_EMPTY_PATH yet
  return syscalls::access(absl::StrCat("/proc/self/fd/", inode.GetFD()), mask);
}

absl::Status PageAlignFS::Create(
    FuseRequest &req, fuse_ino_t ino, std::string_view name, mode_t mode,
    fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "Create() ino:" << inode;

  ASSIGN_OR_RETURN(
      FileDescriptor fd,
      syscalls::openat(
        inode.GetFD(), std::string(name).c_str(),
        (fi.flags | O_CLOEXEC | O_CREAT) & ~O_NOFOLLOW, mode));

  fi.noflush = (fi.flags & O_ACCMODE) == O_RDONLY;
  fi.parallel_direct_writes = 1;
  fi.fh = *fd;

  RETURN_IF_ERROR(ReplyWithCreate(req, inode, name, fi));
  std::move(fd).Release();
  return absl::OkStatus();
}

absl::Status PageAlignFS::GetLk(
    FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi, flock &lock) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "GetLk() ino:" << inode;

  RETURN_IF_ERROR(syscalls::fcntl(inode.GetFD(), F_GETLK, &lock).status());
  return req.ReplyLock(lock);
}

absl::Status PageAlignFS::SetLk(
    FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi, flock &lock,
    bool sleep) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "SetLk() ino:" << inode;

  int cmd = sleep ? F_SETLKW : F_SETLK;
  // TODO: This will set the lock owner to our pid/uid, meaning a GetLk call
  // will return our pid instead of the original acquirer. Instead we should
  // track the caller (in fi.owner) and return that from GetLk. See setlk in
  // fuse_lowlevel.h
  return syscalls::fcntl(inode.GetFD(), cmd, &lock).status();
}

absl::Status PageAlignFS::FLock(
    FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi, int op) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "FLock() ino:" << inode;
  return syscalls::flock(inode.GetFD(), op);
}

absl::Status PageAlignFS::FAllocate(
    FuseRequest &req, fuse_ino_t ino, int mode, off_t offset, off_t length,
    fuse_file_info &) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "FAllocate() ino:" << inode;
  return syscalls::fallocate(inode.GetFD(), mode, offset, length);
}

absl::Status PageAlignFS::CopyFileRange(
    FuseRequest &req,
    fuse_ino_t ino_in, off_t off_in, fuse_file_info &fi_in,
    fuse_ino_t ino_out, off_t off_out, fuse_file_info &fi_out,
    size_t len, int flags) {
  Inode &inode_in = GetInode(ino_in);
  Inode &inode_out = GetInode(ino_out);
  LOG(INFO)
    << "CopyFileRange() ino_in:" << inode_in << ", ino_out:" << inode_out;
  ASSIGN_OR_RETURN(
      size_t nb,
      syscalls::copy_file_range(
        inode_in.GetFD(), &off_in, inode_out.GetFD(), &off_out, len, flags));
  return req.ReplyWrite(nb);
}

absl::Status PageAlignFS::LSeek(
    FuseRequest &req, fuse_ino_t ino, off_t off, int whence,
    fuse_file_info &fi) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "LSeek() ino:" << inode;
  ASSIGN_OR_RETURN(off_t next_off, syscalls::lseek(inode.GetFD(), off, whence));
  return req.ReplyLSeek(next_off);
}

absl::Status PageAlignFS::Poll(
    FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi, FusePollHandle ph) {
  Inode &inode = GetInode(ino);
  LOG(INFO) << "Poll() ino:" << inode;
  inode.AddPollHandle(std::move(ph));
  struct pollfd pfd {
    .fd = inode.GetFD(),
    .events =
      (POLLIN | POLLPRI | POLLOUT | POLLRDHUP | POLLERR | POLLHUP | POLLNVAL),
    .revents = 0,
  };
  ASSIGN_OR_RETURN(
      nfds_t ready_fds,
      syscalls::poll({&pfd, 1}, /*timeout=*/absl::ZeroDuration()));
  CHECK_LE(ready_fds, 1);
  return req.ReplyPoll(pfd.revents);
}


#if 0
absl::Status DoStuff(std::string mountpoint) {
  // prctl(PR_SET_IO_FLUSHER, 1, 0, 0, 0);
  //
  // struct rlimit rlimit;
  // rlimit.rlim_cur = RLIM_INFINITY;
  // rlimit.rlim_max = RLIM_INFINITY;
  // rc = setrlimit(RLIMIT_MEMLOCK, &rlimit);
  // mlock all

  LOG(INFO) << "Mounting to " << mountpoint;
  absl::StatusOr<FuseMount> mount = FuseMount::Create(
      std::move(mountpoint),
      {
          .fsname = "pafs",
          .mount_source = "mydisks",
          .mount_options = {"default_permissions"},
      });
  if (!mount.ok()) return std::move(mount).status();

  LOG(INFO) << "Success";

  return absl::OkStatus();
}
#endif

PageAlignFS::PageAlignFS(Inode root, Options opts)
  : root_(std::move(root)), opts_(std::move(opts))
{}

absl::Status PageAlignFS::ReadDirInternal(
    FuseRequest &req, const Inode &dir_inode, size_t size, off_t off,
    fuse_file_info &fi, bool plus) {
  // TODO change to use fuse_reply_data

  auto &dir = *reinterpret_cast<Directory *>(fi.fh);

  RETURN_IF_ERROR(syscalls::seekdir(*dir, off));

  std::vector<std::shared_ptr<Inode>> inodes;
  absl::Cleanup unref_inodes([this, &inodes]() {
    for (const std::shared_ptr<Inode> &inode : inodes) {
      LOG_IF_ERROR(WARNING, inodes_.Unref(*inode));
    }
  });

  FuseDirsBuilder dirs(&req, plus, /*maxsize=*/size);
  while (true) {
    ASSIGN_OR_RETURN(struct dirent *entry, syscalls::readdir(*dir));
    if (entry == nullptr) break;

    ASSIGN_OR_RETURN(
        std::shared_ptr<Inode> inode,
        FindOrCreateInode(dir_inode, entry->d_name));

    ASSIGN_OR_RETURN(
        fuse_entry_param param,
        CreateFuseEntryParam(inode.get(), /*with_generation=*/plus));
    if (!dirs.AddDirEntry(entry->d_name, std::move(param), entry->d_off)) {
      break;
    }

    if (plus) {
      // ReadDirPlus is supposed to increment the refcnt, whereas ReadDir does
      // not.
      RETURN_IF_ERROR(inodes_.Ref(*inode));
      inodes.push_back(inode);
    }
  }

  RETURN_IF_ERROR(std::move(dirs).Reply());
  std::move(unref_inodes).Cancel();
  return absl::OkStatus();
}

absl::StatusOr<PageAlignFS> PageAlignFS::Create(
    std::string_view srcdir, Options opts) {
  ASSIGN_OR_RETURN(auto root, Inode::Create(srcdir));
  ASSIGN_OR_RETURN(struct stat st, root.Stat());
  if (!S_ISDIR(st.st_mode)) {
    return absl::FailedPreconditionError("Mountpoint is not a directory");
  }
  return absl::StatusOr<PageAlignFS>(absl::in_place_t{}, std::move(root), opts);
}

Inode &PageAlignFS::GetInode(fuse_ino_t ino) {
  if (ino == FUSE_ROOT_ID) return root_;
  return *reinterpret_cast<Inode *>(ino);
}

absl::StatusOr<std::shared_ptr<Inode>>
PageAlignFS::FindOrCreateInode(const Inode &parent, std::string_view path) {
  ASSIGN_OR_RETURN(auto inode, Inode::Create(path, parent.GetFD()));
  return inodes_.Insert(std::move(inode));
}

absl::StatusOr<fuse_entry_param> PageAlignFS::CreateFuseEntryParam(
    const Inode *inode, bool with_generation) {
  fuse_entry_param param;
  ASSIGN_OR_RETURN(param.attr, inode->Stat());
  if (with_generation) {
    ASSIGN_OR_RETURN(param.generation, inode->GetGeneration());
  }
  param.attr_timeout = absl::ToDoubleSeconds(opts_.kernel_attribute_timeout);
  param.entry_timeout = absl::ToDoubleSeconds(opts_.kernel_entry_timeout);
  static_assert(sizeof(fuse_ino_t) >= sizeof(inode));
  param.ino = reinterpret_cast<fuse_ino_t>(inode);
  return param;
}

absl::Status PageAlignFS::ReplyWithCreate(
    FuseRequest &req, const Inode &parent, std::string_view name,
    const fuse_file_info &fi) {
  ASSIGN_OR_RETURN(
      std::shared_ptr<Inode> inode, FindOrCreateInode(parent, name));

  if (parent.GetSourceDevice() != inode->GetSourceDevice()) {
    // TODO can we do this after all?
    return absl::FailedPreconditionError(
        "PageAlignFS cannot span multiple source mounts");
  }

  ASSIGN_OR_RETURN(
      fuse_entry_param param,
      CreateFuseEntryParam(inode.get(), /*with_generation=*/true));

  // Increment the refcnt on behalf of the kernel.
  RETURN_IF_ERROR(inodes_.Ref(*inode));
  // ... but decrement it if we fail to reply
  absl::Cleanup unref_inode([this, &inode = *inode]() {
    LOG_IF_ERROR(WARNING, inodes_.Unref(inode));
  });

  RETURN_IF_ERROR(req.ReplyCreate(param, fi));
  // We replied successfully, don't decrement
  std::move(unref_inode).Cancel();
  return absl::OkStatus();
}

absl::Status PageAlignFS::ReplyWithLookup(
    FuseRequest &req, const Inode &parent, std::string_view name) {
  ASSIGN_OR_RETURN(
      std::shared_ptr<Inode> inode, FindOrCreateInode(parent, name));

  if (parent.GetSourceDevice() != inode->GetSourceDevice()) {
    // TODO can we do this after all?
    return absl::FailedPreconditionError(
        "PageAlignFS cannot span multiple source mounts");
  }

  ASSIGN_OR_RETURN(
      fuse_entry_param param,
      CreateFuseEntryParam(inode.get(), /*with_generation=*/true));

  // Increment the refcnt on behalf of the kernel.
  RETURN_IF_ERROR(inodes_.Ref(*inode));
  // ... but decrement it if we fail to reply
  absl::Cleanup unref_inode([this, &inode = *inode]() {
    LOG_IF_ERROR(WARNING, inodes_.Unref(inode));
  });

  RETURN_IF_ERROR(req.ReplyEntry(param));
  // We replied successfully, don't decrement
  std::move(unref_inode).Cancel();
  return absl::OkStatus();
}

absl::Status PageAlignFS::ReplyWithAttrs(FuseRequest &req, const Inode &inode) {
  ASSIGN_OR_RETURN(struct stat attrs, inode.Stat());
  return req.ReplyAttr(attrs, opts_.kernel_entry_timeout);
}

}  // namespace pafs
