#pragma once

#include <string>
using std::string;

#include <QString>
#include <QPixmap>

#include "common.h"

int parseInt(const QString &str, bool allow_empty = false);
float parseFloat(const QString &str, bool allow_empty = false);
bool parseBool(const QString &str, bool allow_empty = false);

enum NOISEMODE_t {
    NOISEMODE_PERLIN = 0,
    NOISEMODE_SMOKE = 1,
    NOISEMODE_PATCHY = 2,
    NOISEMODE_MARBLE = 3,
    NOISEMODE_WOOD = 4,
    NOISEMODE_CLOUDS = 5
};
QPixmap createNoise(int w, int h, float scale_u, float scale_v, const unsigned char filter_max[3], const unsigned char filter_min[3], NOISEMODE_t noisemode, int n_iterations);

//void convertToHTML(QString &string);
string convertToHTML(const string &str);
bool stringAnyNonWhitespace(const string &str);
