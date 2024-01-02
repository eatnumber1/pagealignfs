#ifndef FUSE_USE_VERSION
// Needed by fuse/fuse_lowlevel.h
#define FUSE_USE_VERSION 312
#elif FUSE_USE_VERSION != 312
#error this file is written for fuse 3.12
#endif

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "absl/cleanup/cleanup.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "fuse/fuse_lowlevel.h"
#include "pafs/fuse.h"
#include "pafs/fuse_ops.h"
#include "pafs/syscalls.h"
#include "pafs/page_align_fs.h"

ABSL_FLAG(absl::Duration, kernel_attribute_timeout, absl::ZeroDuration(), "How long the kernel can cache inode attributes.");
ABSL_FLAG(absl::Duration, kernel_entry_timeout, absl::ZeroDuration(), "How long the kernel can cache fs entries (files and directories).");

namespace pafs {

absl::StatusOr<int> Main(int argc, char *argv[]) {
  absl::SetProgramUsageMessage("[flags] -- [fuse_flags [--]] mountpoint");
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  //argc = args.size();
  //args.push_back(nullptr);
  //argv = args.data();

  // TODO safe cast args.size()
  struct fuse_args fuse_args = FUSE_ARGS_INIT(static_cast<int>(args.size()), args.data());
  absl::Cleanup cleanup_fuse_args = [&fuse_args]() {
    fuse_opt_free_args(&fuse_args);
  };

  struct fuse_cmdline_opts fuse_opts;
  if (fuse_parse_cmdline(&fuse_args, &fuse_opts) != 0) return EXIT_FAILURE;
  absl::Cleanup cleanup_fuse_opts = [&fuse_opts]() {
    free(fuse_opts.mountpoint);
  };

  if (fuse_opts.show_help) {
    fuse_cmdline_help();
    fuse_lowlevel_help();
    return EXIT_SUCCESS;
  }

  if (fuse_opts.show_version) {
    std::cerr << "FUSE library version " << fuse_pkgversion() << std::endl;
    fuse_lowlevel_version();
    return EXIT_SUCCESS;
  }

  if (fuse_opts.mountpoint == nullptr) {
    std::cerr << "TODO usage here" << std::endl;
    return EXIT_FAILURE;
  }

  absl::StatusOr<PageAlignFS> pafs = PageAlignFS::Create(
      fuse_opts.mountpoint,
      {
        .kernel_entry_timeout = absl::GetFlag(FLAGS_kernel_entry_timeout),
        .kernel_attribute_timeout = absl::GetFlag(FLAGS_kernel_attribute_timeout),
      });
  RETURN_IF_ERROR(pafs.status());

  struct fuse_lowlevel_ops pafs_ops = AsFuseLowLevelOps<PageAlignFS>();
  struct fuse_session *fuse_session =
      fuse_session_new(&fuse_args, &pafs_ops, sizeof(pafs_ops), &(*pafs));
  if (fuse_session == nullptr) return EXIT_FAILURE;
  absl::Cleanup cleanup_fuse_session = [fuse_session]() {
    fuse_session_destroy(fuse_session);
  };

  if (fuse_set_signal_handlers(fuse_session) != 0) return EXIT_FAILURE;
  absl::Cleanup cleanup_fuse_signal_handlers = [fuse_session]() {
    fuse_remove_signal_handlers(fuse_session);
  };

  if (fuse_session_mount(fuse_session, fuse_opts.mountpoint) != 0) {
    return EXIT_FAILURE;
  }
  absl::Cleanup cleanup_fuse_mount = [fuse_session]() {
    fuse_session_unmount(fuse_session);
  };

  fuse_daemonize(fuse_opts.foreground);

  if (fuse_opts.singlethread) {
    return fuse_session_loop(fuse_session);
  }

  struct fuse_loop_config *loop_config = fuse_loop_cfg_create();
  CHECK_NE(loop_config, nullptr);
  absl::Cleanup cleanup_loop_config = [loop_config]() {
    fuse_loop_cfg_destroy(loop_config);
  };
  fuse_loop_cfg_set_clone_fd(loop_config, fuse_opts.clone_fd);
  fuse_loop_cfg_set_idle_threads(loop_config, fuse_opts.max_idle_threads);
  fuse_loop_cfg_set_max_threads(loop_config, fuse_opts.max_threads);
  return fuse_session_loop_mt(fuse_session, loop_config);

#if 0
  absl::Status st = pafs.DoStuff(fuse_opts.mountpoint);
  if (!st.ok()) {
    std::cerr << st << std::endl;
    return EXIT_FAILURE;
  }
#endif
}

}  // namespace pafs

int main(int argc, char *argv[]) {
  absl::StatusOr<int> ret = pafs::Main(argc, argv);
  if (!ret.ok()) {
    std::cerr << ret.status() << std::endl;
    return EXIT_FAILURE;
  }
  return *ret;
}
