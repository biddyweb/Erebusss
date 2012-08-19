#include "infodialog.h"
#include "game.h"
#include "qt_screen.h"
#include "logiface.h"

InfoDialog::InfoDialog(const string &text, const vector<string> &buttons, bool horiz, bool small_buttons) {
    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    //QLabel *label = new QLabel(text.c_str());
    //label->setWordWrap(true);
    label = new QTextEdit(text.c_str());
    label->setFont(game_g->getFontSmall());
    label->setReadOnly(true);
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

        for(vector<string>::const_iterator iter = buttons.begin(); iter != buttons.end(); ++iter) {
            const string button_text = *iter;
            QPushButton *button = new QPushButton(button_text.c_str());
            game_g->initButton(button);
            button->setFont(small_buttons ? game_g->getFontSmall() : game_g->getFontStd());
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            layout2->addWidget(button);
            connect(button, SIGNAL(clicked()), this, SLOT(clicked()));
            buttons_list.push_back(button);
        }
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
    this->label->verticalScrollBar()->setValue( this->label->verticalScrollBar()->maximum() );
}

int InfoDialog::exec() {
    game_g->getScreen()->setPaused(true);
    int result = QDialog::exec();
    game_g->getScreen()->setPaused(false);
    return result;
}
