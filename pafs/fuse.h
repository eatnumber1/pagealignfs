#ifndef PAFS_FUSE_H_
#define PAFS_FUSE_H_

#ifndef FUSE_USE_VERSION
// Needed by fuse/fuse_lowlevel.h
#define FUSE_USE_VERSION 312
#elif FUSE_USE_VERSION != 312
#error this file is written for fuse 3.12
#endif

#include <concepts>
#include <span>
#include <string>
#include <cstdint>
#include <sys/stat.h>
#include <type_traits>
#include <utility>
#include <sys/statvfs.h>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "fuse/fuse_lowlevel.h"
#include "pafs/fd.h"
#include "pafs/mount.h"
#include "pafs/status.h"

namespace pafs {

absl::StatusOr<size_t> FuseBufCopy(
    fuse_bufvec &dst, fuse_bufvec &src,
    fuse_buf_copy_flags flags = static_cast<fuse_buf_copy_flags>(0));

// A FuseRequest is a wrapper around fuse_req_t that RAII owns replying to the
// request.
class FuseRequest {
 public:
  FuseRequest() = default;

  // Transfers responsibility to this FuseRequest for replying.
  FuseRequest(fuse_req_t req);
  ~FuseRequest();

  FuseRequest(FuseRequest &&);
  FuseRequest(const FuseRequest &) = delete;
  FuseRequest &operator=(FuseRequest &&);
  FuseRequest &operator=(const FuseRequest &) = delete;

  fuse_req_t &operator*();
  fuse_req_t &Get();

  absl::Status ReplyAttr(
    const struct stat &attr, absl::Duration attr_timeout);

  absl::Status ReplyOpen(const fuse_file_info &fi);

  absl::Status ReplyCreate(
      const fuse_entry_param &entry, const fuse_file_info &fi);

  absl::Status ReplyBuf(std::span<char> buf);

  absl::Status ReplyData(
      fuse_bufvec bufv,
      fuse_buf_copy_flags flags = static_cast<fuse_buf_copy_flags>(0));

  absl::Status ReplyEntry(const fuse_entry_param &param);

  absl::Status ReplyReadLink(std::string_view link);

  absl::Status ReplyWrite(size_t bytes_written);

  absl::Status ReplyStatFS(const struct statvfs &stbuf);

  absl::Status ReplyLock(const struct flock &lock);

  absl::Status ReplyLSeek(off_t off);

  absl::Status ReplyPoll(unsigned revents);

  // A "successful failure" response, e.g. ENOENT, where we succeeded in
  // performing an operation that correctly produces an error code.
  //
  // Also usable to reply success with ReplyErrno(0).
  //
  // Usually does not need to be called, is handled by callback wrappers.
  absl::Status ReplyErrno(int errnum);

  // A true failure response, e.g. db connection lost. We failed to do what the
  // user asked for.
  //
  // Must be called with a non-ok status.
  //
  // Usually does not need to be called, is handled by callback wrappers.
  absl::Status ReplyFailure(const absl::Status &status);

  // Does not need to be called (is called by e.g. GetFuseForgetOp).
  void ReplyNone();

  // Implementation detail -- helpers for the OpFns below
  void ReplyFailureAndLogIfNotOk(const absl::Status &status);
  void ReplyAlwaysAndLogIfNotOk(const absl::Status &status);
 private:

  std::optional<fuse_req_t> req_;
};

class FusePollHandle {
 public:
  FusePollHandle() = default;
  // Assumes ownership of the pollhandle.
  FusePollHandle(fuse_pollhandle *handle);

  ~FusePollHandle();

  FusePollHandle(FusePollHandle &&);
  FusePollHandle &operator=(FusePollHandle &&);
  FusePollHandle(const FusePollHandle &) = delete;
  FusePollHandle &operator=(const FusePollHandle &) = delete;

  operator bool() const;

  // Notify watchers of this handle that a pollable event has occurred.
  absl::Status Notify() const;

 private:
  fuse_pollhandle *handle_ = nullptr;
};

bool operator==(const FusePollHandle &ph, nullptr_t);
bool operator==(nullptr_t, const FusePollHandle &ph);

class FuseDirsBuilder {
 public:
  FuseDirsBuilder(FuseRequest *req, bool plus, size_t maxsize);

  // Adds a directory entry to this builder, unless maxsize is reached.
  //
  // Returns false if maxsize is reached, otherwise returns true.
  //
  // If called from a Readdir context, only param.attr is used.
  bool AddDirEntry(
   std::string_view name,
   fuse_entry_param param,
   off_t offset);

  absl::Status Reply() &&;

 private:
  FuseRequest &req_;
  bool plus_;
  size_t maxsize_;
  std::vector<char> buf_;
};

}  // namespace pafs

#endif  // PAFS_FUSE_H_
