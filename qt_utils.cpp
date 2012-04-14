#include "qt_utils.h"
#include "game.h"

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
