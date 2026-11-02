#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

#include "minitest.hpp"
#include "tinysprite/io.hpp"

/* What happens to the file functions when the allocator gives up: they report a failure instead of
 * letting std::bad_alloc take the program down. */

/* The largest allocation that still succeeds while `refusing` is set. */
static constexpr size_t LARGEST_ALLOWED = size_t{1} << 20;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): the test's switch
static bool refusing = false;

void *operator new(size_t size)
{
    void *memory = refusing && size > LARGEST_ALLOWED ? nullptr : std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr) {
        throw std::bad_alloc();
    }
    return memory;
}

void operator delete(void *memory) noexcept
{
    std::free(memory);
}

void operator delete(void *memory, size_t /*size*/) noexcept
{
    std::free(memory);
}

/* Runs `call` with the allocator refusing large blocks. */
template <class Call> static auto refused(Call call)
{
    refusing = true;
    auto result = call();
    refusing = false;
    return result;
}

/* A sprite whose reading and writing need blocks over LARGEST_ALLOWED. */
static Sprite big_sprite()
{
    Sprite sprite(1024, 1024);
    sprite.addLayer("second");
    return sprite;
}

TEST(pxa_decode_reports_a_failed_allocation)
{
    const std::vector<uint8_t> data = pxa_encode(big_sprite());
    ASSERT_TRUE(!refused([&] { return pxa_decode(data); }).has_value());
}

TEST(pxa_load_reports_a_failed_allocation)
{
    const std::string path = "test_memory_failure.pxa";
    ASSERT_TRUE(pxa_save(big_sprite(), path));
    ASSERT_TRUE(!refused([&] { return pxa_load(path); }).has_value());
    std::remove(path.c_str());
}

TEST(pxa_save_reports_a_failed_allocation)
{
    const std::string path = "test_memory_failure_save.pxa";
    const Sprite sprite = big_sprite();
    ASSERT_TRUE(!refused([&] { return pxa_save(sprite, path); }));
}

TEST(image_import_reports_a_failed_allocation)
{
    const std::string path = "test_memory_failure.png";
    ASSERT_TRUE(image_export(big_sprite(), 0, path, 1));
    ASSERT_TRUE(!refused([&] { return image_import(path); }).has_value());
    std::remove(path.c_str());
}

int main()
{
    RUN(pxa_decode_reports_a_failed_allocation);
    RUN(pxa_load_reports_a_failed_allocation);
    RUN(pxa_save_reports_a_failed_allocation);
    RUN(image_import_reports_a_failed_allocation);
    return TEST_SUMMARY();
}
