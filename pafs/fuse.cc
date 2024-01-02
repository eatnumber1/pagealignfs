#include "pafs/fuse.h"

#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <utility>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>

#include "absl/log/die_if_null.h"
#include "absl/base/macros.h"
#include "pafs/syscalls.h"
#include "pafs/status.h"
#include "absl/status/status.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_cat.h"

namespace pafs {
namespace {

int StatusCodeToErrno(absl::StatusCode sc) {
  switch (sc) {
    case absl::StatusCode::kOk:
      return 0;
    case absl::StatusCode::kInvalidArgument:
      return EINVAL;
    case absl::StatusCode::kDeadlineExceeded:
      return ETIMEDOUT;
    case absl::StatusCode::kNotFound:
      return ENOENT;
    case absl::StatusCode::kAlreadyExists:
      return EEXIST;
    case absl::StatusCode::kPermissionDenied:
      ABSL_FALLTHROUGH_INTENDED;
    case absl::StatusCode::kUnauthenticated:
      return EPERM;
    case absl::StatusCode::kOutOfRange:
      return ERANGE;
    case absl::StatusCode::kFailedPrecondition:
      return EBUSY;
    case absl::StatusCode::kResourceExhausted:
      return ENOSPC;
    case absl::StatusCode::kCancelled:
      return ECANCELED;
    case absl::StatusCode::kAborted:
      return EDEADLK;
    case absl::StatusCode::kUnimplemented:
      return ENOSYS;
    case absl::StatusCode::kUnavailable:
      return EAGAIN;
    case absl::StatusCode::kDataLoss:
      return ENOTRECOVERABLE;
    case absl::StatusCode::kInternal:
      return ELIBBAD;
    case absl::StatusCode::kUnknown:
      ABSL_FALLTHROUGH_INTENDED;
    default:
      return EPROTO;
  }
}

}  // namespace

FuseDirsBuilder::FuseDirsBuilder(FuseRequest *req, bool plus, size_t maxsize)
  : req_(*ABSL_DIE_IF_NULL(req)), plus_(plus), maxsize_(maxsize) {}

bool FuseDirsBuilder::AddDirEntry(
    std::string_view name,
    fuse_entry_param param,
    off_t offset) {
  // TODO make a GarbageBin for this
  std::string name_str(name);

  const auto add_direntry =
    [this, name = name_str.c_str(), &param, offset](
        std::span<char> buf) -> size_t {
      if (plus_) {
        return fuse_add_direntry_plus(
            req_.Get(), buf.data(), buf.size(), name, &param, offset);
      }
      return fuse_add_direntry(
          req_.Get(), buf.data(), buf.size(), name, &param.attr, offset);
    };

  size_t next_entry_size = add_direntry({});
  size_t oldsize = buf_.size();
  if (next_entry_size + oldsize > maxsize_) return false;
  buf_.resize(buf_.size() + next_entry_size);

  size_t added = add_direntry({buf_.data() + oldsize, next_entry_size});
  CHECK_EQ(added, next_entry_size);

  return true;
}

absl::Status FuseDirsBuilder::Reply() && {
  CHECK_LE(buf_.size(), maxsize_);
  return req_.ReplyBuf(buf_);
}

FuseRequest::FuseRequest(fuse_req_t req) : req_(std::move(req)) {}

fuse_req_t &FuseRequest::operator*() { return *req_; }
fuse_req_t &FuseRequest::Get() { return *req_; }

FuseRequest::FuseRequest(FuseRequest &&o)
    : FuseRequest() {
  *this = std::move(o);
}

FuseRequest &FuseRequest::operator=(FuseRequest &&o) {
  using std::swap;
  swap(req_, o.req_);
  return *this;
}

FuseRequest::~FuseRequest() {
  if (!req_) return;
  // TODO imporve this warning
  LOG(WARNING) << "Replying to FuseRequest in destructor";
  absl::Status st = ReplyErrno(ECOMM);
  LOG_IF(ERROR, !st.ok()) << "Failed to send reply: " << st;
}

absl::Status FuseRequest::ReplyAttr(
    const struct stat &attr, absl::Duration attr_timeout) {
  if (!req_) return absl::OkStatus();
  absl::Status st = ErrnoToStatus(
      -fuse_reply_attr(*req_, &attr, absl::ToDoubleSeconds(attr_timeout)),
      "fuse_reply_attr");
  req_ = std::nullopt;
  return st;
}

absl::Status FuseRequest::ReplyOpen(const fuse_file_info &fi) {
  if (!req_) return absl::OkStatus();
  absl::Status st =
    ErrnoToStatus(-fuse_reply_open(*req_, &fi), "fuse_reply_open");
  req_ = std::nullopt;
  return st;
}

absl::Status FuseRequest::ReplyCreate(
    const fuse_entry_param &entry, const fuse_file_info &fi) {
  if (!req_) return absl::OkStatus();
  absl::Status st =
    ErrnoToStatus(-fuse_reply_create(*req_, &entry, &fi), "fuse_reply_create");
  req_ = std::nullopt;
  return st;
}

absl::Status FuseRequest::ReplyErrno(int errnum) {
  if (!req_) return absl::OkStatus();
  absl::Status st =
    ErrnoToStatus(-fuse_reply_err(*req_, errnum), "fuse_reply_err");
  req_ = std::nullopt;
  return st;
}

void FuseRequest::ReplyAlwaysAndLogIfNotOk(const absl::Status &status) {
  LOG_IF(ERROR, !status.ok()) << status;
  absl::Status reply_s;
  if (status.ok()) {
    reply_s = ReplyErrno(0);
  } else {
    reply_s = ReplyFailure(status);
  }
  LOG_IF(WARNING, !reply_s.ok()) << "Failed to reply to fuse: " << reply_s;
}

absl::Status FuseRequest::ReplyFailure(const absl::Status &status) {
  CHECK(!status.ok()) << status;
  if (!req_) return absl::OkStatus();
  absl::StatusOr<int> errno_ret = GetErrnoFromStatus(status);
  if (!errno_ret.ok()) errno_ret = StatusCodeToErrno(status.code());
  absl::Status st = ReplyErrno(*errno_ret);
  req_ = std::nullopt;
  return st;
}

void FuseRequest::ReplyFailureAndLogIfNotOk(const absl::Status &status) {
  if (status.ok()) return;
  LOG(ERROR) << status;
  absl::Status reply_s = ReplyFailure(status);
  LOG_IF(WARNING, !reply_s.ok()) << "Failed to reply with failure: " << reply_s;
}

absl::Status FuseRequest::ReplyBuf(std::span<char> buf) {
  if (!req_) return absl::OkStatus();
  absl::Status st =
    ErrnoToStatus(
        -fuse_reply_buf(*req_, buf.data(), buf.size()),
        "fuse_reply_buf");
  req_ = std::nullopt;
  return st;
}

absl::Status FuseRequest::ReplyEntry(const fuse_entry_param &param) {
  if (!req_) return absl::OkStatus();
  absl::Status st =
    ErrnoToStatus(
        -fuse_reply_entry(*req_, &param),
        "fuse_reply_entry");
  req_ = std::nullopt;
  return st;
}

absl::Status FuseRequest::ReplyReadLink(std::string_view link) {
  if (!req_) return absl::OkStatus();
  absl::Status st =
    ErrnoToStatus(
        -fuse_reply_readlink(*req_, std::string(link).c_str()),
        "fuse_reply_readlink");
  req_ = std::nullopt;
  return st;
}

void FuseRequest::ReplyNone() {
  if (!req_) return;
  fuse_reply_none(*req_);
  req_ = std::nullopt;
}

absl::Status FuseRequest::ReplyData(
    fuse_bufvec bufv, fuse_buf_copy_flags flags) {
  if (!req_) return absl::OkStatus();
  absl::Status st =
    ErrnoToStatus(
        -fuse_reply_data(*req_, &bufv, flags),
        "fuse_reply_data");
  req_ = std::nullopt;
  return st;
}

absl::Status FuseRequest::ReplyWrite(size_t bytes_written) {
  if (!req_) return absl::OkStatus();
  absl::Status st =
    ErrnoToStatus(
        -fuse_reply_write(*req_, bytes_written),
        "fuse_reply_write");
  req_ = std::nullopt;
  return st;
}

absl::Status FuseRequest::ReplyStatFS(const struct statvfs &stbuf) {
  if (!req_) return absl::OkStatus();
  absl::Status st =
    ErrnoToStatus(
        -fuse_reply_statfs(*req_, &stbuf),
        "fuse_reply_statfs");
  req_ = std::nullopt;
  return st;
}

absl::Status FuseRequest::ReplyLock(const struct flock &lock) {
  if (!req_) return absl::OkStatus();
  absl::Status st =
    ErrnoToStatus(
        -fuse_reply_lock(*req_, &lock),
        "fuse_reply_lock");
  req_ = std::nullopt;
  return st;
}

absl::Status FuseRequest::ReplyLSeek(off_t off) {
  if (!req_) return absl::OkStatus();
  absl::Status st =
    ErrnoToStatus(
        -fuse_reply_lseek(*req_, off),
        "fuse_reply_lseek");
  req_ = std::nullopt;
  return st;
}

absl::Status FuseRequest::ReplyPoll(unsigned revents) {
  if (!req_) return absl::OkStatus();
  absl::Status st =
    ErrnoToStatus(
        -fuse_reply_poll(*req_, revents),
        "fuse_reply_poll");
  req_ = std::nullopt;
  return st;
}

absl::StatusOr<size_t> FuseBufCopy(
    fuse_bufvec &dst, fuse_bufvec &src, fuse_buf_copy_flags flags) {
  ssize_t nb = fuse_buf_copy(&dst, &src, flags);
  if (nb < 0) return ErrnoToStatus(-nb, "fuse_buf_copy");
  return static_cast<size_t>(nb);
}

FusePollHandle::FusePollHandle(fuse_pollhandle *handle) : handle_(handle) {}

FusePollHandle::~FusePollHandle() {
  if (handle_ == nullptr) return;
  fuse_pollhandle_destroy(handle_);
  handle_ = nullptr;
}

FusePollHandle::FusePollHandle(FusePollHandle &&o)
  : FusePollHandle() {
  *this = std::move(o);
}

FusePollHandle &FusePollHandle::operator=(FusePollHandle &&o) {
  using std::swap;
  swap(handle_, o.handle_);
  return *this;
}

absl::Status FusePollHandle::Notify() const {
  return ErrnoToStatus(
        -fuse_lowlevel_notify_poll(handle_),
        "fuse_lowlevel_notify_poll");
}

FusePollHandle::operator bool() const {
  return handle_;
}

bool operator==(const FusePollHandle &ph, nullptr_t) {
  return !ph;
}

bool operator==(nullptr_t, const FusePollHandle &ph) {
  return ph == nullptr;
}

}  // namespace pafs
