# Copy-on-Write (COW) File System - Detailed Implementation

## File Structure
The project consists of three main files:
- `cowfs.hpp` - Main header file
- `cowfs.cpp` - Core implementation
- `cowfs_metadata.hpp` - Metadata management header
- `cowfs_metadata.cpp` - Metadata implementation

## 1. cowfs.hpp
### Constants and Types
```cpp
constexpr size_t BLOCK_SIZE = 4096;
constexpr size_t MAX_FILENAME_LENGTH = 255;
constexpr size_t MAX_FILES = 1024;
using fd_t = int32_t;
```

### Enums and Structures
```cpp
enum class FileMode {
    READ = 0x01,
    WRITE = 0x02,
    CREATE = 0x04
};

struct FileStatus {
    bool is_open;
    bool is_modified;
    size_t current_size;
    size_t current_version;
};

struct Block {
    uint8_t data[BLOCK_SIZE];
    size_t next_block;
    bool is_used;
};

struct Inode {
    char filename[MAX_FILENAME_LENGTH];
    size_t first_block;
    size_t size;
    size_t version_count;
    bool is_used;
    std::vector<VersionInfo> version_history;
};

struct VersionInfo {
    size_t version_number;
    size_t block_index;
    size_t size;
    std::string timestamp;
};
```

### COWFileSystem Class Methods
```cpp
class COWFileSystem {
public:
    // Constructor/Destructor
    COWFileSystem(const std::string& disk_path, size_t disk_size);
    ~COWFileSystem();

    // Core Operations
    fd_t create(const std::string& filename);
    fd_t open(const std::string& filename, FileMode mode);
    ssize_t read(fd_t fd, void* buffer, size_t size);
    ssize_t write(fd_t fd, const void* buffer, size_t size);
    int close(fd_t fd);

    // Version Management
    size_t get_version_count(fd_t fd);
    bool revert_to_version(fd_t fd, size_t version);
    std::vector<VersionInfo> get_version_history(fd_t fd);

    // File System Operations
    bool list_files(std::vector<std::string>& files);
    size_t get_file_size(fd_t fd);
    FileStatus get_file_status(fd_t fd);

    // Memory Management
    size_t get_total_memory_usage() const;
    void garbage_collect();
};
```

## 2. cowfs.cpp
### Constructor/Destructor Implementation
```cpp
COWFileSystem::COWFileSystem(const std::string& disk_path, size_t disk_size)
```
- Initializes file system with specified size
- Allocates space for inodes and blocks
- Creates or loads disk file

```cpp
~COWFileSystem()
```
- Saves current state to disk
- Cleans up resources

### Core Operations Implementation
```cpp
create(const std::string& filename)
```
- Finds free inode
- Initializes inode with filename
- Returns file descriptor

```cpp
open(const std::string& filename, FileMode mode)
```
- Finds existing inode
- Creates file descriptor
- Sets appropriate mode and position

```cpp
read(fd_t fd, void* buffer, size_t size)
```
- Validates file descriptor
- Reads from current block chain
- Returns bytes read

```cpp
write(fd_t fd, const void* buffer, size_t size)
```
- Creates new version using COW
- Allocates new blocks as needed
- Updates version history
- Returns bytes written

```cpp
close(fd_t fd)
```
- Validates file descriptor
- Marks descriptor as invalid
- Updates file system state

### Helper Functions Implementation
```cpp
initialize_disk()
```
- Creates new disk file or loads existing
- Initializes data structures

```cpp
find_inode(const std::string& filename)
```
- Searches inode array for filename
- Returns pointer to inode if found

```cpp
allocate_block(size_t& block_index)
```
- Finds unused block
- Marks block as used
- Returns success/failure

```cpp
copy_block(size_t source_block, size_t& dest_block)
```
- Implements COW mechanism
- Creates new block with copied data

## 3. cowfs_metadata.hpp
### MetadataManager Class
```cpp
class MetadataManager {
public:
    static bool save_and_print_metadata(COWFileSystem& fs, const std::string& version_label);
    static void print_metadata(COWFileSystem& fs);
    static bool save_metadata(COWFileSystem& fs, const std::string& version_label);
private:
    static std::string generate_metadata_json(COWFileSystem& fs);
};
```

## 4. cowfs_metadata.cpp
### Metadata Functions Implementation
```cpp
generate_metadata_json(COWFileSystem& fs)
```
- Creates JSON structure
- Includes file system stats
- Lists all files and versions

```cpp
save_and_print_metadata(COWFileSystem& fs, const std::string& version_label)
```
- Generates JSON metadata
- Prints to console
- Saves to file

```cpp
print_metadata(COWFileSystem& fs)
```
- Displays current system state
- Shows version history

```cpp
save_metadata(COWFileSystem& fs, const std::string& version_label)
```
- Creates metadata file
- Names file with version label

### JSON Format Example
```json
{
  "filesystem": {
    "total_memory_usage": 8192,
    "files": [
      {
        "name": "test.txt",
        "size": 22,
        "version_count": 2,
        "is_open": false,
        "version_history": [
          {
            "version_number": 1,
            "block_index": 1,
            "size": 22,
            "timestamp": "2024-03-28 15:30:45"
          }
        ]
      }
    ]
  }
}
```

## Professor's Requirements Met

1. **4096-byte Buffer Requirement**
- Implemented through `BLOCK_SIZE` constant
- Used in Block structure for data storage

2. **File Descriptor with Inode**
- File descriptors maintain link to inodes
- Tracks file state and position

3. **Complete File System**
- Full implementation of basic operations
- Versioning support
- Metadata management

4. **Large Disk Section Allocation**
- Configurable disk size
- Efficient block management
- Version storage

5. **Efficient Block Combination**
- Copy-on-Write implementation
- Block linking for large files
- Version tracking

## Key Features
- Version Control
- Data Integrity
- Resource Management
- Metadata Tracking
- JSON Format Reports
- Crash Recovery Support

## Usage Example
```cpp
COWFileSystem fs("cowfs.disk", 1024 * 1024);  // 1MB space
fd_t fd = fs.create("test.txt");
fs.write(fd, "Hello", 5);  // Version 1
fs.write(fd, "World", 5);  // Version 2 (COW)
fs.close(fd);
```

This markdown file provides a complete documentation of the project, including all components, functions, and how they meet the professor's requirements.
