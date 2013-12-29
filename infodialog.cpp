#include <cassert>

#include <sstream>
using std::stringstream;

#include <QApplication>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDesktopWidget>

#ifdef USING_WEBKIT
#include <QWebView>
#include <QWebFrame>
#else
#include <QTextEdit>
#include <QScrollBar>
#endif

#include "infodialog.h"
#include "game.h"
#include "logiface.h"

InfoDialog::InfoDialog(const string &text, const string &picture, const vector<string> &buttons, bool horiz, bool small_buttons, bool numbered_shortcuts) {
    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    if( picture.length() > 0 ) {
        /*QPicture pic;
        if( !pic.load(picture.c_str()) ) {
            throw string("failed to load image");
        }*/
        try {
            QPixmap pixmap = game_g->loadImage(picture);
            QLabel *picture_label = new QLabel();
            //picture_label->setPicture(pic);
            int height = QApplication::desktop()->availableGeometry().height();
            int max_pic_height = height/3;
            if( pixmap.height() > max_pic_height ) {
                pixmap = pixmap.scaledToHeight(max_pic_height, Qt::SmoothTransformation);
            }
            picture_label->setPixmap(pixmap);
            layout->addWidget(picture_label);
            layout->setAlignment(picture_label, Qt::AlignCenter);
        }
        catch(const string &str) {
            LOG("failed to load: %s\n", picture.c_str());
            LOG("error: %s\n", str.c_str());
        }
    }

    //QLabel *label = new QLabel(text.c_str());
    //label->setWordWrap(true);
#ifdef USING_WEBKIT
    label = new QWebView();
    game_g->setWebView(label);
#else
    label = new QTextEdit();
    game_g->setTextEdit(label);
#endif
    label->setHtml(text.c_str());
    /*string text2 = text;
    //text2 += "<font color=\"red\">blah</font> <font color=\"black\">blah</font> blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah ";
    text2 += "blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah ";
    text2 += "blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah ";
    text2 += "blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah ";
    label->setHtml(text2.c_str());*/

    label->setFont(game_g->getFontSmall());
    //label->setReadOnly(true);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(label);

    {
        QLayout *layout2 = NULL;
        if( horiz ) {
            layout2 = new QHBoxLayout();
            layout->addLayout(layout2);
        }
        else {
            layout2 = layout;
        }

        int index = 0;
        for(vector<string>::const_iterator iter = buttons.begin(); iter != buttons.end(); ++iter, index++) {
            string button_text = *iter;
            if( numbered_shortcuts && !smallscreen_c && index < buttons.size()-1 ) {
                stringstream str;
                str << (index+1) << ": " << button_text;
                button_text = str.str();
            }
            QPushButton *button = new QPushButton(button_text.c_str());
            game_g->initButton(button);
            if( numbered_shortcuts && !smallscreen_c ) {
                if( index < buttons.size()-1 ) {
                    button->setShortcut(QKeySequence(QString::number(index+1)));
                }
                else {
                    button->setShortcut(QKeySequence(Qt::Key_Return));
                }
            }
            else if( index == 0 ) {
                button->setShortcut(QKeySequence(Qt::Key_Return));
            }
            else if( index == buttons.size()-1 ) {
                button->setShortcut(QKeySequence(Qt::Key_Escape));
            }
            button->setFont(small_buttons ? game_g->getFontSmall() : game_g->getFontStd());
            //button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            layout2->addWidget(button);
            connect(button, SIGNAL(clicked()), this, SLOT(clicked()));
            buttons_list.push_back(button);
        }
    }

    //this->setEnabled(true);
}

InfoDialog *InfoDialog::createInfoDialogOkay(const string &text, const string &picture) {
    vector<string> buttons;
    buttons.push_back("Okay");
    return new InfoDialog(text, picture, buttons, true, false, false);
}

InfoDialog *InfoDialog::createInfoDialogOkay(const string &text) {
    return createInfoDialogOkay(text, "");
}

InfoDialog *InfoDialog::createInfoDialogYesNo(const string &text) {
    vector<string> buttons;
    buttons.push_back("Yes");
    buttons.push_back("No");
    return new InfoDialog(text, "", buttons, true, false, false);
}

void InfoDialog::reject() {
    // called when pressing Alt+F4 on Windows, Android back button, etc
    if( this->buttons_list.size() > 0 ) {
        QPushButton *button = this->buttons_list.at( this->buttons_list.size() - 1 );
        button->click();
    }
}

void InfoDialog::clicked() {
    int result = -1;
    QPushButton *button_sender = static_cast<QPushButton *>(this->sender());
    for(int i=0;i<buttons_list.size();i++) {
        const QPushButton *button = buttons_list[i];
        if( button_sender == button ) {
            result = i;
            break;
        }
    }
    ASSERT_LOGGER(result >= 0);
    if( result == -1 ) {
        result = 0;
    }
    this->done(result);
}

void InfoDialog::scrollToBottom() {
#ifdef USING_WEBKIT
    this->label->page()->mainFrame()->setScrollBarValue(Qt::Vertical, this->label->page()->mainFrame()->scrollBarMaximum(Qt::Vertical));
#else
    this->label->verticalScrollBar()->setValue( this->label->verticalScrollBar()->maximum() );
#endif
}

int InfoDialog::exec() {
    if( game_g->isTesting() ) {
        game_g->recordTestInfoDialog();
        return 0;
    }
    game_g->setPaused(true, true);
    int result = QDialog::exec();
    game_g->setPaused(false, true);
    return result;
}
