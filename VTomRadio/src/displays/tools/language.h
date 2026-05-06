#include "../../core/options.h"
#pragma once
namespace LANG{
//==================================================
#if LANGUAGE == RU
#define LANG_PATH "../../../locale/language_ru.h"
#elif LANGUAGE == EN
#define LANG_PATH "../../../locale/language_en.h"
#elif LANGUAGE == HU
#define LANG_PATH "../../../locale/language_hu.h"
#elif LANGUAGE == PL
#define LANG_PATH "../../../locale/language_pl.h"
#elif LANGUAGE == NL
#define LANG_PATH "../../../locale/language_nl.h"
#elif LANGUAGE == GR
#define LANG_PATH "../../../locale/language_gr.h"
#elif LANGUAGE == SK
#define LANG_PATH "../../../locale/language_sk.h"
#elif LANGUAGE == UA
#define LANG_PATH "../../../locale/language_ua.h"
#elif LANGUAGE == DE
#define LANG_PATH "../../../locale/language_de.h"
#elif LANGUAGE == ES
#define LANG_PATH "../../../locale/language_es.h"
#endif

#include LANG_PATH
//==================================================
}
