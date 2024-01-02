#ifndef PAFS_PAGE_ALIGN_FS_H_
#define PAFS_PAGE_ALIGN_FS_H_

#include <cstdint>
#include <optional>
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <span>

#include "absl/functional/any_invocable.h"
#include "absl/container/flat_hash_map.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "fuse/fuse_lowlevel.h"
#include "pafs/fuse.h"
#include "pafs/fuse_ops.h"
#include "pafs/fd.h"
#include "pafs/inode.h"
#include "pafs/syscalls.h"

namespace pafs {

class PageAlignFS {
 public:
  struct Options {
    // Validity timeout for filenames.
    absl::Duration kernel_entry_timeout = absl::ZeroDuration();
    // Validity timeout for inode attributes.
    absl::Duration kernel_attribute_timeout = absl::ZeroDuration();
  };

  static absl::StatusOr<PageAlignFS> Create(
      std::string_view srcdir, Options opts);

  absl::Status Init(struct fuse_conn_info &conn);
  static_assert(FuseInitOp<PageAlignFS>);

  absl::Status Destroy();
  static_assert(FuseDestroyOp<PageAlignFS>);

  absl::Status Lookup(
      FuseRequest &req, fuse_ino_t parent, std::string_view name);
  static_assert(FuseLookupOp<PageAlignFS>);

  absl::Status Forget(FuseRequest &req, fuse_ino_t ino, uint64_t nlookup);
  static_assert(FuseForgetOp<PageAlignFS>);

  absl::Status GetAttr(FuseRequest &req, fuse_ino_t ino);
  static_assert(FuseGetAttrOp<PageAlignFS>);

  absl::Status SetAttr(
      FuseRequest &req, fuse_ino_t ino, struct stat &attr, int to_set,
      fuse_file_info &fi);
  static_assert(FuseSetAttrOp<PageAlignFS>);

  absl::Status ReadLink(FuseRequest &req, fuse_ino_t ino);
  static_assert(FuseReadLinkOp<PageAlignFS>);

  absl::Status OpenDir(FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi);
  static_assert(FuseOpenDirOp<PageAlignFS>);

  absl::Status ReleaseDir(FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi);
  static_assert(FuseReleaseDirOp<PageAlignFS>);

  absl::Status ReadDir(
      FuseRequest &req, fuse_ino_t ino, size_t size, off_t off,
      fuse_file_info &fi);
  static_assert(FuseReadDirOp<PageAlignFS>);

  absl::Status ReadDirPlus(
      FuseRequest &req, fuse_ino_t ino, size_t size, off_t off,
      fuse_file_info &fi);
  static_assert(FuseReadDirPlusOp<PageAlignFS>);

  absl::Status ForgetMulti(
      FuseRequest &req, std::span<fuse_forget_data> forgets);
  static_assert(FuseForgetMultiOp<PageAlignFS>);

  absl::Status Mknod(
      FuseRequest &req, fuse_ino_t ino, std::string_view name, mode_t mode,
      dev_t rdev);
  static_assert(FuseMknodOp<PageAlignFS>);

  absl::Status Mkdir(
      FuseRequest &req, fuse_ino_t ino, std::string_view name, mode_t mode);
  static_assert(FuseMkdirOp<PageAlignFS>);

  absl::Status Unlink(FuseRequest &req, fuse_ino_t ino, std::string_view name);
  static_assert(FuseUnlinkOp<PageAlignFS>);

  absl::Status Rmdir(FuseRequest &req, fuse_ino_t ino, std::string_view name);
  static_assert(FuseRmdirOp<PageAlignFS>);

  absl::Status Symlink(
      FuseRequest &req, std::string_view link, fuse_ino_t parent,
      std::string_view name);
  static_assert(FuseSymlinkOp<PageAlignFS>);

  absl::Status Rename(
      FuseRequest &req, fuse_ino_t parent, std::string_view name,
      fuse_ino_t newparent, std::string_view newname, unsigned int flags);
  static_assert(FuseRenameOp<PageAlignFS>);

  absl::Status Link(
      FuseRequest &req, fuse_ino_t ino, fuse_ino_t newparent,
      std::string_view newname);
  static_assert(FuseLinkOp<PageAlignFS>);

  absl::Status Open(
      FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi);
  static_assert(FuseOpenOp<PageAlignFS>);

  absl::Status Release(
      FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi);
  static_assert(FuseReleaseOp<PageAlignFS>);

  absl::Status Read(
      FuseRequest &req, fuse_ino_t ino, size_t size, off_t off,
      fuse_file_info &fi);
  static_assert(FuseReadOp<PageAlignFS>);

  absl::Status WriteBuf(
      FuseRequest &req, fuse_ino_t ino, fuse_bufvec &in_buf, off_t off,
      fuse_file_info &fi);
  static_assert(FuseWriteBufOp<PageAlignFS>);

  absl::Status Flush(
      FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi);
  static_assert(FuseFlushOp<PageAlignFS>);

  absl::Status FSync(
      FuseRequest &req, fuse_ino_t ino, bool datasync, fuse_file_info &fi);
  static_assert(FuseFSyncOp<PageAlignFS>);

  absl::Status FSyncDir(
      FuseRequest &req, fuse_ino_t ino, bool datasync, fuse_file_info &fi);
  static_assert(FuseFSyncDirOp<PageAlignFS>);

  absl::Status StatFS(FuseRequest &req, fuse_ino_t ino);
  static_assert(FuseStatFSOp<PageAlignFS>);

  absl::Status SetXAttr(
      FuseRequest &req, fuse_ino_t ino, std::string_view name,
      std::span<const char> value, int flags);
  static_assert(FuseSetXAttrOp<PageAlignFS>);

  absl::Status GetXAttr(
      FuseRequest &req, fuse_ino_t ino, std::string_view name, size_t size);
  static_assert(FuseGetXAttrOp<PageAlignFS>);

  absl::Status ListXAttr(
      FuseRequest &req, fuse_ino_t ino, size_t size);
  static_assert(FuseListXAttrOp<PageAlignFS>);

  absl::Status RemoveXAttr(
      FuseRequest &req, fuse_ino_t ino, std::string_view name);
  static_assert(FuseRemoveXAttrOp<PageAlignFS>);

  absl::Status Access(FuseRequest &req, fuse_ino_t ino, int mask);
  static_assert(FuseAccessOp<PageAlignFS>);

  absl::Status Create(
      FuseRequest &req, fuse_ino_t ino, std::string_view name, mode_t mode,
      fuse_file_info &fi);
  static_assert(FuseCreateOp<PageAlignFS>);

  absl::Status GetLk(
      FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi, flock &lock);
  static_assert(FuseGetLkOp<PageAlignFS>);

  absl::Status SetLk(
      FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi, flock &lock,
      bool sleep);
  static_assert(FuseSetLkOp<PageAlignFS>);

  absl::Status Poll(
      FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi, FusePollHandle ph);
  static_assert(FusePollOp<PageAlignFS>);

  absl::Status FLock(
      FuseRequest &req, fuse_ino_t ino, fuse_file_info &fi, int op);
  static_assert(FuseFLockOp<PageAlignFS>);

  absl::Status FAllocate(
      FuseRequest &req, fuse_ino_t ino, int mode, off_t offset, off_t length,
      fuse_file_info &fi);
  static_assert(FuseFAllocateOp<PageAlignFS>);

  absl::Status CopyFileRange(
      FuseRequest &req,
      fuse_ino_t ino_in, off_t off_in, fuse_file_info &fi_in,
      fuse_ino_t ino_out, off_t off_out, fuse_file_info &fi_out,
      size_t len, int flags);
  static_assert(FuseCopyFileRangeOp<PageAlignFS>);

  absl::Status LSeek(
      FuseRequest &req, fuse_ino_t ino, off_t off, int whence,
      fuse_file_info &fi);
  static_assert(FuseLSeekOp<PageAlignFS>);

#if 0
  absl::Status DoStuff(std::string mountpoint);
#endif

 private:
  // Gets an Inode from a fuse_ino_t.
  Inode &GetInode(fuse_ino_t ino);

  absl::StatusOr<std::shared_ptr<Inode>>
    FindOrCreateInode(const Inode &parent, std::string_view path);

  // Set `plus` to true if handling ReadDirPlus.
  absl::Status ReadDirInternal(
      FuseRequest &req, const Inode &dir_inode, size_t size, off_t off,
      fuse_file_info &fi, bool plus);

  // The passed-in inode pointer is retained by the fuse_entry_param.
  absl::StatusOr<fuse_entry_param> CreateFuseEntryParam(
      const Inode *inode, bool with_generation);

  // Any call to ReplyWithLookup increments the reference count of the Inode.
  absl::Status ReplyWithLookup(
      FuseRequest &req, const Inode &parent, std::string_view name);

  // Any call to ReplyWithCreate increments the reference count of the Inode.
  absl::Status ReplyWithCreate(
      FuseRequest &req, const Inode &parent, std::string_view name,
      const fuse_file_info &fi);

  absl::Status ReplyWithAttrs(FuseRequest &req, const Inode &inode);

 public:
  // TODO this should be private
  PageAlignFS(Inode root, Options opts);
 private:

  Inode root_;
  InodeCache inodes_;
  const Options opts_;
};

}  // namespace pafs

#endif  // PAFS_PAGE_ALIGN_FS_H_
