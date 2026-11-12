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
    people. `List()` still returns it, and what else it affects depends on
    the protocol.
- Attributes don't guarantee behavior: a file that isn't read-only may still
  not be writable, for any number of reasons. A protocol whose file system
  Game Bits controls (`MemoryFileProtocol`) specifies exactly what each
  attribute does; one backed by another file system (`LocalFileProtocol`)
  leaves it to that file system.
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

Supports `kReadOnly` and `kHidden`. A read-only file can't be opened for
writing. Its constructor gains optional `ReadOnlyFlags`, which add what else a
read-only file prevents. With none (the default), it can still be deleted.
- `kPreventDelete`: it can't be deleted either, as on OZ-3 disks and in
  Windows' own tools. A recursive folder delete fails at the first read-only
  file in it, having deleted what came before, as for any other partial delete.

A read-only folder prevents nothing, as on Windows. Making it act as on POSIX
would be another `ReadOnlyFlag` (see the backlog). A hidden file or folder
prevents nothing.

Attributes can be changed while a file is open, and an open file keeps the
access it was opened with.

### LocalFileProtocol

Supports `kReadOnly` everywhere, and `kHidden` on Windows. Elsewhere, hidden
is a name starting with `.`, which is a naming convention rather than an
attribute that can be set, so it is not supported.

- The attributes are the operating system's: Windows' read-only and hidden
  attributes, or no write permission for anyone elsewhere, for files and
  folders alike. What they prevent is up to the operating system, and isn't
  specified or tested (Observed lists what Windows does).
- Links are followed, as for everything else: a link reports and sets the
  attributes of what it links to.
- A drive's root folder, like the protocol's, has no attributes, although
  Windows reports one as hidden.

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
- `MemoryFileProtocol::ReadOnlyFlag` and `ReadOnlyFlags`, with
  `kPreventDelete`, matching `FileFlag` and `FileProtocolFlag`. Flags, not a
  mode, so what a read-only folder prevents can be added as another flag.


### FileProtocol and FileSystem

```
  // FileProtocol: the attributes this protocol supports, on both files and
  // folders (none by default). Only used with FileProtocolFlag::kInfo.
  virtual FileAttributes GetAttributeFlags() const;

  bool SetAttributes(std::string_view protocol_name, std::string_view path,
                     FileAttributes attributes, FileAttributes mask);

 protected:
  // Called only with kInfo and kFileWrite, `mask` holding only supported
  // attributes, and `attributes` only attributes in `mask`.
  virtual bool DoSetAttributes(std::string_view protocol_name,
                               std::string_view path,
                               FileAttributes attributes, FileAttributes mask);

  // Called only on an existing path, with every attribute it should have,
  // when they differ from what it has.
  virtual bool BasicSetAttributes(std::string_view protocol_name,
                                  std::string_view path,
                                  FileAttributes attributes);
```

- `FileSystem::SetAttributes()` checks the protocol's flags and the
  attributes, then removes unsupported attributes from `mask`, and attributes
  outside `mask` from `attributes`, so a protocol never sees either.
- `FileProtocol::SetAttributes()` validates the path and takes
  `LockType::kModify`. The default `DoSetAttributes` reads the attributes with
  `DoGetPathInfo`, applies the mask, and calls `BasicSetAttributes` only if
  they change, all under the one lock. A root folder has none, so setting any
  on one fails.
- The default `DoCopyFile` sets the copy's attributes with `DoSetAttributes`
  after `BasicCopyFile`, so a protocol overriding only `BasicCopyFile` keeps
  attributes for free. The default `DoCopyFolder` does the same for each
  folder it creates, after copying what is in it.
- `GenericCopyFile` and `GenericCopyFolder` (copies between protocols) work
  out the attributes both protocols support, and set those on the copy, with
  them as the mask. With none shared, they query nothing more than before.
- A protocol without attributes costs nothing more: the checks compare empty
  flags.

### MemoryFileProtocol

- Each node holds its attributes, read and written only under the protocol's
  lock. `DoGetPathInfo` reports them, `BasicOpenFile` refuses to open a
  read-only file for writing, and `BasicDeleteFile` refuses a read-only file
  with `kPreventDelete`.
- Copies go through the default `DoCopyFile`, and moves keep the node, so both
  keep attributes.

### LocalFileProtocol

An internal platform interface, `local_file_attributes.h`, as
`local_current_path.h` is:

```
namespace gb::internal {

// Returns the attributes local files support on this operating system.
FileAttributes GetLocalAttributeFlags();

// Returns the attributes of the file or folder at `path`, following links, or
// an empty set if they can't be read.
FileAttributes GetLocalAttributes(const std::filesystem::path& path);

// Sets the attributes of the file or folder at `path`, following links, to
// `attributes`, which must all be in GetLocalAttributeFlags(). Returns false if
// it can't.
bool SetLocalAttributes(const std::filesystem::path& path,
                        FileAttributes attributes);

}  // namespace gb::internal
```

- `win_local_file_attributes.cc` reads with `GetFileAttributesW`, and opens a
  link (a reparse point) to read what it links to. Setting opens the path,
  following links, and changes only the read-only and hidden bits with
  `SetFileInformationByHandle`, keeping every other attribute as it is
  (Windows ignores those that can't be set).
- `gen_local_file_attributes.cc` supports `kReadOnly` only, through
  `std::filesystem` permissions: read-only is no write permission for anyone,
  and clearing it gives the owner write permission. It is compiled only off
  Windows, so it hasn't been built or tested, and it relies on
  `std::filesystem::copy_file` copying permissions, as the standard says.
- `LocalFileProtocol` overrides `GetAttributeFlags()` and
  `BasicSetAttributes`, and `DoGetPathInfo` adds the attributes, except for a
  root folder. `IsRootFolder()` (the protocol's root, or a drive's) is shared
  with the delete and move checks.
- Copying a file keeps attributes through `std::filesystem::copy_file`.
  `LocalFileProtocol` has no `DoCopyFolder` override: the recursive
  `std::filesystem::copy` doesn't give the folders it creates the source's
  attributes, so the default copies, through `DoCopyFile`.
- Opening, copying, and deleting are otherwise unchanged, so what an
  attribute prevents is whatever the operating system does.
- Performance: `DoGetPathInfo` makes one more system call
  (`GetFileAttributesW`), plus an open and close for a link. A folder copy
  lists and queries each folder through the protocol, rather than in one
  `std::filesystem::copy`.

**Brittleness:**
- A protocol must report only the attributes it supports. `FileProtocol`
  doesn't mask what `DoGetPathInfo` returns, since its own defaults call it
  directly; the common protocol tests check every protocol instead.
- A protocol that overrides `DoSetAttributes` must apply the mask itself, and
  one that overrides `DoCopyFile` or `DoCopyFolder` must keep attributes
  itself. The common protocol tests check both.
- `MemoryFileProtocol`'s node attributes are guarded by a comment, not a
  type: they are only safe to touch under the protocol's lock.

### Observed

What Windows does, with MSVC's `std::filesystem`. Only what the code relies on
(copies keeping attributes, and how links report them) is tested; the rest is
the operating system's to change.
- `fopen` with `"wb"` or `"w+b"` fails on an existing hidden file, while
  `"r+b"` works on one.
- `std::filesystem::copy_file` onto an existing hidden or read-only file
  fails.
- `std::filesystem::copy_file` gives the copy the source's read-only and
  hidden attributes, both for a new file and onto an existing one. The
  recursive `std::filesystem::copy` doesn't give the folders it creates the
  source folders' attributes.
- `GetFileAttributesW` on a junction reports the link's own attributes, not
  its target's, and opening it reports the target's.
- A read-only folder prevents nothing: files can be added, renamed, and
  removed in it, and it can be renamed and removed.
- `std::filesystem::remove` and `remove_all` delete read-only files and
  folders, so `delete_at_exit` cleans them up too.
- File symbolic links need developer mode, but junctions (links to folders)
  need no privileges.

### Building blocks

- **Common protocol tests** (`common_protocol_test.cc`): `GetAndSetAttributes`,
  `ReadOnlyFile`, `CopyFileKeepsAttributes`, `CopyFolderKeepsAttributes`, and
  `MoveKeepsAttributes` run for any protocol that supports attributes, and
  skip themselves otherwise. A new protocol gets them by instantiating
  `CommonProtocolTest`.
- **`TestProtocol`** supports attributes configured in `State`
  (`attribute_flags`), held in each `PathState`, with call counts for
  `DoSetAttributes` and `BasicSetAttributes`.
- **Default `DoSetAttributes`, `DoCopyFile`, and `DoCopyFolder`**: a protocol
  that implements `BasicSetAttributes` and reports attributes in
  `DoGetPathInfo` gets masking, and copies that keep attributes.
- **`WinLocalFileAttributesTest::CreateJunction()`** in
  `win_local_file_protocol_test.cc` makes a link to a folder without special
  privileges, for testing links on any Windows machine.

## Checks

- `file_protocol_test` and `file_system_test`: validation, masking, locking,
  and copies keeping attributes (within and between protocols), against
  `TestProtocol`.
- The common protocol tests pass for `MemoryFileProtocol` (with and without
  `kPreventDelete`) and `LocalFileProtocol`.
- `memory_file_protocol_test`: read-only files can be deleted by default but
  not with `kPreventDelete` (a recursive delete stops at one); read-only
  folders and hidden files and folders prevent nothing; attributes change on
  an open file, which keeps its access.
- `win_local_file_protocol_test`: other Windows attributes are kept when
  setting, a junction reports and sets its target's attributes, and a drive's
  root folder has none. `local_file_protocol_test`: `delete_at_exit` removes
  read-only files.
