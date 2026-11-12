# File Attributes

Code using `gb::FileSystem` can read and change the attributes of files and
folders, starting with read-only and hidden. `GetPathInfo()` reports a path's
attributes, `SetAttributes()` changes some or all of them, and each protocol
says which ones it supports, which may differ by platform for local files.
What an attribute does beyond its basic meaning is up to the protocol,
following the file system behind it, so local files act as the operating
system's do. Copies keep their attributes.

OZ-3's disk images have read-only and hidden files, and its `oz3dsk attrib`
command reads and sets them; with this, its `DiskFileProtocol` can expose them
through `gb::FileSystem` like any other protocol's.

## Behavior

Nothing breaks: every addition has a default, and a protocol that doesn't
override anything supports no attributes.

- `FileAttribute` is an enum of every attribute any protocol may support,
  `FileAttributes` is `Flags<FileAttribute>`, and `kAllFileAttributes` holds
  every one:
  - `kReadOnly`: a read-only file can't be opened for writing, so it also
    can't be replaced by a copy. Whether it can be deleted, and what a
    read-only folder prevents, depends on the protocol. Moving it and changing
    its attributes are allowed.
  - `kHidden`: the file or folder is normally left out of what is shown to
    people. It changes nothing a protocol does: `List()` still returns it, and
    it can be opened, copied, moved, and deleted as any other.
- A protocol that supports an attribute supports it on both files and folders,
  even where it does nothing on one of them. A root folder never has
  attributes, so setting any on one fails.
- `PathInfo` gains `FileAttributes attributes`, holding only attributes the
  protocol supports.
- `FileProtocol::GetAttributeFlags()` returns the attributes a protocol
  supports (none by default). They are only available if the protocol supports
  `FileProtocolFlag::kInfo`; without it, `FileSystem` treats the protocol as
  supporting none.
- `FileSystem::GetAttributeFlags(protocol_name)` returns what a registered
  protocol supports, as `GetFlags()` does for its flags.
- `FileSystem::SetAttributes(path, attributes, mask = kAllFileAttributes)`
  changes the attributes in `mask` to match `attributes`, and leaves the rest:
  - `SetAttributes(path, FileAttribute::kHidden, FileAttribute::kHidden)`
    hides a path, `SetAttributes(path, {}, FileAttribute::kHidden)` shows it,
    and `SetAttributes(path, attributes)` replaces every attribute.
  - Attributes in `mask` that the protocol doesn't support are ignored, so the
    default mask works with every protocol.
  - It fails if nothing is at the path, if the protocol doesn't support both
    `kInfo` and `kFileWrite` (a protocol registered read-only can report
    attributes but not change them), or if an attribute in both `attributes`
    and `mask` isn't supported.
  - The change is made under the protocol's lock, as one operation.
- Copies keep attributes:
  - A copied file has the source's attributes. A folder that a folder copy
    creates has the source folder's, set once everything in it is copied, so
    a read-only folder doesn't stop its own copy; a folder that already
    existed keeps its own.
  - Between protocols, a copy keeps the attributes both protocols support.
    Attributes only the destination supports are left as the destination had
    them (none, for a new file).
  - Copying onto a read-only file fails, since it can't be opened for writing.
- Moves keep attributes, as the file or folder is the same one.

### MemoryFileProtocol

Supports `kReadOnly` and `kHidden`. Its constructor gains an optional
`ReadOnlyMode`, which says what a read-only file prevents:
- `kNoWrite` (the default): only writing. It can still be deleted, as for
  `LocalFileProtocol`.
- `kNoWriteOrDelete`: writing and deleting, as on OZ-3 disks and in Windows'
  own tools. A recursive folder delete fails at the first read-only file in it,
  having deleted what came before, as for any other partial delete.

A read-only folder prevents nothing, as on Windows.

Attributes can be changed while a file is open, and an open file keeps the
access it was opened with.

### LocalFileProtocol

Supports `kReadOnly` everywhere, and `kHidden` on Windows. Elsewhere, hidden
is a name starting with `.`, which is a naming convention rather than an
attribute that can be set, so it is not supported.

- Read-only is the operating system's: Windows' read-only attribute, or no
  write permission elsewhere, for files and folders alike.
- As `std::filesystem` does on every platform, including Windows (whose own
  tools refuse), a read-only file can be deleted, and a recursive folder
  delete removes read-only files in it.
- On Windows, a read-only folder prevents nothing (Windows uses the attribute
  to mark folders Explorer customizes). Elsewhere, nothing can be added to,
  removed from, or renamed in a read-only folder, so a recursive delete fails
  inside one, including the cleanup of a protocol with `kKeyDeleteAtExit`.
- Links are followed, as for everything else: a link reports and sets the
  attributes of what it links to.
- On Windows, a hidden file can be written over (opened with
  `FileFlag::kReset`, or copied onto) like any other, although Windows' file
  creation refuses to replace one.

## Design

Everything is in `gb_file`.

### Names

- `FileAttribute`, `FileAttributes`, `kAllFileAttributes`: no existing
  "attribute" names in `gb_file`. Distinct from `FileFlag` (how a file is
  opened) and `FileProtocolFlag` (what a protocol can do).
- `GetAttributeFlags()`: it pairs with `GetFlags()`, which returns a protocol's
  `FileProtocolFlags`.
- `SetAttributes()`, with a `mask`: one call both replaces every attribute
  (the default mask) and changes some. `ChangeAttributes(path, add, remove)`
  reads better for one change, but can't replace them all.
- `MemoryFileProtocol::ReadOnlyMode`, with `kNoWrite` and `kNoWriteOrDelete`,
  matching `FolderMode`.

### file_types.h

```
// Attributes of a file or folder, which a protocol may support (see
// FileProtocol::GetAttributeFlags()). A protocol that supports one supports
// it on both files and folders. What each one does, beyond what is said here,
// depends on the protocol, which follows the file system behind it.
enum class FileAttribute {
  // A read-only file can't be opened for writing, so it also can't be replaced
  // by a copy. Whether it can be deleted, and what a read-only folder
  // prevents, depends on the protocol.
  kReadOnly,

  // The file or folder is normally left out of what is shown to people. This
  // changes nothing a protocol does: List() still returns it, and it can be
  // opened, copied, moved, and deleted as any other.
  kHidden,
};
using FileAttributes = Flags<FileAttribute>;
inline constexpr FileAttributes kAllFileAttributes = {
    FileAttribute::kReadOnly,
    FileAttribute::kHidden,
};

struct PathInfo {
  PathInfo() = default;
  PathInfo(PathType type, int64_t size = 0, FileAttributes attributes = {});

  PathType type = PathType::kInvalid;  // Type of path.
  int64_t size = 0;                    // Set only for PathType::kFile.
  FileAttributes attributes;           // Only attributes the protocol
                                       // supports.
};
```

### FileProtocol

```
  // Returns the attributes this protocol supports, on both files and
  // folders, which GetPathInfo() reports and SetAttributes() sets. They are
  // only used if FileProtocolFlag::kInfo is supported.
  //
  // If not overridden, the protocol supports no attributes.
  virtual FileAttributes GetAttributeFlags() const;

  // Changes the attributes in `mask` of the file or folder at the specified
  // path to match `attributes`, leaving the others.
  //
  // Derived classes must override DoSetAttributes or BasicSetAttributes to
  // implement this if GetAttributeFlags() isn't empty. GetPathInfo is called
  // if DoSetAttributes is not implemented.
  bool SetAttributes(std::string_view protocol_name, std::string_view path,
                     FileAttributes attributes, FileAttributes mask);

 protected:
  // SetAttributes should fail if nothing is at `path`.
  //
  // SetAttributes is only called if the protocol supports both
  // FileProtocolFlag::kInfo and FileProtocolFlag::kFileWrite, `mask` holds
  // only attributes in GetAttributeFlags(), and `attributes` only attributes
  // in `mask`. The protocol is responsible for all further validation.
  virtual bool DoSetAttributes(std::string_view protocol_name,
                               std::string_view path,
                               FileAttributes attributes, FileAttributes mask);

  // BasicSetAttributes is only called on an existing path, with every
  // attribute it should have, when they differ from what it has.
  virtual bool BasicSetAttributes(std::string_view protocol_name,
                                  std::string_view path,
                                  FileAttributes attributes);
```

- `FileSystem` removes unsupported attributes from `mask`, and attributes
  outside `mask` from `attributes`, so protocols never see either.
- `SetAttributes` validates the path like the other public functions, and
  takes `LockType::kModify` (`MemoryFileProtocol` takes its writer lock for
  anything but `kQuery`).
- The default `DoSetAttributes` reads the path's attributes with
  `DoGetPathInfo`, works out the new ones, and calls `BasicSetAttributes` only
  if they differ, all under the one lock.
- `DoGetPathInfo`'s comment says which attributes it must report.
  `DoCopyFile` and `DoCopyFolder`'s comments say copies keep attributes.
- The default `DoCopyFile` calls `DoSetAttributes` with the source's
  attributes once `BasicCopyFile` has made the copy, so a protocol that
  overrides only `BasicCopyFile` needn't copy attributes. The default
  `DoCopyFolder` does the same for a folder it creates, after copying what is
  in it. Both are within one protocol, so every attribute is supported on both
  sides.
- No cost for a protocol without attributes: the attribute checks compare
  empty flags, and nothing more is queried.

### FileSystem

```
  // Returns the attributes the protocol supports.
  //
  // If no protocol is registered with that name, or it doesn't support
  // FileProtocolFlag::kInfo, this returns an empty set of attributes.
  FileAttributes GetAttributeFlags(std::string_view protocol_name) const;

  // Changes the attributes in `mask` of a file or folder to match
  // `attributes`, leaving the others, as one operation: ...
  bool SetAttributes(std::string_view path, FileAttributes attributes,
                     FileAttributes mask = kAllFileAttributes);
```

- `CopyFile()` and `CopyFolder()`'s comments describe how copies keep
  attributes.
- `GenericCopyFile` and `GenericCopyFolder` (copies between protocols) work out
  the attributes both protocols support. If there are any, they read the
  source's, and set the destination's with that as the mask, after copying
  (after the contents, for a folder). With none shared, they query nothing
  more than now.

### MemoryFileProtocol

```
  // What a read-only file prevents, beyond opening it for writing.
  enum class ReadOnlyMode {
    kNoWrite,          // Nothing more: a read-only file can be deleted.
    kNoWriteOrDelete,  // Deleting it, including in a recursive folder delete.
  };

  explicit MemoryFileProtocol(FileProtocolFlags flags = kDefaultFlags,
                              ReadOnlyMode read_only_mode =
                                  ReadOnlyMode::kNoWrite);

  FileAttributes GetAttributeFlags() const override;  // kReadOnly, kHidden.

 protected:
  bool BasicSetAttributes(...) override;
```

- Each node holds its attributes. `DoGetPathInfo` reports them,
  `BasicOpenFile` fails to open a read-only file for writing, and
  `BasicDeleteFile` fails on a read-only file in `kNoWriteOrDelete`. Copies go
  through the default `DoCopyFile`, which keeps attributes; moves keep the
  node.

### LocalFileProtocol

A new internal platform interface, as `local_current_path.h` is:

```
namespace gb::internal {

// Returns the attributes local files support on this operating system.
FileAttributes GetLocalAttributeFlags();

// Returns the attributes of the file or folder at `path`, following links, or
// an empty set if they can't be read.
FileAttributes GetLocalAttributes(const std::filesystem::path& path);

// Sets the attributes of the file or folder at `path`, following links, to
// `attributes`, which are all supported. Returns false if it can't.
bool SetLocalAttributes(const std::filesystem::path& path,
                        FileAttributes attributes);

}  // namespace gb::internal
```

- `win_local_file_attributes.cc` uses `GetFileAttributesW`, and for a link
  (a reparse point) opens it to read what it links to, as `std::filesystem`
  does. Setting opens the path (following links) and changes only the
  read-only and hidden bits with `SetFileInformationByHandle`, keeping every
  other bit (archive, system, and the rest).
- `gen_local_file_attributes.cc` supports `kReadOnly` only, through
  `std::filesystem` permissions: read-only is no write permission for anyone;
  clearing it gives the owner write permission. It is compiled only off
  Windows, so it isn't built or tested here, as `gen_local_current_path.cc`
  isn't.
- `LocalFileProtocol` overrides `GetAttributeFlags()` and `BasicSetAttributes`
  (the default `DoSetAttributes` validates first), and `DoGetPathInfo` adds
  the attributes.
- Windows' file creation (`CREATE_ALWAYS`, which `fopen` uses for its `"w"`
  modes) refuses to replace a hidden file. `DoOpenFile` with `kReset` on an
  existing file instead empties it with `std::filesystem::resize_file` and
  opens it with `"r+b"`. `DoCopyFile` onto a hidden file clears its hidden
  attribute first, since the copy gets the source's attributes anyway. Both
  are portable code, needing no platform file.
- Performance: `DoGetPathInfo` makes one more system call
  (`GetFileAttributesW`), plus an open and close only for a link.

**Brittleness:**
- A protocol must report only the attributes it supports. `FileProtocol`
  doesn't mask what `DoGetPathInfo` returns, since its default implementations
  call `DoGetPathInfo` directly too; the common protocol tests check every
  protocol instead.
- A protocol that overrides `DoSetAttributes` must apply the mask itself, and
  one that overrides `DoCopyFile` or `DoCopyFolder` must keep attributes
  itself. The common protocol tests check both.

### To confirm

- CL3: `fopen` with `"wb"` fails on an existing hidden file on Windows, and
  `"r+b"` after `resize_file` works on one.
- CL3: `std::filesystem::copy_file` onto an existing hidden file fails on
  Windows (`CopyFile`'s documentation says so for hidden and read-only).
- CL3: `std::filesystem::copy_file` copies read-only and hidden to the new
  file, and the recursive `std::filesystem::copy` gives the folders it creates
  the source folders' attributes. If not, `LocalFileProtocol` sets them.
- CL3: `GetFileAttributesW` on a link reports the link's own attributes, not
  its target's (so links need the open).
- Already confirmed: `std::filesystem::remove` deletes read-only files on
  Windows, clearing the attribute first if it must (MSVC's `filesystem.cpp`),
  so `delete_at_exit` cleans up read-only files too.

## CLs

### CL1 [x] file: attribute API

Depends on: nothing.

- `file_types.h`: `FileAttribute`, `FileAttributes`, `kAllFileAttributes`,
  and `PathInfo::attributes`.
- `FileProtocol`: `GetAttributeFlags()`, `SetAttributes()`,
  `DoSetAttributes()`, and `BasicSetAttributes()`. The default
  `DoCopyFile()` and `DoCopyFolder()` keep attributes.
- `FileSystem`: `GetAttributeFlags()` and `SetAttributes()`. Copies between
  protocols keep the attributes both support.
- `TestProtocol`: attributes configured in `State` (`attribute_flags`), held
  in each `PathState`, with call counts for `DoSetAttributes` and
  `BasicSetAttributes`.
- No protocol in Game Bits supports an attribute yet, so no visible change.

**Verify**
- Standard checks (see CLAUDE.md).
- `file_protocol_test`: `SetAttributes` validates paths and takes the modify
  lock. The default `DoSetAttributes` fails for a missing path, applies the
  mask, succeeds without calling `BasicSetAttributes` when nothing changes,
  and calls it with the full new set otherwise. The default `DoCopyFile` and
  `DoCopyFolder` copy attributes, replacing a destination file's, including
  after a protocol's own `BasicCopyFile`, and only for folders they create.
  (No protocol here has a read-only folder that stops changes, so setting a
  folder's attributes after its contents is only checked by reading the code.)
- `file_system_test`: `GetAttributeFlags` is empty for an unregistered
  protocol or one without `kInfo`. `SetAttributes` fails without `kInfo` or
  `kFileWrite`, for unsupported attributes, and for invalid paths, ignores
  unsupported attributes in the mask, and otherwise reaches the protocol.
  Copies between protocols keep the shared attributes, leave the
  destination's others, and make no extra calls when none are shared.

### CL2 [ ] file: MemoryFileProtocol attributes

Depends on: CL1.

- `MemoryFileProtocol` supports `kReadOnly` and `kHidden`, with
  `ReadOnlyMode`.
- Common protocol tests for attributes, for any protocol that supports them:
  getting and setting each supported attribute on files and folders, with and
  without a mask, failing for unsupported attributes, a read-only file failing
  to open for writing or be copied onto, and copies and moves keeping
  attributes (a folder copy giving the folders it creates the source's, even
  when read-only).
- Backlog: an option for `MemoryFileProtocol` read-only folders to prevent
  adding, removing, and renaming in them, as on POSIX.

**Verify**
- Standard checks.
- Common protocol tests pass for `MemoryFileProtocol`, and are skipped for
  protocols without attributes.
- `memory_file_protocol_test`: read-only files can be deleted in `kNoWrite`,
  and in `kNoWriteOrDelete` can't be, nor can a folder holding one be deleted
  recursively (which deletes what came before it). Attributes change on an
  open file, which keeps its access.

### CL3 [ ] file: LocalFileProtocol attributes

Depends on: CL2 (for the common protocol tests).

- `local_file_attributes.h`, with `win_local_file_attributes.cc` and
  `gen_local_file_attributes.cc`, chosen in `CMakeLists.txt`.
- `LocalFileProtocol` reports and sets attributes, and writes over hidden
  files on Windows.
- Settles everything under To confirm, adjusting the design to match.

**Verify**
- Standard checks.
- Common protocol tests pass for `LocalFileProtocol`.
- `win_local_file_protocol_test`: hidden files can be opened with `kReset`,
  written with `FileSystem::WriteFile()`, and copied onto. Read-only files can
  be deleted, and so can folders holding them, recursively. Read-only folders
  are reported and set, and don't stop anything. Other Windows attributes
  (archive) are kept when setting read-only and hidden. A link reports and
  sets its target's attributes.
- A temporary protocol with `delete_at_exit` removes read-only files in it.
