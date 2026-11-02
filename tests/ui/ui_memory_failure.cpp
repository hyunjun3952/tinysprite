/*
 * What the editor does when the allocator gives up in the middle of a frame or a click: the edit
 * or the ghosts are given up, and the program goes on. Built only when the UI is built; runs under
 * SDL_VIDEODRIVER=dummy.
 */

#include <cstdlib>
#include <memory>
#include <new>

#include "editor.hpp"
#include "minitest.hpp"
#include "ui_editor_support.hpp"

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

TEST(the_onion_skin_gives_way_when_memory_is_short)
{
    std::unique_ptr<Editor> e = Editor::create(renderer, 1024, 1024);
    e->addFrame();
    e->sprite().layer(0)->cel(1)->set(5, 5, Rgba{255, 0, 0, 255});
    OnionSkin skin;
    skin.before = 1;
    e->setOnion(true, skin);
    refusing = true;
    e->updateTexture();
    refusing = false;
    ASSERT_TRUE(e->shownPixel(5, 5) == (Rgba{255, 0, 0, 255})); /* the frame itself is drawn */
}

TEST(a_fill_gives_way_when_memory_is_short)
{
    std::unique_ptr<Editor> e = Editor::create(renderer, 2048, 1024);
    e->selectTool(ToolKind::Bucket);
    e->options().symmetry.mirror_x = true;
    refusing = true;
    e->press(0, 0);
    refusing = false;
    e->release();
    ASSERT_TRUE(!e->history().strokeActive());
    ASSERT_EQ_INT(0, e->sprite().layer(0)->cel(0)->get(0, 0).a);
    ASSERT_TRUE(e->takeMemoryShort());
}

int main()
{
    if (!ui_init()) {
        return 1;
    }
    RUN(the_onion_skin_gives_way_when_memory_is_short);
    RUN(a_fill_gives_way_when_memory_is_short);
    ui_quit();
    return TEST_SUMMARY();
}
