#include "map_ui.h"

#include <Preferences.h>
#include <math.h>

#include "config.h"

namespace {

using namespace ui;
using route_client::GeoPoint;
using route_client::Mode;

constexpr const char *kPrefsNamespace = "cydmap";
constexpr const char *kKeyFrom = "from";
constexpr const char *kKeyTo = "to";
constexpr const char *kKeyMode = "mode";
constexpr const char *kTitle = "Chi duong";

// Form layout.
constexpr int16_t kFieldX = 8;
constexpr int16_t kFieldW = kScreenW - 2 * kFieldX;
constexpr int16_t kFieldH = 28;
constexpr int16_t kFromLabelY = 28;
constexpr int16_t kToLabelY = 72;
constexpr int16_t kLabelGap = 10;
constexpr int16_t kFieldTextPad = 6;
constexpr int16_t kFieldRadius = 4;
constexpr int16_t kModeY = 118;
constexpr int16_t kModeH = 30;
constexpr int16_t kModeGap = 6;
constexpr int16_t kStatusY = 158;
constexpr int16_t kHintY = 180;
constexpr int16_t kTextSize2W = kFontW * 2;
constexpr size_t kSmallLineChars = (kScreenW - 2 * kFieldX) / kFontW;
constexpr const char *kModeLabels[] = {"Xe may", "Xe dap", "Di bo"};
constexpr Mode kModes[] = {Mode::Car, Mode::Bike, Mode::Foot};

// Map layout.
constexpr int16_t kStripH = 28;
constexpr int16_t kMapTop = kHeaderH + kStripH;
constexpr int16_t kMapBottom = kScreenH - kFooterH;
constexpr int16_t kMapH = kMapBottom - kMapTop;
constexpr int16_t kMapCx = kScreenW / 2;
constexpr int16_t kMapCy = kMapTop + kMapH / 2;
constexpr int16_t kStripLine1Y = kHeaderH + 4;
constexpr int16_t kStripLine2Y = kHeaderH + 16;
constexpr size_t kStripChars = (kScreenW - 2 * kMargin) / kFontW;
constexpr const char *kMapButtonLabels[] = {"Sua", "<", ">", "-", "+"};

// Map look (on top of the OpenStreetMap tiles).
constexpr uint16_t kMapBg = rgb565(0x1B, 0x22, 0x2C);  // where a tile is missing
constexpr uint16_t kCasingColor = kColorWhite;
constexpr uint16_t kRouteColor = rgb565(0x1A, 0x73, 0xE8);
constexpr uint16_t kStartColor = rgb565(0x2E, 0xCC, 0x71);
constexpr uint16_t kEndColor = rgb565(0xE7, 0x4C, 0x3C);
constexpr uint16_t kFocusColor = rgb565(0xFF, 0xD5, 0x4F);
constexpr uint16_t kLabelBox = rgb565(0xF5, 0xF5, 0xF5);
constexpr int16_t kCasingHalf = 3;  // 7 px wide
constexpr int16_t kRouteHalf = 1;   // 3 px wide
constexpr int16_t kEndpointR = 5;
constexpr int16_t kFocusR = 6;
constexpr float kMinSegmentPx = 1.5f;  // shorter segments are merged into the next one

// View limits.
constexpr float kFitMargin = 0.85f;
constexpr int kOverviewMaxZoom = 17;  // a very short route is not shown closer than this
constexpr int kStepZoom = 16;         // zoom used when showing one step (320 px ~ 700 m)
constexpr int kBrowseZoom = 15;       // "Ban do" without a route: around MAP_HOME_LAT/LON
constexpr int16_t kPanMinPx = 4;

// Scale bar.
constexpr float kScaleTargetPx = 60.0f;
constexpr int16_t kScaleBarX = 8;
constexpr int16_t kScaleBarY = kMapBottom - 8;
constexpr int16_t kScaleTick = 4;

// Clips segment (x0,y0)-(x1,y1) to the rectangle (Liang-Barsky); false when nothing is left.
bool clipSegment(float &x0, float &y0, float &x1, float &y1, float xmin, float ymin, float xmax, float ymax)
{
    const float dx = x1 - x0, dy = y1 - y0;
    const float p[4] = {-dx, dx, -dy, dy};
    const float q[4] = {x0 - xmin, xmax - x0, y0 - ymin, ymax - y0};
    float t0 = 0, t1 = 1;
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0) {
            if (q[i] < 0) {
                return false;
            }
            continue;
        }
        const float t = q[i] / p[i];
        if (p[i] < 0) {
            if (t > t1) {
                return false;
            }
            t0 = max(t0, t);
        } else {
            if (t < t0) {
                return false;
            }
            t1 = min(t1, t);
        }
    }
    const float sx = x0, sy = y0;
    x0 = sx + t0 * dx;
    y0 = sy + t0 * dy;
    x1 = sx + t1 * dx;
    y1 = sy + t1 * dy;
    return true;
}

// Round length (1, 2 or 5 x 10^n meters) that is about kScaleTargetPx long on screen.
float niceStep(float scale)
{
    const float target = kScaleTargetPx / scale;
    const float power = powf(10.0f, floorf(log10f(target)));
    for (float m : {1.0f, 2.0f, 5.0f}) {
        if (m * power >= target) {
            return m * power;
        }
    }
    return 10.0f * power;
}

}  // namespace

MapUi::MapUi(Adafruit_GFX &gfx) : gfx_(gfx)
{
    fromBox_ = {kFieldX, int16_t(kFromLabelY + kLabelGap), kFieldW, kFieldH};
    toBox_ = {kFieldX, int16_t(kToLabelY + kLabelGap), kFieldW, kFieldH};
    const int16_t modeW = (kFieldW - (int16_t)(kModeCount - 1) * kModeGap) / (int16_t)kModeCount;
    for (size_t i = 0; i < kModeCount; ++i) {
        modeButtons_[i] = {int16_t(kFieldX + i * (modeW + kModeGap)), kModeY, modeW, kModeH};
    }
}

// ---------------------------------------------------------------- state

void MapUi::loadSaved()
{
    loaded_ = true;
    from_ = MAP_DEFAULT_FROM;
    Preferences prefs;
    // Read-write so the namespace is created on first use (read-only begin() fails if it does not exist).
    if (!prefs.begin(kPrefsNamespace, false)) {
        return;
    }
    if (prefs.isKey(kKeyFrom)) {
        from_ = prefs.getString(kKeyFrom, from_);
    }
    to_ = prefs.getString(kKeyTo, "");
    const uint8_t mode = prefs.getUChar(kKeyMode, 0);
    mode_ = mode < kModeCount ? kModes[mode] : Mode::Car;
    prefs.end();
}

void MapUi::saveInputs()
{
    Preferences prefs;
    if (!prefs.begin(kPrefsNamespace, false)) {
        return;
    }
    prefs.putString(kKeyFrom, from_);
    prefs.putString(kKeyTo, to_);
    prefs.putUChar(kKeyMode, (uint8_t)mode_);
    prefs.end();
}

void MapUi::setStatus(const String &text, bool isError)
{
    status_ = text;
    statusIsError_ = isError;
}

void MapUi::forgetRoute()
{
    haveRoute_ = false;
    route_ = route_client::Route();  // frees the path and steps before the next search
    path_.clear();
    path_.shrink_to_fit();
    stepAt_.clear();
    stepAt_.shrink_to_fit();
    stepPathIndex_.clear();
    stepPathIndex_.shrink_to_fit();
    step_ = -1;
}

void MapUi::setFrom(const String &text)
{
    String t = text;
    t.trim();
    if (t != from_) {
        from_ = t;
        forgetRoute();
        setStatus(String(), false);
    }
}

void MapUi::setTo(const String &text)
{
    String t = text;
    t.trim();
    if (t != to_) {
        to_ = t;
        forgetRoute();
        setStatus(String(), false);
    }
}

void MapUi::show()
{
    if (!loaded_) {
        loadSaved();
    }
    visible_ = true;
    dragging_ = false;
    if (view_ == View::Map) {
        drawMapView();
    } else {
        view_ = View::Form;
        drawForm();
    }
}

// ---------------------------------------------------------------- form

void MapUi::drawField(const Rect &box, const char *label, const String &text, const char *placeholder)
{
    gfx_.setTextWrap(false);
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorMuted);
    gfx_.setCursor(box.x, box.y - kLabelGap);
    gfx_.print(label);
    gfx_.fillRoundRect(box.x, box.y, box.w, box.h, kFieldRadius, kColorRow);
    gfx_.drawRoundRect(box.x, box.y, box.w, box.h, kFieldRadius, kColorDivider);
    const bool empty = text.length() == 0;
    gfx_.setTextSize(2);
    gfx_.setTextColor(empty ? kColorMuted : kColorText);
    gfx_.setCursor(box.x + kFieldTextPad, box.y + (box.h - kFontH * 2) / 2 + 1);
    gfx_.print(ellipsize(empty ? String(placeholder) : toAscii(text), (box.w - 2 * kFieldTextPad) / kTextSize2W));
}

void MapUi::drawModes()
{
    for (size_t i = 0; i < kModeCount; ++i) {
        const bool on = kModes[i] == mode_;
        drawButton(gfx_, modeButtons_[i], kModeLabels[i], on ? kColorAccent : kColorButton, kColorWhite);
    }
}

void MapUi::drawFormStatus()
{
    gfx_.fillRect(0, kStatusY, kScreenW, kScreenH - kFooterH - kStatusY, kColorBg);
    String line = status_;
    uint16_t color = statusIsError_ ? kColorSystem : kColorText;
    if (line.length() == 0) {
        line = haveRoute_ ? "Da tim: " + route_client::formatDistance(route_.distanceM) + ", " +
                                route_client::formatDuration(route_.durationS) + " - bam \"Ban do\""
                          : String("Go diem den roi bam Tim. \"Ban do\" de xem quanh day");
        color = kColorText;
    }
    gfx_.setTextWrap(false);
    gfx_.setTextSize(1);
    gfx_.setTextColor(color);
    // Up to 2 lines, so a network error keeps its reason (DNS, TLS, RAM...).
    gfx_.setCursor(kFieldX, kStatusY);
    gfx_.print(line.substring(0, kSmallLineChars));
    if (line.length() > kSmallLineChars) {
        gfx_.setCursor(kFieldX, kStatusY + kFontH + 2);
        gfx_.print(ellipsize(line.substring(kSmallLineChars), kSmallLineChars));
    }
    gfx_.setTextColor(kColorMuted);
    gfx_.setCursor(kFieldX, kHintY);
    gfx_.print("Xe may di theo duong o to. Toa do: 20.86,106.68");
    gfx_.setCursor(kFieldX, kHintY + kFontH + 4);
    gfx_.print("Ban do (c) OpenStreetMap - Photon, OSRM");
}

void MapUi::drawFormFooter()
{
    drawButton(gfx_, footerSlot(0, kFormButtons), "Doi", kColorButton, kColorWhite);
    drawButton(gfx_, footerSlot(1, kFormButtons), "Ban do", kColorButton, kColorWhite);
    drawButton(gfx_, footerSlot(2, kFormButtons), "Tim", kColorAccent, kColorWhite);
}

void MapUi::drawForm()
{
    gfx_.fillScreen(kColorBg);
    drawBackHeader(gfx_, kTitle, "OpenStreetMap");
    drawField(fromBox_, "Tu (diem di):", from_, "Vi du: aeon mall le chan");
    drawField(toBox_, "Den (diem den):", to_, "Vi du: nha hat lon hai phong");
    drawModes();
    drawFormStatus();
    drawFormFooter();
}

MapAction MapUi::pressForm(int16_t x, int16_t y)
{
    if (backTapped(x, y)) {
        return MapAction::Exit;
    }
    if (fromBox_.contains(x, y)) {
        return MapAction::EditFrom;
    }
    if (toBox_.contains(x, y)) {
        return MapAction::EditTo;
    }
    for (size_t i = 0; i < kModeCount; ++i) {
        if (modeButtons_[i].contains(x, y) && kModes[i] != mode_) {
            mode_ = kModes[i];
            forgetRoute();
            setStatus(String(), false);
            drawModes();
            drawFormStatus();
            drawFormFooter();
            return MapAction::None;
        }
    }
    if (footerSlot(0, kFormButtons).contains(x, y)) {
        const String from = from_;
        from_ = to_;
        to_ = from;
        forgetRoute();
        setStatus(String(), false);
        drawForm();
    } else if (footerSlot(1, kFormButtons).contains(x, y)) {
        // Without a route the map simply opens around home, to look around.
        if (!haveRoute_) {
            centerOnHome();
        }
        view_ = View::Map;
        drawMapView();
    } else if (footerSlot(2, kFormButtons).contains(x, y)) {
        search();
    }
    return MapAction::None;
}

void MapUi::search()
{
    if (from_.length() == 0 || to_.length() == 0) {
        setStatus("Nhap ca diem di va diem den", true);
        drawFormStatus();
        return;
    }
    forgetRoute();
    String error;
    route_client::Place start, end;
    drawSplash(gfx_, kTitle, "Dang tim diem di...", ellipsize(toAscii(from_), kSmallLineChars).c_str());
    bool ok = route_client::findPlace(from_, start, error);
    if (!ok) {
        error = "Diem di: " + error;
    } else {
        drawSplash(gfx_, kTitle, "Dang tim diem den...", ellipsize(toAscii(to_), kSmallLineChars).c_str());
        ok = route_client::findPlace(to_, end, error);
        if (!ok) {
            error = "Diem den: " + error;
        }
    }
    if (ok) {
        drawSplash(gfx_, kTitle, "Dang tim duong...", ellipsize(end.name, kSmallLineChars).c_str());
        ok = route_client::findRoute(start.at, end.at, mode_, route_, error);
    }
    if (!ok) {
        forgetRoute();
        setStatus(error, true);
        view_ = View::Form;
        drawForm();
        return;
    }
    haveRoute_ = true;
    fromName_ = start.name;
    toName_ = end.name;
    setStatus(String(), false);
    saveInputs();
    buildWorldPath();
    fitRoute();
    indexSteps();
    view_ = View::Map;
    drawMapView();
}

// ---------------------------------------------------------------- map

void MapUi::buildWorldPath()
{
    const auto &path = route_.path;
    originX_ = map_tiles::lonToX(path[0].lon, map_tiles::kMaxZoom);
    originY_ = map_tiles::latToY(path[0].lat, map_tiles::kMaxZoom);
    auto toWorld = [this](const GeoPoint &g) {
        return WorldPoint{(float)(map_tiles::lonToX(g.lon, map_tiles::kMaxZoom) - originX_),
                          (float)(map_tiles::latToY(g.lat, map_tiles::kMaxZoom) - originY_)};
    };
    path_.clear();
    path_.reserve(path.size());
    for (const GeoPoint &g : path) {
        path_.push_back(toWorld(g));
    }
    stepAt_.clear();
    stepAt_.reserve(route_.steps.size());
    for (const route_client::Step &s : route_.steps) {
        stepAt_.push_back(toWorld(s.at));
    }
    // The lat/lon copy is no longer needed: give its RAM back.
    route_.path.clear();
    route_.path.shrink_to_fit();
}

void MapUi::indexSteps()
{
    stepPathIndex_.assign(stepAt_.size(), 0);
    size_t from = 0;
    for (size_t s = 0; s < stepAt_.size(); ++s) {
        float best = INFINITY;
        size_t bestIndex = from;
        // Steps follow the path, so the search starts at the previous step's point.
        for (size_t i = from; i < path_.size(); ++i) {
            const float dx = path_[i].x - stepAt_[s].x, dy = path_[i].y - stepAt_[s].y;
            const float d = dx * dx + dy * dy;
            if (d < best) {
                best = d;
                bestIndex = i;
            }
        }
        stepPathIndex_[s] = bestIndex;
        from = bestIndex;
    }
}

void MapUi::toScreen(const WorldPoint &p, float &x, float &y) const
{
    const double scale = ldexp(1.0, zoom_ - map_tiles::kMaxZoom);
    x = (float)(kMapCx + ((originX_ + p.x) * scale - centerX_));
    y = (float)(kMapCy + ((originY_ + p.y) * scale - centerY_));
}

map_tiles::View MapUi::tileView() const
{
    map_tiles::View v;
    v.zoom = zoom_;
    v.centerX = centerX_;
    v.centerY = centerY_;
    v.left = 0;
    v.top = kMapTop;
    v.width = kScreenW;
    v.height = kMapH;
    return v;
}

void MapUi::centerOnHome()
{
    zoom_ = kBrowseZoom;
    centerX_ = map_tiles::lonToX(MAP_HOME_LON, zoom_);
    centerY_ = map_tiles::latToY(MAP_HOME_LAT, zoom_);
    step_ = -1;
}

void MapUi::fitRoute()
{
    float minX = path_[0].x, maxX = minX, minY = path_[0].y, maxY = minY;
    for (const WorldPoint &p : path_) {
        minX = min(minX, p.x);
        maxX = max(maxX, p.x);
        minY = min(minY, p.y);
        maxY = max(maxY, p.y);
    }
    // The closest zoom level that still shows the whole route.
    int zoom = map_tiles::kMaxZoom;
    while (zoom > map_tiles::kMinZoom) {
        const double scale = ldexp(1.0, zoom - map_tiles::kMaxZoom);
        if ((maxX - minX) * scale <= kScreenW * kFitMargin && (maxY - minY) * scale <= kMapH * kFitMargin) {
            break;
        }
        --zoom;
    }
    zoom_ = min(zoom, kOverviewMaxZoom);
    const double scale = ldexp(1.0, zoom_ - map_tiles::kMaxZoom);
    centerX_ = (originX_ + (minX + maxX) / 2) * scale;
    centerY_ = (originY_ + (minY + maxY) / 2) * scale;
    step_ = -1;
}

void MapUi::focusStep(int index)
{
    if (index < 0) {
        fitRoute();
    } else {
        step_ = index;
        zoom_ = max(zoom_, kStepZoom);
        const double scale = ldexp(1.0, zoom_ - map_tiles::kMaxZoom);
        const WorldPoint &at = stepAt_[(size_t)index];
        centerX_ = (originX_ + at.x) * scale;
        centerY_ = (originY_ + at.y) * scale;
    }
    drawStepStrip();
    drawMap();
}

void MapUi::zoomBy(int delta)
{
    const int zoom = constrain(zoom_ + delta, map_tiles::kMinZoom, map_tiles::kMaxZoom);
    if (zoom == zoom_) {
        return;
    }
    const double factor = ldexp(1.0, zoom - zoom_);
    centerX_ *= factor;
    centerY_ *= factor;
    zoom_ = zoom;
    drawMap();
}

void MapUi::drawMapView()
{
    const String summary = haveRoute_ ? route_client::formatDistance(route_.distanceM) + "  " +
                                            route_client::formatDuration(route_.durationS)
                                      : String("Xem ban do");
    drawBackHeader(gfx_, "Ban do", summary, kColorText);
    drawStepStrip();
    // The form's 3 footer buttons sit under the gaps between these 5: clear them first.
    gfx_.fillRect(0, kMapBottom, kScreenW, kScreenH - kMapBottom, kColorBg);
    for (size_t i = 0; i < kMapButtons; ++i) {
        drawButton(gfx_, footerSlot(i, kMapButtons), kMapButtonLabels[i], i == 0 ? kColorButton : kColorKeySpecial,
                   kColorWhite);
    }
    drawMap();
}

void MapUi::drawStepStrip()
{
    gfx_.fillRect(0, kHeaderH, kScreenW, kStripH, kColorRow);
    String line1, line2;
    const size_t count = route_.steps.size();
    if (!haveRoute_) {
        line1 = "Keo but de di chuyen, - / + de thu phong";
        line2 = "Chi duong: bam Sua, go diem den roi bam Tim";
    } else if (step_ < 0) {
        line1 = "Tong quan: " + String(count) + " buoc - bam > de xem tung buoc";
        line2 = "A " + fromName_ + " -> B " + toName_;
    } else {
        const route_client::Step &s = route_.steps[(size_t)step_];
        line1 = String(step_ + 1) + "/" + String(count) + "  " + s.text;
        line2 = s.distanceM > 0 ? "roi di " + route_client::formatDistance(s.distanceM) : String("Ban da den noi");
    }
    gfx_.setTextWrap(false);
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorWhite);
    gfx_.setCursor(kMargin, kStripLine1Y);
    gfx_.print(ellipsize(line1, kStripChars));
    gfx_.setTextColor(step_ < 0 ? kColorMuted : kFocusColor);
    gfx_.setCursor(kMargin, kStripLine2Y);
    gfx_.print(ellipsize(line2, kStripChars));
}

// Replaces the second strip line for a moment (tile download progress).
void MapUi::drawStripNote(const String &text)
{
    gfx_.fillRect(0, kStripLine2Y - 1, kScreenW, kFontH + 2, kColorRow);
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorSystem);
    gfx_.setCursor(kMargin, kStripLine2Y);
    gfx_.print(ellipsize(text, kStripChars));
}

void MapUi::drawPolyline(size_t first, size_t last, uint16_t color, int16_t half)
{
    if (last >= path_.size() || first >= last) {
        return;
    }
    // Keep the thick copies inside the map area.
    const float xmin = half, xmax = kScreenW - 1 - half;
    const float ymin = kMapTop + half, ymax = kMapBottom - 1 - half;
    float px, py;
    toScreen(path_[first], px, py);
    for (size_t i = first + 1; i <= last; ++i) {
        float x, y;
        toScreen(path_[i], x, y);
        if (i != last && fabsf(x - px) < kMinSegmentPx && fabsf(y - py) < kMinSegmentPx) {
            continue;
        }
        float x0 = px, y0 = py, x1 = x, y1 = y;
        if (clipSegment(x0, y0, x1, y1, xmin, ymin, xmax, ymax)) {
            const int16_t ax = (int16_t)lroundf(x0), ay = (int16_t)lroundf(y0);
            const int16_t bx = (int16_t)lroundf(x1), by = (int16_t)lroundf(y1);
            for (int16_t t = -half; t <= half; ++t) {
                gfx_.drawLine(ax + t, ay, bx + t, by, color);
                gfx_.drawLine(ax, ay + t, bx, by + t, color);
            }
        }
        px = x;
        py = y;
    }
}

void MapUi::drawMarker(const WorldPoint &p, int16_t r, uint16_t color)
{
    float x, y;
    toScreen(p, x, y);
    if (x < r || x > kScreenW - 1 - r || y < kMapTop + r || y > kMapBottom - 1 - r) {
        return;
    }
    gfx_.fillCircle((int16_t)lroundf(x), (int16_t)lroundf(y), r, color);
    gfx_.drawCircle((int16_t)lroundf(x), (int16_t)lroundf(y), r, kColorWhite);
}

void MapUi::drawScaleBar()
{
    const double mpp = map_tiles::metersPerPixel(map_tiles::yToLat(centerY_, zoom_), zoom_);
    const float stepM = niceStep((float)(1.0 / mpp));
    const int16_t len = (int16_t)lround(stepM / mpp);
    const String label = route_client::formatDistance(stepM);
    // Light box so the bar reads on any map colors.
    gfx_.fillRect(kScaleBarX - 3, kScaleBarY - kScaleTick - kFontH - 4, max<int16_t>(len, label.length() * kFontW) + 7,
                  kFontH + kScaleTick + 7, kLabelBox);
    gfx_.fillRect(kScaleBarX, kScaleBarY - 1, len + 1, 2, kColorBlack);
    gfx_.drawFastVLine(kScaleBarX, kScaleBarY - kScaleTick, kScaleTick + 1, kColorBlack);
    gfx_.drawFastVLine(kScaleBarX + len, kScaleBarY - kScaleTick, kScaleTick + 1, kColorBlack);
    gfx_.setTextWrap(false);
    gfx_.setTextSize(1);
    gfx_.setTextColor(kColorBlack);
    gfx_.setCursor(kScaleBarX + 2, kScaleBarY - kScaleTick - kFontH - 1);
    gfx_.print(label);
}

// OpenStreetMap asks for this credit on every map that shows its tiles.
void MapUi::drawAttribution(bool tilesMissing)
{
    const char *text = tilesMissing ? "Thieu nen ban do" : "(c) OpenStreetMap";
    const int16_t w = (int16_t)strlen(text) * kFontW + 4;
    const int16_t x = kScreenW - w, y = kMapBottom - kFontH - 3;
    gfx_.fillRect(x, y, w, kFontH + 3, kLabelBox);
    gfx_.setTextSize(1);
    gfx_.setTextColor(tilesMissing ? kEndColor : kColorBlack);
    gfx_.setCursor(x + 2, y + 2);
    gfx_.print(text);
}

void MapUi::drawMap()
{
    const map_tiles::View view = tileView();
    const int missing = map_tiles::fetch(view, [this](const String &text) { drawStripNote(text); });
    const bool drewTiles = map_tiles::draw(view, kMapBg);
    if (missing > 0) {
        drawStepStrip();  // remove the download progress text
        if (map_tiles::lastError().length() > 0) {
            drawStripNote(map_tiles::lastError());  // why the background is missing
        }
    }
    if (haveRoute_) {
        drawRoute();
    }
    drawScaleBar();
    drawAttribution(missing > 0 || !drewTiles);
}

void MapUi::drawRoute()
{
    const size_t last = path_.size() - 1;
    drawPolyline(0, last, kCasingColor, kCasingHalf);
    drawPolyline(0, last, kRouteColor, kRouteHalf);
    if (step_ >= 0) {
        // The stretch of this step (up to the next maneuver) in orange.
        const size_t a = stepPathIndex_[(size_t)step_];
        const size_t b = (size_t)step_ + 1 < stepPathIndex_.size() ? stepPathIndex_[(size_t)step_ + 1] : last;
        drawPolyline(a, b, kColorAccent, kRouteHalf);
    }
    drawMarker(path_.front(), kEndpointR, kStartColor);
    drawMarker(path_.back(), kEndpointR, kEndColor);
    if (step_ >= 0) {
        drawMarker(stepAt_[(size_t)step_], kFocusR, kFocusColor);
    }
}

void MapUi::pressMap(int16_t x, int16_t y)
{
    if (backTapped(x, y) || footerSlot(0, kMapButtons).contains(x, y)) {
        view_ = View::Form;
        drawForm();
        return;
    }
    const int last = (int)route_.steps.size() - 1;
    if (footerSlot(1, kMapButtons).contains(x, y)) {
        if (haveRoute_ && step_ >= 0) {
            focusStep(step_ - 1);
        }
    } else if (footerSlot(2, kMapButtons).contains(x, y)) {
        if (haveRoute_ && step_ < last) {
            focusStep(step_ + 1);
        }
    } else if (footerSlot(3, kMapButtons).contains(x, y)) {
        zoomBy(-1);
    } else if (footerSlot(4, kMapButtons).contains(x, y)) {
        zoomBy(1);
    } else if (y >= kMapTop && y < kMapBottom) {
        dragging_ = true;
        dragStartX_ = dragX_ = x;
        dragStartY_ = dragY_ = y;
    }
}

MapAction MapUi::press(int16_t x, int16_t y)
{
    dragging_ = false;
    if (view_ == View::Map) {
        pressMap(x, y);
        return MapAction::None;
    }
    return pressForm(x, y);
}

void MapUi::drag(int16_t x, int16_t y)
{
    if (dragging_) {
        dragX_ = x;
        dragY_ = y;
    }
}

// The map moves when the stylus lifts (redrawing while dragging would only flicker on this SPI screen).
void MapUi::release()
{
    if (!dragging_) {
        return;
    }
    dragging_ = false;
    const int16_t dx = dragX_ - dragStartX_, dy = dragY_ - dragStartY_;
    if (abs(dx) < kPanMinPx && abs(dy) < kPanMinPx) {
        return;
    }
    centerX_ -= dx;
    centerY_ -= dy;
    drawMap();
}
