#pragma once

enum WidgetAlign { WA_LEFT, WA_CENTER, WA_RIGHT };
enum BitrateFormat { BF_UNKNOWN, BF_MP3, BF_AAC, BF_FLAC, BF_OGG, BF_WAV, BF_VOR, BF_OPU }; // Módisítás "bitrate" BF_VOR, BF_OPU

struct WidgetConfig {
    uint16_t    left;
    uint16_t    top;
    uint16_t    textsize;
    WidgetAlign align;
};

struct ScrollConfig {
    WidgetConfig widget;
    uint16_t     buffsize;
    bool         uppercase;
    uint16_t     width;
    uint16_t     startscrolldelay;
    uint8_t      scrolldelta;
    uint16_t     scrolltime;
};

struct FillConfig {
    WidgetConfig widget;
    uint16_t     width;
    uint16_t     height;
    bool         outlined;
};

struct ProgressConfig {
    uint16_t speed;
    uint16_t width;
    uint16_t barwidth;
};

struct VU_WidgetConfig {
    uint16_t left;
    uint16_t top;
    uint16_t textsize;
    uint16_t width;
    uint16_t height;
    uint8_t  space;
    uint8_t  vspace;
    uint8_t  perheight;
    uint8_t  fadespeed;
    uint8_t  labelwidth;
    uint8_t  labelheight;
};

struct MoveConfig {
    uint16_t x;
    uint16_t y;
    int16_t  width;
};

struct BitrateBoxConfig {
    uint16_t    left;
    uint16_t    top;
    uint8_t     textsize;
    WidgetAlign align;
    uint8_t     border;
    uint8_t     radius;
    bool        fill;
    uint8_t     paddingX;
    uint8_t     paddingY;
    uint16_t    dimension; // square esetén a négyzet oldalhossza, flat esetén a téglalap rövidebbik oldalának hossza
};

struct VolumeWidgetConfig {
    uint16_t left, top, width, height;

    uint8_t segments;
    uint8_t segWidth;
    uint8_t segGap;
    uint8_t segHeight;
    uint8_t iconW;
    uint8_t iconH;
    uint8_t radius;
    uint8_t border;
};

struct WifiWidgetConfig {
    uint16_t    left;
    uint16_t    top;
    uint16_t    width;
    uint16_t    height;
    const char* image1;
    const char* image2;
    const char* image3;
    const char* image4;
};

struct ImageWidgetConfig {
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
};

struct NamedayWidgetConfig {
    uint16_t    left;
    uint16_t    top;
    uint16_t    width;
    uint16_t    height;
    uint16_t    textsize1;
    uint16_t    textsize2;
    WidgetAlign align;
};
