// "Ban do" (directions): type where you start and where you go, pick motorbike/car, bicycle or walking, and
// the board draws the route on an OpenStreetMap background (map_tiles) with turn-by-turn steps. The CYD has
// no GPS, so the start is typed too (MAP_DEFAULT_FROM in config.h). Drag the map to move it, "-"/"+" to
// zoom, "<"/">" for the steps.
#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <vector>

#include "map_tiles.h"
#include "route_client.h"
#include "ui_common.h"

enum class MapAction : uint8_t { None, Exit, EditFrom, EditTo };

class MapUi {
public:
    explicit MapUi(Adafruit_GFX &gfx);

    // Draws the current page: the address form, or the map once a route was found.
    void show();
    bool visible() const { return visible_; }
    void hide() { visible_ = false; }

    const String &from() const { return from_; }
    const String &to() const { return to_; }
    void setFrom(const String &text);
    void setTo(const String &text);

    // Searching happens inside press() and blocks for a few seconds (splash screen meanwhile).
    MapAction press(int16_t x, int16_t y);
    void drag(int16_t x, int16_t y);
    void release();

private:
    enum class View : uint8_t { Form, Map };
    static constexpr size_t kModeCount = 3;
    static constexpr size_t kFormButtons = 3;
    static constexpr size_t kMapButtons = 5;

    void loadSaved();
    void saveInputs();
    void setStatus(const String &text, bool isError);
    void forgetRoute();

    void drawForm();
    void drawField(const ui::Rect &box, const char *label, const String &text, const char *placeholder);
    void drawModes();
    void drawFormStatus();
    void drawFormFooter();
    MapAction pressForm(int16_t x, int16_t y);
    void search();

    // A route point in world pixels at map_tiles::kMaxZoom, relative to origin (floats stay precise).
    struct WorldPoint {
        float x, y;
    };

    void drawMapView();
    void drawStepStrip();
    void drawStripNote(const String &text);
    void drawMap();
    void drawRoute();
    void drawPolyline(size_t first, size_t last, uint16_t color, int16_t half);
    void drawMarker(const WorldPoint &p, int16_t r, uint16_t color);
    void drawScaleBar();
    void drawAttribution(bool tilesMissing);
    void pressMap(int16_t x, int16_t y);
    void buildWorldPath();
    void centerOnHome();
    void fitRoute();
    void focusStep(int index);
    void zoomBy(int delta);
    void indexSteps();
    void toScreen(const WorldPoint &p, float &x, float &y) const;
    map_tiles::View tileView() const;

    Adafruit_GFX &gfx_;
    View view_ = View::Form;
    bool visible_ = false;
    bool loaded_ = false;
    String from_;
    String to_;
    route_client::Mode mode_ = route_client::Mode::Car;
    String status_;
    bool statusIsError_ = false;

    route_client::Route route_;  // distance, duration and step texts (the path moves to path_)
    bool haveRoute_ = false;
    String fromName_;
    String toName_;
    std::vector<WorldPoint> path_;
    std::vector<WorldPoint> stepAt_;     // where each step's maneuver is
    std::vector<size_t> stepPathIndex_;  // path point closest to each step
    int step_ = -1;                      // -1 = whole route

    // View (Web Mercator, like the tiles): zoom level and map center in world pixels at that zoom.
    double originX_ = 0, originY_ = 0;  // world pixels at kMaxZoom of the route's first point
    int zoom_ = map_tiles::kMinZoom;
    double centerX_ = 0, centerY_ = 0;

    bool dragging_ = false;
    int16_t dragStartX_ = 0, dragStartY_ = 0, dragX_ = 0, dragY_ = 0;

    ui::Rect fromBox_;
    ui::Rect toBox_;
    ui::Rect modeButtons_[kModeCount];
};
