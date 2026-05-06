#include "../../core/options.h"
#if DSP_MODEL != DSP_DUMMY
#    include "../../core/config.h"
#    include "../display_select.h"
#    include "widgets.h"
#    include "../../core/fonts.h"

// Konstruktor
PlayListWidget::PlayListWidget() : Widget() {
    _cacheBaseIdx = -1;
    _lastIndex = 0xFFFF;
}

// Destruktor
PlayListWidget::~PlayListWidget() {}

void PlayListWidget::_loadDspFont() {
    dsp.unloadFont();
    uint8_t sz = playlistConf.widget.textsize;
    if (sz == 20 && font_vlw_20)
        dsp.loadFont(font_vlw_20);
    else if (sz == 22 && font_vlw_22)
        dsp.loadFont(font_vlw_22);
    else if (sz == 24 && font_vlw_24)
        dsp.loadFont(font_vlw_24);
    else if (sz == 26 && font_vlw_26)
        dsp.loadFont(font_vlw_26);
    else {
        dsp.setFont(nullptr);
        dsp.setTextSize(2);
    }
}

void PlayListWidget::_prepareMovingModeLayout() {
    // 1. Font betöltése a méréshez
    _loadDspFont();

    // 2. Valós font magasság mérése
    if (dsp.getFont()) {
        _plItemHeight = dsp.fontHeight() + 4;
    } else {
        _plItemHeight = playlistConf.widget.textsize + 4;
    }
    dsp.unloadFont();

    // 3. Hány elem fér el a kijelzőn
    _plTtemsCount = dsp.height() / _plItemHeight;
    if (_plTtemsCount < 1) _plTtemsCount = 1;
    if (_plTtemsCount > MAX_PL_PAGE_ITEMS) _plTtemsCount = MAX_PL_PAGE_ITEMS;

    // 4. Vertikális középre igazítás
    uint16_t contentHeight = _plTtemsCount * _plItemHeight;
    _plYStart = (dsp.height() - contentHeight) / 2;

    // 5. Kényszerített újrarajzolás a következő híváskor
    _plLoadedPage = -1;
    _plLastGlobalPos = -1;
    _lastIndex = 0xFFFF;

    Serial.printf("PlayListWidget init (MOVING_CURSOR): dspHeight=%ld, itemHeight=%d, itemsPerPage=%d, yStart=%d\n", dsp.height(), _plItemHeight, _plTtemsCount, _plYStart);
}

void PlayListWidget::_prepareFixedModeLayout() {
    // A fix mód ugyanazzal a tényleges fontmagassággal számoljon, mint a moving mód.
    int16_t fHeight = 0;
    if (_currentScroll) {
        _currentScroll->setFontBySize(playlistConf.widget.textsize);
        _lastScrollFontSize = playlistConf.widget.textsize;
        fHeight = (int16_t)_currentScroll->textHeight();
    }
    if (fHeight <= 0) {
        _loadDspFont();
        fHeight = dsp.getFont() ? (int16_t)dsp.fontHeight() : (int16_t)playlistConf.widget.textsize;
        dsp.unloadFont();
    }

    _plItemHeight = fHeight + 4;

    uint8_t totalRows = dsp.height() / _plItemHeight;
    if (totalRows < 1) totalRows = 1;
    if (totalRows > MAX_PL_PAGE_ITEMS) totalRows = MAX_PL_PAGE_ITEMS;
    if (totalRows % 2 == 0 && totalRows > 1) totalRows--;

    _plTtemsCount = totalRows;
    _sideRows = totalRows / 2;

    _lastIndex = 0xFFFF; // Force initial draw
}

void PlayListWidget::init(ScrollWidget* current) {
    Widget::init({0, 0, 0, WA_LEFT}, 0, 0);
    _currentScroll = current;

    _lastMovingMode = config.store.playlistMovingCursor;

    if (_lastMovingMode) {
        _prepareMovingModeLayout();
    } else {
        _prepareFixedModeLayout();
    }
}

void PlayListWidget::_loadPlaylistPage(int pageIndex, int itemsPerPage, int totalItems) {
    for (int i = 0; i < MAX_PL_PAGE_ITEMS; i++) _plCache[i] = "";

    int playlistLen = config.playlistLength();
    if (playlistLen == 0) return;

    File playlist = config.SDPLFS()->open(REAL_PLAYL, "r");
    File index = config.SDPLFS()->open(REAL_INDEX, "r");

    if (!playlist || !index) return;

    int startIdx = pageIndex * itemsPerPage;

    for (int i = 0; i < itemsPerPage; i++) {
        int currentGlobalIdx = startIdx + i;
        if (currentGlobalIdx >= playlistLen) break;

        index.seek(currentGlobalIdx * 4, SeekSet);
        uint32_t posAddr;
        if (index.readBytes((char*)&posAddr, 4) != 4) break;

        playlist.seek(posAddr, SeekSet);
        String line = playlist.readStringUntil('\n');
        // Serial.printf("LINE FROM INDEX: '%s'\n", line.c_str());

        int tabIdx = line.indexOf('\t');
        if (tabIdx > 0) line = line.substring(0, tabIdx);
        line.trim();
        //  Serial.printf("PROCESSED LINE: '%s'\n", line.c_str());
        //   Serial.printf("CONFIG NUMPLAYLIST: %d\n", config.store.numplaylist);
        //  Serial.printf("CURRENT GLOBAL IDX: %d\n", currentGlobalIdx);
        //  Serial.printf("LINE LENGTH: %d\n", line.length());
        if (config.store.numplaylist && line.length() > 0) {
            _plCache[i] = String(currentGlobalIdx + 1) + " " + line;
            //    Serial.printf("CACHED LINE WITH NUMBER: '%s'\n", _plCache[i].c_str());
        } else {
            _plCache[i] = line;
            //    Serial.printf("CACHED LINE WITHOUT NUMBER: '%s'\n", _plCache[i].c_str());
        }
    }
    playlist.close();
    index.close();
}

void PlayListWidget::drawPlaylist(uint16_t currentItem) {
    bool movingMode = config.store.playlistMovingCursor;

    if (movingMode != _lastMovingMode) {
        _lastMovingMode = movingMode;
        if (_lastMovingMode)
            _prepareMovingModeLayout();
        else
            _prepareFixedModeLayout();
        _lastIndex = 0xFFFF;
        _plLoadedPage = -1;
        _plLastGlobalPos = -1;
        _cacheBaseIdx = -1;
        if (_currentScroll) _currentScroll->setActive(false, true);
    }

    if (movingMode) {
        if (_plTtemsCount == 0) return; // init() még nem futott le

        // Ha 2000ms-nél több telt el, feltételezzük, hogy más képernyőről tértünk vissza
        bool isLongPause = (millis() - _plLastDrawTime > 2000);
        _plLastDrawTime = millis();

        // 3. Pozíció kiszámítása
        int activeIdx = (currentItem > 0) ? (int)(currentItem - 1) : 0;
        int itemsPerPage = _plTtemsCount;
        int newPage = activeIdx / itemsPerPage;
        int newLocalPos = activeIdx % itemsPerPage;
        _plCurrentPos = (uint16_t)newLocalPos;

        bool pageChanged = (newPage != _plLoadedPage);

        // A kurzorsor Y pozíciója – ide kerül a ScrollWidget teteje
        int16_t cursorY = _plYStart + newLocalPos * _plItemHeight;

        if (pageChanged || isLongPause || _plCache[0].length() == 0) {
            // --- TELJES ÚJRARAJZOLÁS ---
            int playlistLen = config.playlistLength();

            // Betöltjük az új oldalt, ha szükséges
            if (pageChanged || _plCache[0].length() == 0) {
                _loadPlaylistPage(newPage, itemsPerPage, playlistLen);
                _plLoadedPage = newPage;
            }

            // ScrollWidget deaktiválása: külön clear nélkül, hogy ne villanjon a saját háttérszíne.
            if (_currentScroll) _currentScroll->setActive(false, false);

            // Teljes listaterület törlése
            dsp.fillRect(0, _plYStart, dsp.width(), itemsPerPage * _plItemHeight, config.theme.background);

            // ScrollWidget áthelyezése és aktiválása a kurzorsorba először,
            // így a felhasználó azonnal látja az aktív elemet.
            if (_currentScroll) {
                if (_lastScrollFontSize != playlistConf.widget.textsize) {
                    _currentScroll->setFontBySize(playlistConf.widget.textsize);
                    _lastScrollFontSize = playlistConf.widget.textsize;
                }
                int16_t textY = cursorY + (int16_t)(_plItemHeight - _currentScroll->textHeight()) / 2;
                if (textY < 0) textY = 0;
                dsp.fillRect(0, cursorY, dsp.width(), _plItemHeight, config.theme.plcurrentfill);
                _currentScroll->setText(_plCache[newLocalPos].c_str());
                _currentScroll->moveTo({TFT_FRAMEWDT, (uint16_t)textY, (int16_t)playlistConf.width});
                _currentScroll->setActive(true, false);
            }

            // Minden sor kirajzolása, a kurzorsor kivételével
            _loadDspFont();
            for (int i = 0; i < itemsPerPage; i++) {
                if (i != newLocalPos) { _printPLitem(i, _plCache[i].c_str(), true); }
                if (i % 2 == 0) yield();
            }
            dsp.unloadFont();
        } else {
            // --- SMART REDRAW: csak a megváltozott sorok frissítése ---
            int oldLocalPos = (_plLastGlobalPos > 0 ? (int)(_plLastGlobalPos - 1) : 0) % itemsPerPage;

            if (oldLocalPos != newLocalPos) {
                // Régi kurzorsor deaktiválása és sima szövegként újrarajzolása
                if (_currentScroll) _currentScroll->setActive(false, false);
                if (oldLocalPos >= 0 && oldLocalPos < itemsPerPage) { _printPLitem((uint8_t)oldLocalPos, _plCache[oldLocalPos].c_str()); }

                // ScrollWidget áthelyezése az új kurzorsorba
                if (_currentScroll) {
                    if (_lastScrollFontSize != playlistConf.widget.textsize) {
                        _currentScroll->setFontBySize(playlistConf.widget.textsize);
                        _lastScrollFontSize = playlistConf.widget.textsize;
                    }
                    int16_t textY = cursorY + (int16_t)(_plItemHeight - _currentScroll->textHeight()) / 2;
                    if (textY < 0) textY = 0;
                    dsp.fillRect(0, cursorY, dsp.width(), _plItemHeight, config.theme.plcurrentfill);
                    _currentScroll->setText(_plCache[newLocalPos].c_str());
                    _currentScroll->moveTo({TFT_FRAMEWDT, (uint16_t)textY, (int16_t)playlistConf.width});
                    _currentScroll->setActive(true, false);
                }
            }
        }

        _plLastGlobalPos = currentItem;
    } else {
        if (_plItemHeight == 0) return; // init() még nem futott le

        if (currentItem == _lastIndex) return;
        _lastIndex = currentItem;

        // 1. Cache frissítése, ha szükséges
        _updateNameCache(currentItem);

        // 2. Középső elem frissítése
        if (_currentScroll) {
            if (_lastScrollFontSize != playlistConf.widget.textsize) {
                _currentScroll->setFontBySize(playlistConf.widget.textsize);
                _lastScrollFontSize = playlistConf.widget.textsize;
            }
            dsp.fillRect(0, currentTop(), dsp.width(), _plItemHeight, config.theme.plcurrentfill);
            int16_t textY = (int16_t)currentTop() + (int16_t)(_plItemHeight - _currentScroll->textHeight()) / 2;
            if (textY < 0) textY = 0;
            _currentScroll->moveTo({TFT_FRAMEWDT, (uint16_t)textY, (int16_t)playlistConf.width});
            _currentScroll->setText(_getCachedName(currentItem).c_str());
            _currentScroll->setActive(true, false);
        }

        // 3. Többi sor rajzolása
        _renderStaticList(currentItem);
    }

    dsp.unloadFont();
    dsp.setFont(nullptr);
    dsp.setTextSize(1);
}

void PlayListWidget::_printPLitem(uint8_t pos, const char* item) {
    _printPLitem(pos, item, false);
}

void PlayListWidget::_printPLitem(uint8_t pos, const char* item, bool usePreloadedFont) {
    if (pos >= _plTtemsCount) return;
    int16_t  yPos = _plYStart + (pos * _plItemHeight);
    uint16_t fgColor = config.theme.playlist[0];
    uint16_t bgColor = config.theme.background;

    // Font beállítása – önálló, nem függ a hívó kontextusától
    if (!usePreloadedFont) _loadDspFont();
    dsp.setTextDatum(top_left);

    dsp.fillRect(0, yPos, dsp.width(), _plItemHeight, bgColor);
    if (item && item[0] != '\0') {
        dsp.setTextColor(fgColor, bgColor);
        dsp.setCursor(TFT_FRAMEWDT, yPos + 4);
        dsp.print(item);
    }
    if (!usePreloadedFont) dsp.unloadFont();
}

void PlayListWidget::_updateNameCache(int centerIdx) {
    // Csak akkor frissítünk, ha elmozdultunk a korábbi cache közepétől
    if (_cacheBaseIdx != -1 && abs(centerIdx - _cacheBaseIdx) < 4) return;

    _cacheBaseIdx = centerIdx;

    auto fs = config.SDPLFS();
    if (!fs) return;

    // EGYSZER nyitjuk meg a fájlokat a teljes ciklus előtt!
    File playlist = fs->open(REAL_PLAYL, "r");
    File idxFile = fs->open(REAL_INDEX, "r");

    if (playlist && idxFile) {
        int startIdx = centerIdx - 7;
        int playlistLen = config.playlistLength();
        for (int i = 0; i < 15; i++) {
            int targetIdx = startIdx + i;
            if (targetIdx < 1 || targetIdx > playlistLen) {
                _nameCache[i] = "";
                continue;
            }

            uint32_t pos;
            idxFile.seek((targetIdx - 1) * 4);
            idxFile.read((uint8_t*)&pos, 4);
            playlist.seek(pos);

            String name = playlist.readStringUntil('\n');
            name.trim();
            int tabIdx = name.indexOf('\t');
            if (tabIdx != -1) name = name.substring(0, tabIdx);

            if (config.store.numplaylist) name = String(targetIdx) + ". " + name;
            _nameCache[i] = name;
        }
    }

    if (idxFile) idxFile.close();
    if (playlist) playlist.close();
}

String PlayListWidget::_getCachedName(int index) {
    int cachePos = index - (_cacheBaseIdx - 7);
    if (cachePos >= 0 && cachePos < 15) { return _nameCache[cachePos]; }
    return _fetchStationName(index); // Biztonsági tartalék
}

void PlayListWidget::_renderStaticList(uint16_t centerIdx) {
    int32_t centralY = currentTop();
    dsp.startWrite();

    // Háttér takarítása
    dsp.fillRect(0, 0, dsp.width(), centralY, config.theme.background);
    uint32_t lowerY = centralY + _plItemHeight;
    dsp.fillRect(0, lowerY, dsp.width(), dsp.height() - lowerY, config.theme.background);

    _loadDspFont();

    //  dsp.setTextColor(config.theme.playlist[1], config.theme.background);
    dsp.setTextDatum(middle_left);
    dsp.setTextSize(1);

    // Felső sorok rajzolása a CACHE-ből
    for (int i = 1; i <= _sideRows; i++) {
        int idx = centerIdx - i;
        int y = centralY - (i * _plItemHeight) + (_plItemHeight / 2);
        int colorIdx = i;
        if (colorIdx >= 5) colorIdx = 4; // Max szín index
        dsp.setTextColor(config.theme.playlist[colorIdx], config.theme.background);
        if (y > 0) dsp.drawString(_getCachedName(idx), 10, y);
    }

    // Alsó sorok rajzolása a CACHE-ből
    for (int i = 1; i <= _sideRows; i++) {
        int idx = centerIdx + i;
        int y = centralY + (i * _plItemHeight) + (_plItemHeight / 2);
        int colorIdx = i;
        if (colorIdx >= 5) colorIdx = 4; // Max szín index
        dsp.setTextColor(config.theme.playlist[colorIdx], config.theme.background);
        if (y < dsp.height()) dsp.drawString(_getCachedName(idx), 10, y);
    }

    dsp.unloadFont();
    dsp.endWrite();
}

String PlayListWidget::_fetchStationName(int index) {
    int playlistLen = config.playlistLength();
    if (index < 1 || index > playlistLen) return "";

    auto fs = config.SDPLFS();
    if (!fs) return "";

    String name = "";
    File   playlist = fs->open(REAL_PLAYL, "r");
    File   idxFile = fs->open(REAL_INDEX, "r");

    if (playlist && idxFile) {
        uint32_t pos;
        idxFile.seek((index - 1) * 4);
        idxFile.read((uint8_t*)&pos, 4);
        playlist.seek(pos);
        name = playlist.readStringUntil('\n');
        name.trim();
        int tabIdx = name.indexOf('\t');
        if (tabIdx != -1) name = name.substring(0, tabIdx);
        if (config.store.numplaylist) name = String(index) + ". " + name;
    }

    if (idxFile) idxFile.close();
    if (playlist) playlist.close();
    return name;
}
#endif