#ifndef PAFS_FD_H_
#define PAFS_FD_H_

#include "absl/status/status.h"

namespace pafs {

class FileDescriptor {
 public:
  FileDescriptor();
  explicit FileDescriptor(int fd);
  ~FileDescriptor();
  int operator*() const;
  int Release() &&;

  // Moveable, but not copyable. Copying would require dup2
  FileDescriptor(FileDescriptor &&);
  FileDescriptor(const FileDescriptor &) = delete;
  FileDescriptor &operator=(FileDescriptor &&);
  FileDescriptor &operator=(const FileDescriptor &) = delete;

 private:
  int fd_ = -1;
};

}  // namespace pafs

#endif  // PAFS_FD_H_
