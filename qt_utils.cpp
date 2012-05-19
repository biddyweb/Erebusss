#include "qt_utils.h"
#include "game.h"

#ifdef _DEBUG
#include <cassert>
#endif

int parseInt(const QString &str, bool allow_empty) {
    if( str.length() == 0 && allow_empty ) {
        return 0;
    }
    bool ok = true;
    int value = str.toInt(&ok);
    if( !ok ) {
        LOG("failed to parse string to int: %s\n", str.toStdString().c_str());
        throw string("failed to parse string to int");
    }
    return value;
}

float parseFloat(const QString &str, bool allow_empty) {
    if( str.length() == 0 && allow_empty ) {
        return 0.0f;
    }
    bool ok = true;
    float value = str.toFloat(&ok);
    if( !ok ) {
        LOG("failed to parse string to float: %s\n", str.toStdString().c_str());
        throw string("failed to parse string to float");
    }
    return value;
}

bool parseBool(const QString &str, bool allow_empty) {
    if( str.length() == 0 && allow_empty ) {
        return false;
    }
    bool res = false;
    if( str == "true" ) {
        res = true;
    }
    else if( str == "false" ) {
        res = false;
    }
    else {
        LOG("failed to parse string to bool: %s\n", str.toStdString().c_str());
        throw string("failed to parse string to bool");
    }
    return res;
}

QPixmap createNoise(int w, int h, float scale_u, float scale_v, const unsigned char filter_max[3], const unsigned char filter_min[3], NOISEMODE_t noisemode, int n_iterations) {
    LOG("create noise(%d, %d, %f, %f, {%d, %d, %d}, {%d, %d, %d}, %d, %d\n", w, h, scale_u, scale_v, filter_max[0], filter_max[1], filter_max[2], filter_min[0], filter_min[1], filter_min[2], noisemode, n_iterations);
    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(0); // needed to initialise image!

    ASSERT_LOGGER( !image.isNull() );

    float fvec[2] = {0.0f, 0.0f};
    for(int y=0;y<h;y++) {
        fvec[0] = scale_v * ((float)y) / ((float)h - 1.0f);
        for(int x=0;x<w;x++) {
            fvec[1] = scale_u * ((float)x) / ((float)w - 1.0f);
            float h = 0.0f;
            float max_val = 0.0f;
            float mult = 1.0f;
            for(int j=0;j<n_iterations;j++,mult*=2.0f) {
                float this_fvec[2];
                this_fvec[0] = fvec[0] * mult;
                this_fvec[1] = fvec[1] * mult;
                float this_h = perlin_noise2(this_fvec) / mult;
                if( noisemode == NOISEMODE_PATCHY || noisemode == NOISEMODE_MARBLE )
                    this_h = abs(this_h);
                h += this_h;
                max_val += 1.0f / mult;
            }
            if( noisemode == NOISEMODE_PATCHY ) {
                h /= max_val;
            }
            else if( noisemode == NOISEMODE_MARBLE ) {
                h = sin(scale_u * ((float)x) / ((float)w - 1.0f) + h);
                h = 0.5f + 0.5f * h;
            }
            else {
                h /= max_val;
                h = 0.5f + 0.5f * h;
            }

            if( noisemode == NOISEMODE_CLOUDS ) {
                //const float offset = 0.4f;
                //const float offset = 0.3f;
                const float offset = 0.2f;
                h = offset - h * h;
                h = std::max(h, 0.0f);
                h /= offset;
            }
            // h is now in range [0, 1]
            if( h < 0.0 || h > 1.0 ) {
                LOG("h value is out of bounds\n");
                ASSERT_LOGGER(false);
            }
            if( noisemode == NOISEMODE_WOOD ) {
                h = 20 * h;
                h = h - floor(h);
            }
            int r = (int)((filter_max[0] - filter_min[0]) * h + filter_min[0]);
            int g = (int)((filter_max[1] - filter_min[1]) * h + filter_min[1]);
            int b = (int)((filter_max[2] - filter_min[2]) * h + filter_min[2]);
            int a = 255;
            image.setPixel(x, y, qRgba(r, g, b, a));
        }
    }
    QPixmap pixmap = QPixmap::fromImage(image, Qt::OrderedAlphaDither);
    ASSERT_LOGGER( !pixmap.isNull() );
    return pixmap;
}
