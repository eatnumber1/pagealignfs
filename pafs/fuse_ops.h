#ifndef PAFS_FUSE_OPS_H_
#define PAFS_FUSE_OPS_H_

#ifndef FUSE_USE_VERSION
// Needed by fuse/fuse_lowlevel.h
#define FUSE_USE_VERSION 312
#elif FUSE_USE_VERSION != 312
#error this file is written for fuse 3.12
#endif

#include <concepts>
#include <span>
#include <optional>
#include <string>
#include <cstdint>
#include <sys/stat.h>
#include <type_traits>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "fuse/fuse_lowlevel.h"
#include "pafs/fd.h"
#include "pafs/fuse_ops.h"
#include "pafs/mount.h"
#include "pafs/status.h"

namespace pafs {

// Create a fuse_lowlevel_ops that fills in callbacks for all concepts that T
// implements.
template <typename T>
const struct fuse_lowlevel_ops AsFuseLowLevelOps();

template <typename T>
concept FuseInitOp = requires(T t) {
  {
    t.Init(
        std::declval<std::add_lvalue_reference_t<fuse_conn_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseDestroyOp = requires(T t) {
  { t.Destroy() } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseLookupOp = requires(T t) {
  {
    t.Lookup(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::string_view>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseForgetOp = requires(T t) {
  {
    t.Forget(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        uint64_t{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseGetAttrOp = requires(T t) {
  {
    t.GetAttr(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseSetAttrOp = requires(T t) {
  {
    t.SetAttr(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::add_lvalue_reference_t<struct stat>>(),
        int{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseMknodOp = requires(T t) {
  {
    t.Mknod(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::string_view>(),
        mode_t{},
        dev_t{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseMkdirOp = requires(T t) {
  {
    t.Mkdir(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::string_view>(),
        mode_t{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseUnlinkOp = requires(T t) {
  {
    t.Unlink(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::string_view>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseRmdirOp = requires(T t) {
  {
    t.Rmdir(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::string_view>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseSymlinkOp = requires(T t) {
  {
    t.Symlink(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        std::declval<std::string_view>(),
        fuse_ino_t{},
        std::declval<std::string_view>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseRenameOp = requires(T t) {
  {
    t.Rename(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::string_view>(),
        fuse_ino_t{},
        std::declval<std::string_view>(),
        std::declval<unsigned int>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseLinkOp = requires(T t) {
  {
    t.Link(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        fuse_ino_t{},
        std::declval<std::string_view>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseOpenOp = requires(T t) {
  {
    t.Open(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseReadOp = requires(T t) {
  {
    t.Read(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        size_t{},
        off_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseWriteOp = requires(T t) {
  {
    t.Write(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::span<const char>>(),
        off_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseFlushOp = requires(T t) {
  {
    t.Flush(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseReleaseOp = requires(T t) {
  {
    t.Release(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseFSyncOp = requires(T t) {
  {
    t.FSync(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        bool{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseFSyncDirOp = requires(T t) {
  {
    t.FSyncDir(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        bool{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseReadLinkOp = requires(T t) {
  {
    t.ReadLink(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseOpenDirOp = requires(T t) {
  {
    t.OpenDir(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseReleaseDirOp = requires(T t) {
  {
    t.ReleaseDir(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseReadDirOp = requires(T t) {
  {
    t.ReadDir(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        size_t{},
        off_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseReadDirPlusOp = requires(T t) {
  {
    t.ReadDirPlus(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        size_t{},
        off_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseWriteBufOp = requires(T t) {
  {
    t.WriteBuf(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::add_lvalue_reference_t<struct fuse_bufvec>>(),
        off_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseForgetMultiOp = requires(T t) {
  {
    t.ForgetMulti(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        std::span<fuse_forget_data>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseStatFSOp = requires(T t) {
  {
    t.StatFS(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseSetXAttrOp = requires(T t) {
  {
    t.SetXAttr(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::string_view>(),
        std::span<const char>(),
        int{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseGetXAttrOp = requires(T t) {
  {
    t.GetXAttr(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::string_view>(),
        size_t{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseListXAttrOp = requires(T t) {
  {
    t.ListXAttr(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        size_t{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseRemoveXAttrOp = requires(T t) {
  {
    t.RemoveXAttr(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::string_view>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseAccessOp = requires(T t) {
  {
    t.Access(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        int{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseCreateOp = requires(T t) {
  {
    t.Create(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::string_view>(),
        mode_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseGetLkOp = requires(T t) {
  {
    t.GetLk(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>(),
        std::declval<std::add_lvalue_reference_t<flock>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseSetLkOp = requires(T t) {
  {
    t.SetLk(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>(),
        std::declval<std::add_lvalue_reference_t<flock>>(),
        bool{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseBMapOp = requires(T t) {
  {
    t.BMap(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        size_t{},
        uint64_t{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseIOCtlOp = requires(T t) {
  {
    t.IOCtl(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<unsigned int>(),
        std::declval<void *>(),
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>(),
        unsigned{},
        std::declval<std::span<const char>>(),
        size_t{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FusePollOp = requires(T t) {
  {
    t.Poll(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>(),
        std::declval<FusePollHandle>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseRetrieveReplyOp = requires(T t) {
  {
    t.RetrieveReply(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        std::declval<void *>(),
        fuse_ino_t{},
        off_t{},
        std::declval<std::add_lvalue_reference_t<fuse_bufvec>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseFLockOp = requires(T t) {
  {
    t.FLock(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>(),
        int{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseFAllocateOp = requires(T t) {
  {
    t.FAllocate(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        int{},
        off_t{},
        off_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseCopyFileRangeOp = requires(T t) {
  {
    t.CopyFileRange(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        off_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>(),
        fuse_ino_t{},
        off_t{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>(),
        size_t{},
        int{})
  } -> std::same_as<absl::Status>;
};
template <typename T>
concept FuseLSeekOp = requires(T t) {
  {
    t.LSeek(
        std::declval<std::add_lvalue_reference_t<FuseRequest>>(),
        fuse_ino_t{},
        off_t{},
        int{},
        std::declval<std::add_lvalue_reference_t<fuse_file_info>>())
  } -> std::same_as<absl::Status>;
};

// implementation details below

template <FuseInitOp T>
auto GetFuseInitOp() {
  return [](void *userdata, struct fuse_conn_info *conn) {
    CHECK_NE(userdata, nullptr);
    CHECK_NE(conn, nullptr);
    absl::Status s = static_cast<T*>(userdata)->Init(*conn);
    LOG_IF(ERROR, !s.ok()) << s;
  };
}
template <typename Others>
auto GetFuseInitOp() { return nullptr; }

template <FuseDestroyOp T>
auto GetFuseDestroyOp() {
  return [](void *userdata) {
    CHECK_NE(userdata, nullptr);
    absl::Status s = static_cast<T*>(userdata)->Destroy();
    LOG_IF(ERROR, !s.ok()) << s;
  };
}
template <typename>
auto GetFuseDestroyOp() { return nullptr; }

template <FuseLookupOp T>
auto GetFuseLookupOp() {
  return [](fuse_req_t req, fuse_ino_t parent, const char *name) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->Lookup(fr, parent, name));
  };
}
template <typename>
auto GetFuseLookupOp() { return nullptr; }

template <FuseForgetOp T>
auto GetFuseForgetOp() {
  return [](fuse_req_t req, fuse_ino_t ino, uint64_t nlookup) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    LOG_IF_ERROR(ERROR, t->Forget(fr, ino, nlookup));
    fr.ReplyNone();
  };
}
template <typename>
auto GetFuseForgetOp() { return nullptr; }

template <FuseGetAttrOp T>
auto GetFuseGetAttrOp() {
  return [](fuse_req_t req, fuse_ino_t ino, fuse_file_info *) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->GetAttr(fr, ino));
  };
}
template <typename>
auto GetFuseGetAttrOp() { return nullptr; }

template <FuseSetAttrOp T>
auto GetFuseSetAttrOp() {
  return [](fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set,
            struct fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->SetAttr(fr, ino, *attr, to_set, *fi));
  };
}
template <typename>
auto GetFuseSetAttrOp() { return nullptr; }

template <FuseReadLinkOp T>
auto GetFuseReadLinkOp() {
  return [](fuse_req_t req, fuse_ino_t ino) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->ReadLink(fr, ino));
  };
}
template <typename>
auto GetFuseReadLinkOp() { return nullptr; }

template <FuseMknodOp T>
auto GetFuseMknodOp() {
  return [](fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode,
            dev_t rdev) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->Mknod(fr, parent, name, mode, rdev));
  };
}
template <typename>
auto GetFuseMknodOp() { return nullptr; }

template <FuseMkdirOp T>
auto GetFuseMkdirOp() {
  return [](fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->Mkdir(fr, parent, name, mode));
  };
}
template <typename>
auto GetFuseMkdirOp() { return nullptr; }

template <FuseUnlinkOp T>
auto GetFuseUnlinkOp() {
  return [](fuse_req_t req, fuse_ino_t parent, const char *name) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->Unlink(fr, parent, name));
  };
}
template <typename>
auto GetFuseUnlinkOp() { return nullptr; }

template <FuseRmdirOp T>
auto GetFuseRmdirOp() {
  return [](fuse_req_t req, fuse_ino_t parent, const char *name) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->Rmdir(fr, parent, name));
  };
}
template <typename>
auto GetFuseRmdirOp() { return nullptr; }

template <FuseSymlinkOp T>
auto GetFuseSymlinkOp() {
  return [](fuse_req_t req, const char *link, fuse_ino_t parent,
            const char *name) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->Symlink(fr, link, parent, name));
  };
}
template <typename>
auto GetFuseSymlinkOp() { return nullptr; }

template <FuseRenameOp T>
auto GetFuseRenameOp() {
  return [](fuse_req_t req, fuse_ino_t parent, const char *name,
            fuse_ino_t newparent, const char *newname, unsigned int flags) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(
        t->Rename(fr, parent, name, newparent, newname, flags));
  };
}
template <typename>
auto GetFuseRenameOp() { return nullptr; }

template <FuseLinkOp T>
auto GetFuseLinkOp() {
  return [](fuse_req_t req, fuse_ino_t ino, fuse_ino_t parent,
            const char *newname) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->Link(fr, ino, parent, newname));
  };
}
template <typename>
auto GetFuseLinkOp() { return nullptr; }

template <FuseOpenOp T>
auto GetFuseOpenOp() {
  return [](fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->Open(fr, ino, *fi));
  };
}
template <typename>
auto GetFuseOpenOp() { return nullptr; }

template <FuseReadOp T>
auto GetFuseReadOp() {
  return [](fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
            struct fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->Read(fr, ino, size, off, *fi));
  };
}
template <typename>
auto GetFuseReadOp() { return nullptr; }

template <FuseWriteOp T>
auto GetFuseWriteOp() {
  return [](fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size,
            off_t off, struct fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->Write(fr, ino, {buf, size}, off, *fi));
  };
}
template <typename>
auto GetFuseWriteOp() { return nullptr; }

template <FuseFlushOp T>
auto GetFuseFlushOp() {
  return [](fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->Flush(fr, ino, *fi));
  };
}
template <typename>
auto GetFuseFlushOp() { return nullptr; }

template <FuseReleaseOp T>
auto GetFuseReleaseOp() {
  return [](fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->Release(fr, ino, *fi));
  };
}
template <typename>
auto GetFuseReleaseOp() { return nullptr; }

template <FuseFSyncOp T>
auto GetFuseFSyncOp() {
  return [](fuse_req_t req, fuse_ino_t ino, int datasync,
            struct fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->FSync(fr, ino, datasync, *fi));
  };
}
template <typename>
auto GetFuseFSyncOp() { return nullptr; }

template <FuseOpenDirOp T>
auto GetFuseOpenDirOp() {
  return [](fuse_req_t req, fuse_ino_t ino, fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->OpenDir(fr, ino, *fi));
  };
}
template <typename>
auto GetFuseOpenDirOp() { return nullptr; }

template <FuseReadDirOp T>
auto GetFuseReadDirOp() {
  return [](fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
            fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->ReadDir(fr, ino, size, off, *fi));
  };
}
template <typename>
auto GetFuseReadDirOp() { return nullptr; }

template <FuseReleaseDirOp T>
auto GetFuseReleaseDirOp() {
  return [](fuse_req_t req, fuse_ino_t ino, fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->ReleaseDir(fr, ino, *fi));
  };
}
template <typename>
auto GetFuseReleaseDirOp() { return nullptr; }

template <FuseFSyncDirOp T>
auto GetFuseFSyncDirOp() {
  return [](fuse_req_t req, fuse_ino_t ino, int datasync, fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->FSyncDir(fr, ino, datasync, *fi));
  };
}
template <typename>
auto GetFuseFSyncDirOp() { return nullptr; }

template <FuseStatFSOp T>
auto GetFuseStatFSOp() {
  return [](fuse_req_t req, fuse_ino_t ino) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->StatFS(fr, ino));
  };
}
template <typename>
auto GetFuseStatFSOp() { return nullptr; }

template <FuseSetXAttrOp T>
auto GetFuseSetXAttrOp() {
  return [](fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->SetXAttr(fr, ino, name, {value, size}, flags));
  };
}
template <typename>
auto GetFuseSetXAttrOp() { return nullptr; }

template <FuseGetXAttrOp T>
auto GetFuseGetXAttrOp() {
  return [](fuse_req_t req, fuse_ino_t ino, const char *name, size_t size) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->GetXAttr(fr, ino, name, size));
  };
}
template <typename>
auto GetFuseGetXAttrOp() { return nullptr; }

template <FuseListXAttrOp T>
auto GetFuseListXAttrOp() {
  return [](fuse_req_t req, fuse_ino_t ino, size_t size) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->ListXAttr(fr, ino, size));
  };
}
template <typename>
auto GetFuseListXAttrOp() { return nullptr; }

template <FuseRemoveXAttrOp T>
auto GetFuseRemoveXAttrOp() {
  return [](fuse_req_t req, fuse_ino_t ino, const char *name) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->RemoveXAttr(fr, ino, name));
  };
}
template <typename>
auto GetFuseRemoveXAttrOp() { return nullptr; }

template <FuseAccessOp T>
auto GetFuseAccessOp() {
  return [](fuse_req_t req, fuse_ino_t ino, int mask) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->Access(fr, ino, mask));
  };
}
template <typename>
auto GetFuseAccessOp() { return nullptr; }

template <FuseCreateOp T>
auto GetFuseCreateOp() {
  return [](fuse_req_t req, fuse_ino_t ino, const char *name, mode_t mode,
            fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->Create(fr, ino, name, mode, *fi));
  };
}
template <typename>
auto GetFuseCreateOp() { return nullptr; }

template <FuseGetLkOp T>
auto GetFuseGetLkOp() {
  return [](fuse_req_t req, fuse_ino_t ino, fuse_file_info *fi, flock *lock) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->GetLk(fr, ino, *fi, *lock));
  };
}
template <typename>
auto GetFuseGetLkOp() { return nullptr; }

template <FuseSetLkOp T>
auto GetFuseSetLkOp() {
  return [](fuse_req_t req, fuse_ino_t ino, fuse_file_info *fi, flock *lock,
            int sleep) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->SetLk(fr, ino, *fi, *lock, sleep));
  };
}
template <typename>
auto GetFuseSetLkOp() { return nullptr; }

template <FuseBMapOp T>
auto GetFuseBMapOp() {
  return [](fuse_req_t req, fuse_ino_t ino, size_t blocksize, uint64_t idx) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->BMap(fr, ino, blocksize, idx));
  };
}
template <typename>
auto GetFuseBMapOp() { return nullptr; }

template <FuseIOCtlOp T>
auto GetFuseIOCtlOp() {
  return [](
      fuse_req_t req, fuse_ino_t ino, unsigned int cmd, void *arg,
      fuse_file_info *fi, unsigned flags, const void *in_buf, size_t in_bufsz,
      size_t out_bufsz) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(
        t->IOCtl(
          fr, ino, cmd, arg, *fi, flags,
          {static_cast<const char *>(in_buf), in_bufsz}, out_bufsz));
  };
}
template <typename>
auto GetFuseIOCtlOp() { return nullptr; }

template <FusePollOp T>
auto GetFusePollOp() {
  return [](
      fuse_req_t req, fuse_ino_t ino, fuse_file_info *fi, fuse_pollhandle *ph) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(
        t->Poll(fr, ino, *fi, FusePollHandle(ph)));
  };
}
template <typename>
auto GetFusePollOp() { return nullptr; }

template <FuseWriteBufOp T>
auto GetFuseWriteBufOp() {
  return [](fuse_req_t req, fuse_ino_t ino, fuse_bufvec *in_buf, off_t off, fuse_file_info *fi) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    LOG_IF_ERROR(ERROR, t->WriteBuf(fr, ino, *in_buf, off, *fi));
    fr.ReplyNone();
  };
}
template <typename>
auto GetFuseWriteBufOp() { return nullptr; }

template <FuseRetrieveReplyOp T>
auto GetFuseRetrieveReplyOp() {
  return [](
      fuse_req_t req, void *cookie, fuse_ino_t ino, off_t offset, fuse_bufvec *bufv) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    LOG_IF_ERROR(ERROR, t->RetrieveReply(fr, cookie, ino, offset, *bufv));
    fr.ReplyNone();
  };
}
template <typename>
auto GetFuseRetrieveReplyOp() { return nullptr; }

template <FuseForgetMultiOp T>
auto GetFuseForgetMultiOp() {
  return [](fuse_req_t req, size_t count, fuse_forget_data *forgets) {
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    LOG_IF_ERROR(ERROR, t->ForgetMulti(fr, {forgets, count}));
    fr.ReplyNone();
  };
}
template <typename>
auto GetFuseForgetMultiOp() { return nullptr; }

template <FuseFLockOp T>
auto GetFuseFLockOp() {
  return [](fuse_req_t req, fuse_ino_t ino, fuse_file_info *fi, int op) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->FLock(fr, ino, *fi, op));
  };
}
template <typename>
auto GetFuseFLockOp() { return nullptr; }

template <FuseFAllocateOp T>
auto GetFuseFAllocateOp() {
  return [](fuse_req_t req, fuse_ino_t ino, int mode, off_t offset,
            off_t length, fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(
        t->FAllocate(fr, ino, mode, offset, length, *fi));
  };
}
template <typename>
auto GetFuseFAllocateOp() { return nullptr; }

template <FuseReadDirPlusOp T>
auto GetFuseReadDirPlusOp() {
  return [](fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
            fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyAlwaysAndLogIfNotOk(t->ReadDirPlus(fr, ino, size, off, *fi));
  };
}
template <typename>
auto GetFuseReadDirPlusOp() { return nullptr; }

template <FuseCopyFileRangeOp T>
auto GetFuseCopyFileRangeOp() {
  return [](fuse_req_t req,
            fuse_ino_t ino_in, off_t off_in, fuse_file_info *fi_in,
            fuse_ino_t ino_out, off_t off_out, fuse_file_info *fi_out,
            size_t len, int flags) {
    CHECK_NE(fi_in, nullptr);
    CHECK_NE(fi_out, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(
        t->CopyFileRange(
          fr, ino_in, off_in, *fi_in, ino_out, off_out, *fi_out, len, flags));
  };
}
template <typename>
auto GetFuseCopyFileRangeOp() { return nullptr; }

template <FuseLSeekOp T>
auto GetFuseLSeekOp() {
  return [](fuse_req_t req, fuse_ino_t ino, off_t off, int whence,
            fuse_file_info *fi) {
    CHECK_NE(fi, nullptr);
    auto *t = static_cast<T*>(fuse_req_userdata(req));
    CHECK_NE(t, nullptr);
    FuseRequest fr(req);
    fr.ReplyFailureAndLogIfNotOk(t->LSeek(fr, ino, off, whence, *fi));
  };
}
template <typename>
auto GetFuseLSeekOp() { return nullptr; }

template <typename T>
const struct fuse_lowlevel_ops AsFuseLowLevelOps() {
  struct fuse_lowlevel_ops ops = {
    .init = GetFuseInitOp<T>(),
    .destroy = GetFuseDestroyOp<T>(),
    .lookup = GetFuseLookupOp<T>(),
    .forget = GetFuseForgetOp<T>(),
    .getattr = GetFuseGetAttrOp<T>(),
    .setattr = GetFuseSetAttrOp<T>(),
    .readlink = GetFuseReadLinkOp<T>(),
    .mknod = GetFuseMknodOp<T>(),
    .mkdir = GetFuseMkdirOp<T>(),
    .unlink = GetFuseUnlinkOp<T>(),
    .rmdir = GetFuseRmdirOp<T>(),
    .symlink = GetFuseSymlinkOp<T>(),
    .rename = GetFuseRenameOp<T>(),
    .link = GetFuseLinkOp<T>(),
    .open = GetFuseOpenOp<T>(),
    .read = GetFuseReadOp<T>(),
    .write = GetFuseWriteOp<T>(),
    .flush = GetFuseFlushOp<T>(),
    .release = GetFuseReleaseOp<T>(),
    .fsync = GetFuseFSyncOp<T>(),
    .opendir = GetFuseOpenDirOp<T>(),
    .readdir = GetFuseReadDirOp<T>(),
    .releasedir = GetFuseReleaseDirOp<T>(),
    .fsyncdir = GetFuseFSyncDirOp<T>(),
    .statfs = GetFuseStatFSOp<T>(),
    .setxattr = GetFuseSetXAttrOp<T>(),
    .getxattr = GetFuseGetXAttrOp<T>(),
    .listxattr = GetFuseListXAttrOp<T>(),
    .removexattr = GetFuseRemoveXAttrOp<T>(),
    .access = GetFuseAccessOp<T>(),
    .create = GetFuseCreateOp<T>(),
    .getlk = GetFuseGetLkOp<T>(),
    .setlk = GetFuseSetLkOp<T>(),
    .bmap = GetFuseBMapOp<T>(),
    .ioctl = GetFuseIOCtlOp<T>(),
    .poll = GetFusePollOp<T>(),
    .write_buf = GetFuseWriteBufOp<T>(),
    .retrieve_reply = GetFuseRetrieveReplyOp<T>(),
    .forget_multi = GetFuseForgetMultiOp<T>(),
    .flock = GetFuseFLockOp<T>(),
    .fallocate = GetFuseFAllocateOp<T>(),
    .readdirplus = GetFuseReadDirPlusOp<T>(),
    .copy_file_range = GetFuseCopyFileRangeOp<T>(),
    .lseek = GetFuseLSeekOp<T>(),
  };
  return ops;
}

}  // namespace pafs

#endif  // PAFS_FUSE_H_
