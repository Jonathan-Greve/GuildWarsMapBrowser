#include <pch.h>
#include "show_how_to_use_dat_comparer_guide.h"

void show_how_to_use_dat_comparer_guide(bool* p_open)
{
    if (!*p_open)
        return;

    // Start the Dear ImGui window
    if (ImGui::Begin("How to Use Guide", p_open))
    {
        ImGui::BeginChild("Scrolling");

        // Title
        ImGui::Text("How to Use the DAT File Comparison Tool");
        ImGui::Separator();

        // Instructions
        ImGui::BulletText("Click on the 'Select File' button to choose DAT files for comparison.");
        ImGui::BulletText("Add the files and wait for them to be processed.");
        ImGui::BulletText("Use the filter expression input to set up your comparison criteria.");
        ImGui::Separator();

        // Filtering Results
        ImGui::Text("Filtering Results:");
        ImGui::Indent();
        ImGui::BulletText("Supports logical operations like AND, OR, NOT, XOR.");
        ImGui::BulletText("Supports comparison operations like ==, !=, >=, <=, >, <.");
        ImGui::BulletText("Supports arithmetic operations like +, -, *, /, %.");
        ImGui::BulletText("Special function 'exists' can be used to check for hash presence.");
        ImGui::BulletText("Compare Types: hash, size, fname, fname0, fname1");
        ImGui::BulletText("hash0 and hash2 means the hash of DAT0 and DAT2 respectively. Similarly with fname, fname0, fname1");
        ImGui::BulletText("Note: 'fname, fname0, fname1' are used for comparing filenames (not names!) fname is the full filename while fname0 and fname1 are the two parts of it.");
        ImGui::Unindent();
        ImGui::Separator();

        // Writing Expressions
        ImGui::Text("Examples:");
        ImGui::Indent();
        ImGui::BulletText("This shows the files that have the same filehash in both DAT0 and DAT1: \"hash0 == hash1\"");
        ImGui::BulletText("Shows the files from DAT0 and DAT1 that are not equal or it exists in one DAT but not the other: '|hash0 != hash1\"");
        ImGui::BulletText("Shows the files from DAT0 and DAT1 that are not equal but exists in both DATs: \"hash0 != hash1 and exists(hash0, hash1)\"");
        ImGui::BulletText("Check for existence of a file hash: exists(hash0)");
        ImGui::BulletText("Show only the files where the file exists in all 3 DATs but the size of the file in DAT0 is at least 100 bytes bigger than in DAT1 and DAT2:");
        ImGui::Text("\"exists(hash0, hash1, hash2) and (size0 > size1 + 100 or size0 > size2 + 100)\"");
        ImGui::BulletText("For the filename 0x37510100 you could match it in DAT0 as with: \"fname0 == 0x3751\" or \"fname00==0x3751 and fname10==0x100\"");
        ImGui::Unindent();
        ImGui::Separator();

        // Start Comparison
        ImGui::Text("Starting the Comparison:");
        ImGui::Indent();
        ImGui::BulletText("Enter a valid expression and click 'Start filtering'.");
        ImGui::Unindent();
        ImGui::Separator();

        // Viewing Results
        ImGui::Text("Viewing Results:");
        ImGui::Indent();
        ImGui::BulletText("Results will be displayed in this window.");
        ImGui::BulletText("Use 'Clear filter' to reset the comparison.");
        ImGui::Unindent();
        ImGui::Separator();

        // Important Notes
        ImGui::Text("Important Notes:");
        ImGui::Indent();
        ImGui::BulletText("you can use decimal (base 10) or hexadecimal (base 16) for numbers. E.g. 16 == 0x10.");
        ImGui::Text("But you cannot use hex in hash, size, fname, fname1, fname2. I.e. you cannot do \"hash0x2 == hash2\".");
        ImGui::BulletText("Arithmetic operations can be performed within expressions.");
        ImGui::Unindent();

        ImGui::Text("More syntax examples:");
        ImGui::Indent();
        ImGui::BulletText("hash0 == hash1");
        ImGui::BulletText("hash0 == hash2");
        ImGui::BulletText("hash1 == hash0");
        ImGui::BulletText("hash1 == hash1");
        ImGui::BulletText("hash1 == hash2");
        ImGui::BulletText("hash2 == hash0");
        ImGui::BulletText("hash2 == hash1");
        ImGui::BulletText("hash2 == hash2");
        ImGui::BulletText("size0 == 150");
        ImGui::BulletText("size1 == 0");
        ImGui::BulletText("size2 == 200");
        ImGui::BulletText("size0 == 1");
        ImGui::BulletText("size1 == 1");
        ImGui::BulletText("size2 == 1");
        ImGui::BulletText("size2 > size1");
        ImGui::BulletText("size2 < size1");
        ImGui::BulletText("size2 <= size1");
        ImGui::BulletText("(hash2 == hash1 or hash2 == hash0)");
        ImGui::BulletText("(hash2 == hash1 or hash2 == hash0) or size1 == 0");
        ImGui::BulletText("(hash2 == hash1 or hash2 == hash0) or size1 == 0 and size1 == 1");
        ImGui::BulletText("(hash2 == hash1 or hash2 == hash0) or size1 == 0 and size1 == 0");
        ImGui::BulletText("(hash2 == hash1 or hash2 == hash0) or size1 == 0 and size0 == 150");
        ImGui::BulletText("hash2 == hash1 or hash2 == hash0");
        ImGui::BulletText("hash2 == hash1 or hash2 == hash0 or size1 == 0");
        ImGui::BulletText("hash2 == hash1 or hash2 == hash0 or size1 == 0 and size1 == 1");
        ImGui::BulletText("hash2 == hash1 or hash2 == hash0 or size1 == 0 and size1 == 0");
        ImGui::BulletText("hash2 == hash1 or hash2 == hash0 or size1 == 0 and size0 == 150");
        ImGui::BulletText("1 or 2");
        ImGui::BulletText("0 or 0");
        ImGui::BulletText("0 and 0");
        ImGui::BulletText("1 and 0");
        ImGui::BulletText("1 and 1");
        ImGui::BulletText("1 and 2");
        ImGui::BulletText("exists(hash0)");
        ImGui::BulletText("exists(hash1)");
        ImGui::BulletText("exists(hash2)");
        ImGui::BulletText("exists(hash3)");
        ImGui::BulletText("not exists(hash0)");
        ImGui::BulletText("not exists(hash1)");
        ImGui::BulletText("not exists(hash2)");
        ImGui::BulletText("not exists(hash3)");
        ImGui::BulletText("not exists(hash0) or exists(hash0)");
        ImGui::BulletText("not (exists(hash0) or exists(hash0))");
        ImGui::BulletText("(not exists(hash0)) or exists(hash0)");
        ImGui::BulletText("hash2 == hash1 or not hash2 == hash0");
        ImGui::BulletText("hash2 == hash1 not or hash2 == hash0");
        ImGui::BulletText("hash0 != hash1");
        ImGui::BulletText("size1 != 150");
        ImGui::BulletText("hash0 != size0");
        ImGui::BulletText("size2 != hash2");
        ImGui::BulletText("size1 > size0");
        ImGui::BulletText("size2 > 100");
        ImGui::BulletText("150 < size2");
        ImGui::BulletText("size0 < 150");
        ImGui::BulletText("size1 < 200");
        ImGui::BulletText("size0 >= 150");
        ImGui::BulletText("size1 <= 0");
        ImGui::BulletText("200 >= size2");
        ImGui::BulletText("size2 <= 300");
        ImGui::BulletText("100 <= size1");
        ImGui::BulletText("size0 == 150 and size1 == 0");
        ImGui::BulletText("hash1 == hash0 or size1 < size2");
        ImGui::BulletText("hash1 == hash0 or size1 > size2");
        ImGui::BulletText("size2 > size1 and hash2 != hash1");
        ImGui::BulletText("(size0 == 150 or size1 == 0) and hash2");
        ImGui::BulletText("hash0 and size0 == 150");
        ImGui::BulletText("not size0 == 150");
        ImGui::BulletText("not (hash1 == hash2)");
        ImGui::BulletText("not size2 < size1");
        ImGui::BulletText("not (size2 > 100 and size1 == 0)");
        ImGui::BulletText("not (size0 < 150 or size2 == 200)");
        ImGui::BulletText("not hash0 != size1");
        ImGui::BulletText("exists(hash0, hash1)");
        ImGui::BulletText("exists(hash3, hash1)");
        ImGui::BulletText("not exists(hash3)");
        ImGui::BulletText("exists(hash2) and hash2 == 2");
        ImGui::BulletText("exists(hash1) or size2 > 200");
        ImGui::BulletText("(size0 == 150 or size1 < size2) and not hash1");
        ImGui::BulletText("not (hash2 != hash1 and size1 >= 0)");
        ImGui::BulletText("(exists(hash0, hash1) or size2 < 300) and size0");
        ImGui::BulletText("not (size2 <= size0 or hash0 == hash1)");
        ImGui::BulletText("(size1 == 0 and not size0 == 150) or hash2");
        ImGui::BulletText("(size1 == 0 and not size0 == 150)");
        ImGui::BulletText("not (not size0 == 150)");
        ImGui::BulletText("not not size0 == 150");
        ImGui::BulletText("not not not size0 == 150");
        ImGui::BulletText("not (not (not (size0 == 150)))");
        ImGui::BulletText("(not (size1 > size0) and size2)");
        ImGui::BulletText("not (exists(hash3) or not size2 >= 200)");
        ImGui::BulletText("(exists(hash0) and not (size1 or not hash2))");
        ImGui::BulletText("(not (hash1 == hash0) and not (size2 < size1))");
        ImGui::BulletText("fname00 == fname00");
        ImGui::BulletText("fname01 != fname11");
        ImGui::BulletText("fname00 == fname10");
        ImGui::BulletText("fname11 == fname01");
        ImGui::BulletText("fname10 > fname00");
        ImGui::BulletText("fname00 < fname10");
        ImGui::BulletText("fname01 >= fname11");
        ImGui::BulletText("fname11 <= fname01");
        ImGui::BulletText("fname00 == 900 and fname10 == 980");
        ImGui::BulletText("fname11 == 981 or fname01 < fname00");
        ImGui::BulletText("size0 == 0x96");
        ImGui::BulletText("size0 != 0x96");
        ImGui::BulletText("size2 == 0xC8");
        ImGui::BulletText("size2 == 0xc8");
        ImGui::BulletText("size2 == 0XC8");
        ImGui::BulletText("size2 == 0Xc8");
        ImGui::BulletText("(NOT (HASH1 == HASH0) AND NOT (SIZE2 < SIZE1))");
        ImGui::BulletText("(Not (Hash1 == hash0) AnD not (SIzE2 < sizE1))");
        ImGui::BulletText("size0 == 100 + 50 + 1 + 1 - 2");
        ImGui::BulletText("size0 == 100 + 50 + 1 + 1 - 1");
        ImGui::BulletText("size0 + 1 == 100 + 50 + 1 + 1 - 1");
        ImGui::BulletText("size0 == size1 + 100 + 50");
        ImGui::BulletText("size0 < size0 - 1");
        ImGui::BulletText("size0 == size0 - 1");
        ImGui::BulletText("size0 > size0 - 1");
        ImGui::BulletText("1+1 > 1");
        ImGui::BulletText("1+1 == 2");
        ImGui::BulletText("1+1 != 2");
        ImGui::BulletText("3 + 2 == 5");
        ImGui::BulletText("10 - 5 == 5");
        ImGui::BulletText("4 * 5 == 20");
        ImGui::BulletText("20 / 4 == 5");
        ImGui::BulletText("21 % 5 == 1");
        ImGui::BulletText("(3 + 2) * 5 == 25");
        ImGui::BulletText("10 - (2 * 3) == 4");
        ImGui::BulletText("18 / (2 + 1) == 6");
        ImGui::BulletText("(15 % 4) + 1 == 4");
        ImGui::BulletText("5 * (3 - 1) == 10");
        ImGui::BulletText("(10 + 5) == (3 * 5)");
        ImGui::BulletText("20 - (15 / 3) == 15");
        ImGui::BulletText("(10 % 3) * 5 == 5");
        ImGui::BulletText("(18 / 2) - 3 == 6");
        ImGui::BulletText("((5 + 5) * 2) / 5 == 4");
        ImGui::BulletText("1 / 0 == 0");
        ImGui::BulletText("1 % 0 == 0");
        ImGui::BulletText("0 / 1 == 0");
        ImGui::BulletText("0 % 1 == 0");
        ImGui::BulletText("size0 / size1 == size2");
        ImGui::BulletText("fname00 == 0x384 and fname10 == 0x3d4");
        ImGui::BulletText("0x123 and 1");
        ImGui::BulletText("0x123 and 0x123");
        ImGui::BulletText("(0x123) and 1");
        ImGui::BulletText("(((0x123)) and (1))");
        ImGui::Unindent();

        // End the Dear ImGui window
        ImGui::EndChild();
        ImGui::End();
    }
}
