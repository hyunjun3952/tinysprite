#include "icons.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace icons
{

namespace
{

/*
 * The tool and shape icons as strokes on the 20-unit grid (x, y pairs; a circle is its center and
 * radius). The ink of each is about 15 units long, like the glyph icons, and every line is drawn
 * PATH_STROKE wide. Closed shapes are outlines; only the bucket's drop, the wand's sparkle and the
 * new color of Replace are filled.
 */

constexpr float PENCIL_BODY[] = {4.0f, 16.0f, 4.0f, 12.6f, 13.2f, 3.4f, 16.6f, 6.8f, 7.4f, 16.0f};
constexpr float PENCIL_TIP[] = {11.2f, 5.4f, 14.6f, 8.8f};
constexpr Stroke PENCIL[] = {{PENCIL_BODY, StrokeKind::Closed, 5},
                             {PENCIL_TIP, StrokeKind::Open, 2}};

constexpr float ERASER_BODY[] = {3.5f, 12.5f, 10.5f, 5.5f, 16.5f, 11.5f, 11.0f, 17.0f, 8.0f, 17.0f};
constexpr float ERASER_BAND[] = {7.0f, 9.0f, 13.0f, 15.0f};
constexpr float ERASER_FLOOR[] = {11.0f, 17.0f, 17.0f, 17.0f};
constexpr Stroke ERASER[] = {{ERASER_BODY, StrokeKind::Closed, 5},
                             {ERASER_BAND, StrokeKind::Open, 2},
                             {ERASER_FLOOR, StrokeKind::Open, 2}};

constexpr float BUCKET_CAN[] = {3.5f, 10.0f, 9.5f, 4.0f, 15.5f, 10.0f, 9.5f, 16.0f};
constexpr float BUCKET_RIM[] = {3.5f, 10.0f, 15.5f, 10.0f};
constexpr float BUCKET_DROP[] = {16.8f, 12.2f, 18.2f, 14.8f, 17.5f,
                                 16.2f, 16.1f, 16.2f, 15.4f, 14.8f};
constexpr Stroke BUCKET[] = {{BUCKET_CAN, StrokeKind::Closed, 4},
                             {BUCKET_RIM, StrokeKind::Open, 2},
                             {BUCKET_DROP, StrokeKind::Fill, 5}};

constexpr float DROPPER_TUBE[] = {4.0f,  16.0f, 4.0f, 14.5f, 11.0f, 7.5f,
                                  12.5f, 9.0f,  5.5f, 16.0f, 4.0f,  16.0f};
constexpr float DROPPER_COLLAR[] = {10.0f, 6.5f, 13.5f, 10.0f};
constexpr float DROPPER_BULB[] = {14.5f, 5.5f, 2.6f};
constexpr Stroke EYEDROPPER[] = {{DROPPER_TUBE, StrokeKind::Open, 6},
                                 {DROPPER_COLLAR, StrokeKind::Open, 2},
                                 {DROPPER_BULB, StrokeKind::Circle, 1}};

constexpr float SELECT_BOX[] = {3.5f, 3.5f, 16.5f, 3.5f, 16.5f, 16.5f, 3.5f, 16.5f};
constexpr Stroke SELECT[] = {{SELECT_BOX, StrokeKind::Dashed, 4}};

constexpr float LASSO_LOOP[] = {10.5f, 8.0f, 6.5f};
constexpr float LASSO_TAIL[] = {6.0f, 12.5f, 5.0f, 15.5f, 7.5f, 17.5f};
constexpr Stroke LASSO[] = {{LASSO_LOOP, StrokeKind::Circle, 1}, {LASSO_TAIL, StrokeKind::Open, 3}};

constexpr float WAND_STICK[] = {3.5f, 16.5f, 12.0f, 8.0f};
constexpr float WAND_SPARKLE[] = {14.80f, 1.60f,  15.51f, 4.23f,  18.22f, 4.09f,  15.94f,
                                  5.57f,  16.92f, 8.11f,  14.80f, 6.40f,  12.68f, 8.11f,
                                  13.66f, 5.57f,  11.38f, 4.09f,  14.09f, 4.23f};
constexpr Stroke WAND[] = {{WAND_STICK, StrokeKind::Open, 2}, {WAND_SPARKLE, StrokeKind::Fill, 10}};

constexpr float SLICE_TL[] = {3.5f, 7.5f, 3.5f, 3.5f, 7.5f, 3.5f};
constexpr float SLICE_TR[] = {12.5f, 3.5f, 16.5f, 3.5f, 16.5f, 7.5f};
constexpr float SLICE_BR[] = {16.5f, 12.5f, 16.5f, 16.5f, 12.5f, 16.5f};
constexpr float SLICE_BL[] = {7.5f, 16.5f, 3.5f, 16.5f, 3.5f, 12.5f};
constexpr Stroke SLICE[] = {{SLICE_TL, StrokeKind::Open, 3},
                            {SLICE_TR, StrokeKind::Open, 3},
                            {SLICE_BR, StrokeKind::Open, 3},
                            {SLICE_BL, StrokeKind::Open, 3}};

/* a hand: three fingers, then the little finger, palm and thumb in one line */
constexpr float PAN_INDEX[] = {6.0f, 11.0f, 6.0f, 5.5f, 7.2f, 4.4f, 8.4f, 5.5f, 8.4f, 9.5f};
constexpr float PAN_MIDDLE[] = {8.4f, 9.0f, 8.4f, 3.6f, 9.6f, 2.5f, 10.8f, 3.6f, 10.8f, 9.0f};
constexpr float PAN_RING[] = {10.8f, 9.0f, 10.8f, 4.6f, 12.0f, 3.5f, 13.2f, 4.6f, 13.2f, 9.5f};
constexpr float PAN_PALM[] = {13.2f, 9.5f,  13.2f, 6.6f,  14.4f, 5.5f,  15.6f, 6.6f,
                              15.6f, 12.0f, 14.0f, 16.5f, 8.5f,  16.5f, 4.0f,  11.5f,
                              4.4f,  10.2f, 5.6f,  10.2f, 6.0f,  11.0f};
constexpr Stroke PAN[] = {{PAN_INDEX, StrokeKind::Open, 5},
                          {PAN_MIDDLE, StrokeKind::Open, 5},
                          {PAN_RING, StrokeKind::Open, 5},
                          {PAN_PALM, StrokeKind::Open, 11}};

constexpr float LINE_SEGMENT[] = {4.0f, 16.0f, 16.0f, 4.0f};
constexpr Stroke LINE[] = {{LINE_SEGMENT, StrokeKind::Open, 2}};

constexpr float RECT_BOX[] = {3.5f, 4.5f, 16.5f, 4.5f, 16.5f, 15.5f, 3.5f, 15.5f};
constexpr Stroke RECT[] = {{RECT_BOX, StrokeKind::Closed, 4}};

constexpr float ELLIPSE_RING[] = {10.0f, 10.0f, 6.8f};
constexpr Stroke ELLIPSE[] = {{ELLIPSE_RING, StrokeKind::Circle, 1}};

constexpr float TRIANGLE_CORNERS[] = {10.0f, 3.2f, 17.0f, 16.0f, 3.0f, 16.0f};
constexpr Stroke TRIANGLE[] = {{TRIANGLE_CORNERS, StrokeKind::Closed, 3}};

constexpr float DIAMOND_CORNERS[] = {10.0f, 2.8f, 17.2f, 10.0f, 10.0f, 17.2f, 2.8f, 10.0f};
constexpr Stroke DIAMOND[] = {{DIAMOND_CORNERS, StrokeKind::Closed, 4}};

/* point up, radius 7.4 around the middle */
constexpr float HEXAGON_CORNERS[] = {10.00f, 2.60f,  16.41f, 6.30f,  16.41f, 13.70f,
                                     10.00f, 17.40f, 3.59f,  13.70f, 3.59f,  6.30f};
constexpr Stroke HEXAGON[] = {{HEXAGON_CORNERS, StrokeKind::Closed, 6}};

/* five points of radius 7.6, the inner corners at 3.2 */
constexpr float STAR_CORNERS[] = {10.00f, 3.00f,  11.88f, 8.01f,  17.23f, 8.25f, 13.04f,
                                  11.59f, 14.47f, 16.75f, 10.00f, 13.80f, 5.53f, 16.75f,
                                  6.96f,  11.59f, 2.77f,  8.25f,  8.12f,  8.01f};
constexpr Stroke STAR[] = {{STAR_CORNERS, StrokeKind::Closed, 10}};

constexpr float CROSS_CORNERS[] = {7.6f,  3.0f,  12.4f, 3.0f,  12.4f, 7.6f,  17.0f, 7.6f,
                                   17.0f, 12.4f, 12.4f, 12.4f, 12.4f, 17.0f, 7.6f,  17.0f,
                                   7.6f,  12.4f, 3.0f,  12.4f, 3.0f,  7.6f,  7.6f,  7.6f};
constexpr Stroke CROSS[] = {{CROSS_CORNERS, StrokeKind::Closed, 12}};

/* the palette's "put the foreground color in this swatch": an empty swatch, an arrow, a full one */
constexpr float REPLACE_FROM[] = {3.0f, 3.0f, 9.0f, 3.0f, 9.0f, 9.0f, 3.0f, 9.0f};
constexpr float REPLACE_TO[] = {11.0f, 11.0f, 17.0f, 11.0f, 17.0f, 17.0f, 11.0f, 17.0f};
constexpr float REPLACE_ARROW[] = {12.0f, 4.5f, 15.5f, 4.5f, 15.5f, 8.0f};
constexpr float REPLACE_HEAD[] = {13.7f, 6.3f, 15.5f, 8.0f, 17.3f, 6.3f};
constexpr Stroke REPLACE[] = {{REPLACE_FROM, StrokeKind::Closed, 4},
                              {REPLACE_TO, StrokeKind::Fill, 4},
                              {REPLACE_ARROW, StrokeKind::Open, 3},
                              {REPLACE_HEAD, StrokeKind::Open, 3}};

template <size_t N> constexpr Path pathOf(const Stroke (&strokes)[N])
{
    return Path{strokes, static_cast<int>(N)};
}

constexpr Path PENCIL_PATH = pathOf(PENCIL);
constexpr Path ERASER_PATH = pathOf(ERASER);
constexpr Path BUCKET_PATH = pathOf(BUCKET);
constexpr Path EYEDROPPER_PATH = pathOf(EYEDROPPER);
constexpr Path SELECT_PATH = pathOf(SELECT);
constexpr Path LASSO_PATH = pathOf(LASSO);
constexpr Path WAND_PATH = pathOf(WAND);
constexpr Path SLICE_PATH = pathOf(SLICE);
constexpr Path PAN_PATH = pathOf(PAN);
constexpr Path LINE_PATH = pathOf(LINE);
constexpr Path RECT_PATH = pathOf(RECT);
constexpr Path ELLIPSE_PATH = pathOf(ELLIPSE);
constexpr Path TRIANGLE_PATH = pathOf(TRIANGLE);
constexpr Path DIAMOND_PATH = pathOf(DIAMOND);
constexpr Path HEXAGON_PATH = pathOf(HEXAGON);
constexpr Path STAR_PATH = pathOf(STAR);
constexpr Path CROSS_PATH = pathOf(CROSS);
constexpr Path REPLACE_PATH = pathOf(REPLACE);

/* The box of one stroke, its line width (`half` on each side) included except for a fill. */
void growBox(InkBox &box, const Stroke &stroke, float half)
{
    if (stroke.kind == StrokeKind::Circle) {
        const float reach = stroke.xy[2] + half;
        box.x0 = std::min(box.x0, stroke.xy[0] - reach);
        box.y0 = std::min(box.y0, stroke.xy[1] - reach);
        box.x1 = std::max(box.x1, stroke.xy[0] + reach);
        box.y1 = std::max(box.y1, stroke.xy[1] + reach);
        return;
    }
    const float pad = stroke.kind == StrokeKind::Fill ? 0.0f : half;
    for (int i = 0; i < stroke.count; i++) {
        const float x = stroke.xy[2 * static_cast<size_t>(i)];
        const float y = stroke.xy[2 * static_cast<size_t>(i) + 1];
        box.x0 = std::min(box.x0, x - pad);
        box.y0 = std::min(box.y0, y - pad);
        box.x1 = std::max(box.x1, x + pad);
        box.y1 = std::max(box.y1, y + pad);
    }
}

} // namespace

const Path *path(Icon icon)
{
    switch (icon) {
    case Icon::Pencil:
        return &PENCIL_PATH;
    case Icon::Eraser:
        return &ERASER_PATH;
    case Icon::Bucket:
        return &BUCKET_PATH;
    case Icon::Eyedropper:
        return &EYEDROPPER_PATH;
    case Icon::Select:
        return &SELECT_PATH;
    case Icon::Lasso:
        return &LASSO_PATH;
    case Icon::Wand:
        return &WAND_PATH;
    case Icon::Slice:
        return &SLICE_PATH;
    case Icon::Pan:
        return &PAN_PATH;
    case Icon::Line:
        return &LINE_PATH;
    case Icon::Rect:
        return &RECT_PATH;
    case Icon::Ellipse:
        return &ELLIPSE_PATH;
    case Icon::Triangle:
        return &TRIANGLE_PATH;
    case Icon::Diamond:
        return &DIAMOND_PATH;
    case Icon::Hexagon:
        return &HEXAGON_PATH;
    case Icon::Star:
        return &STAR_PATH;
    case Icon::Cross:
        return &CROSS_PATH;
    case Icon::Replace:
        return &REPLACE_PATH;
    default:
        return nullptr;
    }
}

InkBox path_ink(const Path &path)
{
    InkBox box{PATH_GRID, PATH_GRID, 0.0f, 0.0f};
    for (int s = 0; s < path.count; s++) {
        growBox(box, path.strokes[s], PATH_STROKE / 2.0f);
    }
    return box;
}

float path_stroke(float uiScale)
{
    return PATH_STROKE * uiScale;
}

} // namespace icons
