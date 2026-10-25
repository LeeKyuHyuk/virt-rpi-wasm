/// Unit tests for the WASM module (file_manager.cc).
/// Tests decision points and state transitions, not trivial getters.
///
/// Includes file_manager.cc to access all extern "C" exports.

#include <gtest/gtest.h>
#include <cstring>

#include "../src/file_manager.cc"

// The functions are exported via extern "C" — accessible directly.
extern int load_file(const unsigned char* src, unsigned int length);
extern unsigned int read_at_offset(const void* buffer, unsigned int offset, unsigned int size);
extern void free_file();
extern const char* get_status();

// --- load_file: zero-length rejection (decision point) ---

TEST(FileManager, RejectsZeroLength) {
    // Arrange — ensure clean slate
    free_file();

    // Act — attempt to mount a zero-length file
    const unsigned char data[] = { 0x48, 0x65, 0x6C, 0x6C, 0x6F }; // "Hello"
    int result = load_file(data, 0);

    // Assert
    EXPECT_EQ(-1, result);
    EXPECT_STREQ("error", get_status());
}

// --- load_file: successful mount (state transition) ---

TEST(FileManager, MountsValidFile) {
    // Arrange
    free_file();

    // Act
    const unsigned char data[] = { 0x48, 0x65, 0x6C, 0x6C, 0x6F };
    int result = load_file(data, sizeof(data));

    // Assert
    EXPECT_EQ(0, result);
    EXPECT_STREQ("file_loaded", get_status());
}

// --- free_file: state transition ---

TEST(FileManager, FreeReturnsToIdle) {
    // Arrange — mount a file
    const unsigned char data[] = { 0x01, 0x02, 0x03 };
    load_file(data, sizeof(data));
    EXPECT_STREQ("file_loaded", get_status());

    // Act — unmount
    free_file();

    // Assert
    EXPECT_STREQ("idle", get_status());
}

// --- read_at_offset: returns 0 when no file is mounted ---

TEST(FileManager, ReadAtOffsetFailsWhenIdle) {
    // Arrange — ensure no file mounted
    free_file();

    // Act
    unsigned int bytes = read_at_offset(nullptr, 0, 4);

    // Assert
    EXPECT_EQ(0u, bytes);
    EXPECT_STREQ("error", get_status());
}

// --- read_at_offset: normal read within bounds ---

TEST(FileManager, ReadsNormalBytes) {
    // Arrange
    free_file();
    const unsigned char data[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE };
    load_file(data, sizeof(data));

    // Act — read 4 bytes from offset 0
    unsigned int bytes = read_at_offset(nullptr, 0, 4);

    // Assert
    EXPECT_EQ(4u, bytes);
    EXPECT_STREQ("file_loaded", get_status());
}

// --- read_at_offset: offset at exact file end returns 0 ---

TEST(FileManager, ReadAtExactEndReturnsZero) {
    // Arrange
    free_file();
    const unsigned char data[] = { 0x01, 0x02, 0x03 };
    load_file(data, sizeof(data));

    // Act — offset exactly at file length (no bytes remaining)
    unsigned int bytes = read_at_offset(nullptr, sizeof(data), 1);

    // Assert
    EXPECT_EQ(0u, bytes);
    EXPECT_STREQ("error", get_status());
}

// --- read_at_offset: offset beyond file end ---

TEST(FileManager, ReadBeyondFileEndReturnsZero) {
    // Arrange
    free_file();
    const unsigned char data[] = { 0x01, 0x02 };
    load_file(data, sizeof(data));

    // Act — offset far beyond file
    unsigned int bytes = read_at_offset(nullptr, 100, 4);

    // Assert
    EXPECT_EQ(0u, bytes);
    EXPECT_STREQ("error", get_status());
}

// --- read_at_offset: clamps read size at file boundary ---

TEST(FileManager, ClampsReadSizeAtFileEnd) {
    // Arrange
    free_file();
    const unsigned char data[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    load_file(data, sizeof(data));

    // Act — request 10 bytes from offset 3, but only 2 remain
    unsigned int bytes = read_at_offset(nullptr, 3, 10);

    // Assert — should clamp to 2
    EXPECT_EQ(2u, bytes);
    EXPECT_STREQ("file_loaded", get_status());
}

// --- read_at_offset: after free, read fails ---

TEST(FileManager, ReadFailsAfterFree) {
    // Arrange
    const unsigned char data[] = { 0x11, 0x22 };
    load_file(data, sizeof(data));
    free_file();

    // Act — try to read after unmount
    unsigned int bytes = read_at_offset(nullptr, 0, 1);

    // Assert
    EXPECT_EQ(0u, bytes);
}

// --- read_at_offset: partial read within bounds (not clamped) ---

TEST(FileManager, ReadPartialFromMiddle) {
    // Arrange
    free_file();
    const unsigned char data[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
    load_file(data, sizeof(data));

    // Act — read 3 bytes from offset 2
    unsigned int bytes = read_at_offset(nullptr, 2, 3);

    // Assert
    EXPECT_EQ(3u, bytes);
}

// --- load_file + free_file + mount another: state resets correctly ---

TEST(FileManager, MountsAfterFree) {
    // Arrange
    const unsigned char data1[] = { 0x01, 0x02 };
    load_file(data1, sizeof(data1));
    EXPECT_STREQ("file_loaded", get_status());

    // Act — free then mount again
    free_file();
    const unsigned char data2[] = { 0x0A, 0x0B, 0x0C, 0x0D, 0x0E };
    int result = load_file(data2, sizeof(data2));

    // Assert
    EXPECT_EQ(0, result);
    EXPECT_STREQ("file_loaded", get_status());
}
