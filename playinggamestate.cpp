#include <QApplication>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextStream>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QDesktopWidget>
#include <QTextEdit>
#include <QUrl>

#include <qmath.h> // for M_PI

/*#include <QtOpenGL>

#undef min
#undef max*/

/*#include <QGLWidget>

#undef min
#undef max*/

#if QT_VERSION < 0x050000
#include <QFile>
#include <qmath.h>
#endif

#include <ctime>
#include <algorithm> // needed for stable_sort on Symbian at least

#ifdef _DEBUG
#include <cassert>
#endif

#include "rpg/locationgenerator.h"

#include "playinggamestate.h"
#include "game.h"
#include "animatedobject.h"
#include "particlesystem.h"
#include "infodialog.h"
#include "mainwindow.h"
#include "qt_utils.h"
#include "logiface.h"

#ifdef _DEBUG
#define DEBUG_SHOW_PATH
#endif

const float z_value_tilemap = E_TOL_LINEAR;
const float z_value_scenery_background = 2.0f*E_TOL_LINEAR;
const float z_value_items = 3.0f*E_TOL_LINEAR; // so items appear above DRAWTYPE_BACKGROUND Scenery
const float z_value_gui = 4.0f*E_TOL_LINEAR; // so items appear above DRAWTYPE_BACKGROUND Scenery

//const string music_key_ingame_c = "ingame_music";
const string music_key_combat_c = "combat_music";
const string music_key_trade_c = "trade";
const string music_key_game_over_c = "game_over";

PlayingGamestate *PlayingGamestate::playingGamestate = NULL;

Direction directionFromVecDir(Vector2D dir) {
    if( dir.magnitude() < E_TOL_LINEAR ) {
        return DIRECTION_E; // arbitrary
    }
    dir.normalise();
    float angle = atan2(dir.y, dir.x);
    if( angle < 0.0f )
        angle += (float)(2.0*M_PI);
    angle /= (float)(2.0*M_PI);
    float turn = angle*((int)N_DIRECTIONS) + 0.5f;
    int turn_i = (int)turn;
    /*qDebug("angle %f", angle);
    qDebug("turn %f", turn);
    qDebug("turn_i %d", turn_i);*/
    turn_i += 4; // 0 is west
    Direction direction = (Direction)(turn_i % (int)N_DIRECTIONS);
    return direction;
}

CharacterAction::CharacterAction(Type type, Character *source, Character *target_npc, float offset_y) : type(type), source(source), target_npc(target_npc), time_ms(0), duration_ms(0), offset_y(offset_y), hits(false), weapon_no_effect_magical(false), weapon_no_effect_holy(false), weapon_damage(0), spell(NULL), object(NULL) {
    this->source_pos = source->getPos();
    this->dest_pos = target_npc->getPos();
    this->time_ms = game_g->getGameTimeTotalMS();
}

CharacterAction::~CharacterAction() {
    if( this->object != NULL ) {
        delete this->object;
    }
}

void CharacterAction::implement(PlayingGamestate *playing_gamestate) const {
    qDebug("CharacterAction::implement()");
    if( target_npc == NULL ) {
        // target no longer exists
        qDebug("target no longer exists");
        return;
    }
    if( type == CHARACTERACTION_RANGED_WEAPON ) {
        if( hits ) {
            playing_gamestate->hitEnemy(source, target_npc, true, weapon_no_effect_magical, weapon_no_effect_holy, weapon_damage);
        }
    }
    else if( type == CHARACTERACTION_SPELL ) {
        ASSERT_LOGGER( spell != NULL );
        spell->castOn(playing_gamestate, source, target_npc);
    }
    qDebug("CharacterAction::implement() done");
}

void CharacterAction::update() {
    if( this->target_npc != NULL ) {
        // update destination
        this->dest_pos = this->target_npc->getPos();
    }
    if( this->object != NULL ) {
        int diff_ms = game_g->getGameTimeTotalMS() - this->time_ms;
        float alpha = ((float)diff_ms) / (float)duration_ms;
        alpha = std::max(alpha, 0.0f);
        alpha = std::min(alpha, 1.0f);
        Vector2D curr_pos = this->source_pos * (1.0f-alpha) + this->dest_pos * alpha;
        curr_pos.y += offset_y;
        this->object->setPos(curr_pos.x, curr_pos.y);
        this->object->setZValue(object->pos().y() + 1000.0f);
    }
}

void CharacterAction::notifyDead(const Character *character) {
    if( character == this->source )
        this->source = NULL;
    if( character == this->target_npc )
        this->target_npc = NULL;
}

bool CharacterAction::isExpired() const {
    if( game_g->getGameTimeTotalMS() >= time_ms + duration_ms ) {
        return true;
    }
    return false;
}

CharacterAction *CharacterAction::createSpellAction(PlayingGamestate *playing_gamestate, Character *source, Character *target_npc, const Spell *spell) {
    CharacterAction *character_action = new CharacterAction(CHARACTERACTION_SPELL, source, target_npc, -0.75f);
    //character_action->duration_ms = 250;
    const float speed = 0.01f; // units per ms
    character_action->duration_ms = (int)((character_action->dest_pos - character_action->source_pos).magnitude() / speed);
    character_action->spell = spell;
    character_action->object = playing_gamestate->addSpellGraphic(source->getPos() + Vector2D(0.0f, character_action->offset_y));
    return character_action;
}

CharacterAction *CharacterAction::createProjectileAction(PlayingGamestate *playing_gamestate, Character *source, Character *target_npc, bool hits, bool weapon_no_effect_magical, bool weapon_no_effect_holy, int weapon_damage, const string &projectile_key, float icon_width) {
    qDebug("CharacterAction::createProjectileAction()");
    CharacterAction *character_action = new CharacterAction(CHARACTERACTION_RANGED_WEAPON, source, target_npc, -0.75f);
    //character_action->duration_ms = 250;
    const float speed = 0.02f; // units per ms
    character_action->duration_ms = (int)((character_action->dest_pos - character_action->source_pos).magnitude() / speed);
    character_action->hits = hits;
    character_action->weapon_no_effect_magical = weapon_no_effect_magical;
    character_action->weapon_no_effect_holy = weapon_no_effect_holy;
    character_action->weapon_damage = weapon_damage;

    AnimatedObject *object = new AnimatedObject();
    character_action->object = object;
    object->addAnimationLayer( playing_gamestate->getProjectileAnimationLayer(projectile_key) );
    playing_gamestate->addGraphicsItem(object, icon_width, true);

    Vector2D dir = character_action->dest_pos - character_action->source_pos;
    Direction direction = directionFromVecDir(dir);
    object->setDimension(direction);

    character_action->update(); // set position, z-value
    qDebug("CharacterAction::createProjectileAction() exit");
    return character_action;
}

void CloseSubWindowWidget::closeEvent(QCloseEvent *event) {
    event->ignore();
    playing_gamestate->closeSubWindow();
}

void CloseAllSubWindowsWidget::closeEvent(QCloseEvent *event) {
    event->ignore();
    playing_gamestate->closeAllSubWindows();
}

/*void StatusBar::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    QBrush brush(color);
    painter.fillRect(QRectF(QPointF(0, 0), this->size()), brush);
}*/

StatsWindow::StatsWindow(PlayingGamestate *playing_gamestate) :
    CloseSubWindowWidget(playing_gamestate)
{
    playing_gamestate->addWidget(this, true);

    const Character *player = playing_gamestate->getPlayer();

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    if( player->getPortrait().length() > 0 ) {
        QPixmap pixmap = game_g->getPortraitImage(player->getPortrait());
        QLabel *portrait_label = new QLabel();
        int height = QApplication::desktop()->availableGeometry().height();
        int max_pic_height = height/3;
        qDebug("pixmap height %d , height %d , max_pic_height %d", pixmap.height(), height, max_pic_height);
        if( pixmap.height() > max_pic_height ) {
            qDebug("    scale...");
            pixmap = pixmap.scaledToHeight(max_pic_height, Qt::SmoothTransformation);
        }
        portrait_label->setPixmap(pixmap);
        layout->addWidget(portrait_label);
    }

    QString html ="<html><body>";

    html += "<table>";
    html += "<tr>";
    html += "<td>";

    html += "<b>Name:</b> ";
    html += player->getName().c_str();
    html += "<br/>";

    html += "<b>Difficulty:</b> ";
    html += Game::getDifficultyString(this->playing_gamestate->getDifficulty()).c_str();
    //html += "<br/>";
    html += " ";
    html += "<hr/>";

    html += "<table>";
    html += this->writeStat(profile_key_FP_c, false);
    html += this->writeStat(profile_key_BS_c, false);
    html += this->writeStat(profile_key_S_c, false);
    html += this->writeStat(profile_key_A_c, false);
    html += this->writeStat(profile_key_M_c, false);
    html += this->writeStat(profile_key_D_c, false);
    html += this->writeStat(profile_key_B_c, false);
    html += this->writeStat(profile_key_Sp_c, true);

    html += "<tr>";
    html += "<td>";
    html += "<b>Health:</b> ";
    html += "</td>";
    html += "<td>";
    if( player->getHealth() < player->getMaxHealth() ) {
        html += "<font color=\"#ff0000\">";
        html += QString::number(player->getHealth());
        html += "</font>";
    }
    else {
        html += QString::number(player->getHealth());
    }
    html += " / ";
    html += QString::number(player->getMaxHealth());
    html += " ";
    html += "</td>";
    html += "</tr>";

    html += "<tr>";
    html += "<td>";
    html += "<b>Level:</b> ";
    html += "</td>";
    html += "<td>";
    html += QString::number(player->getLevel());
    //html += "<br/>";
    html += " ";
    html += "</td>";
    html += "</tr>";
    html += "<tr>";
    html += "<td>";
    html += "<b>XP:</b> ";
    html += "</td>";
    html += "<td>";
    html += QString::number(player->getXP()) + " (" + QString::number(player->getXPForNextLevel()) + " required for next level.)";
    //html += "<br/>";
    html += " ";
    html += "</td>";
    html += "</tr>";

    html += "</table>";
    //html += "<br/>";

    if( player->isParalysed() ) {
        html += "<font color=\"#ff0000\">PARALYSED</font><br/>";
    }
    if( player->isDiseased() ) {
        html += "<font color=\"#ff0000\">DISEASED</font><br/>";
    }

    html += "</td>";
    html += "<td>";

    html += player->writeSkills();

    html += "<hr/>";
    html += "<table>";
    {
        stringstream str;
        str << "<tr>";
        str << "<td>";
        str << "<b>Weapon: </b>";
        str << "</td>";
        str << "<td>";
        int damageX = 0, damageY = 0, damageZ = 0;
        if( player->getCurrentWeapon() == NULL ) {
            //player->getNaturalDamage(&damageX, &damageY, &damageZ);
            damageX = player->getNaturalDamageX();
            damageY = player->getNaturalDamageY();
            damageZ = player->getNaturalDamageZ();
            str << "None ";
        }
        else {
            const Weapon *weapon = player->getCurrentWeapon();
            weapon->getDamage(&damageX, &damageY, &damageZ);
            str << weapon->getName() << " ";
        }
        str << "</td>";
        str << "<td>";
        str << "<b>Damage: </b>";
        str << "</td>";
        str << "<td>";
        str << getDiceRollString(damageX, damageY, damageZ);
        //str << "<br/>";
        str << " ";
        str << "</td>";
        str << "</tr>";
        html += str.str().c_str();
    }

    html += "<tr>";
    html += "<td>";
    html += "<b>Armour:</b> ";
    html += "</td>";
    html += "<td>";
    if( player->getCurrentArmour() == NULL ) {
        html += "None ";
    }
    else {
        html += player->getCurrentArmour()->getName().c_str();
        html += " ";
    }
    html += "</td>";
    html += "<td>";
    html += " <b>Rating:</b> ";
    html += "</td>";
    html += "<td>";
    html += QString::number(player->getArmourRating(true, true));
    //html += "<br/><br/>";
    html += " ";
    html += "</td>";
    html += "</tr>";
    html += "</table>";

    html += "</td>";
    html += "</tr>";
    html += "</table>";
    html += "</body></html>";

    QTextEdit *label = new QTextEdit();
    game_g->setTextEdit(label);
    label->setHtml(html);
    layout->addWidget(label);

    QPushButton *closeButton = new QPushButton(tr("Continue"));
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeSubWindow()));
}

QString StatsWindow::writeStat(const string &stat_key, bool is_float) const {
    QString html = playing_gamestate->getPlayer()->writeStat(stat_key, is_float, false);
    return html;
}

bool items_icon_mode = false;
//bool items_icon_mode = true;

ItemsWindow::ItemsWindow(PlayingGamestate *playing_gamestate) :
    CloseSubWindowWidget(playing_gamestate), list(NULL), weightLabel(NULL),
    armButton(NULL), wearButton(NULL), useButton(NULL), dropButton(NULL), infoButton(NULL),
    view_type(VIEWTYPE_ALL)
{
    playing_gamestate->addWidget(this, true);

    Character *player = playing_gamestate->getPlayer();

    QFont font = game_g->getFontStd();
    this->setFont(font);
    QFont font_small = game_g->getFontSmall();

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *viewAllButton = new QPushButton(tr("All"));
        game_g->initButton(viewAllButton);
        viewAllButton->setFont(font_small);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        viewAllButton->setToolTip(tr("Display all items (F1)"));
#endif
        viewAllButton->setShortcut(QKeySequence(Qt::Key_F1));
        h_layout->addWidget(viewAllButton);
        connect(viewAllButton, SIGNAL(clicked()), this, SLOT(clickedViewAll()));

        QPushButton *viewWeaponsButton = new QPushButton(tr("Wpns"));
        game_g->initButton(viewWeaponsButton);
        viewWeaponsButton->setFont(font_small);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        viewWeaponsButton->setToolTip(tr("Display only weapons (F2)"));
#endif
        viewWeaponsButton->setShortcut(QKeySequence(Qt::Key_F2));
        h_layout->addWidget(viewWeaponsButton);
        connect(viewWeaponsButton, SIGNAL(clicked()), this, SLOT(clickedViewWeapons()));

        QPushButton *viewAmmoButton = new QPushButton(tr("Ammo"));
        game_g->initButton(viewAmmoButton);
        viewAmmoButton->setFont(font_small);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        viewAmmoButton->setToolTip(tr("Display only ammunition (F3)"));
#endif
        viewAmmoButton->setShortcut(QKeySequence(Qt::Key_F3));
        h_layout->addWidget(viewAmmoButton);
        connect(viewAmmoButton, SIGNAL(clicked()), this, SLOT(clickedViewAmmo()));

        QPushButton *viewShieldsButton = new QPushButton(tr("Shields"));
        game_g->initButton(viewShieldsButton);
        viewShieldsButton->setFont(font_small);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        viewShieldsButton->setToolTip(tr("Display only shields (F4)"));
#endif
        viewShieldsButton->setShortcut(QKeySequence(Qt::Key_F4));
        h_layout->addWidget(viewShieldsButton);
        connect(viewShieldsButton, SIGNAL(clicked()), this, SLOT(clickedViewShields()));

        QPushButton *viewArmourButton = new QPushButton(tr("Arm", "Armour"));
        game_g->initButton(viewArmourButton);
        viewArmourButton->setFont(font_small);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        viewArmourButton->setToolTip(tr("Display only armour (F5)"));
#endif
        viewArmourButton->setShortcut(QKeySequence(Qt::Key_F5));
        h_layout->addWidget(viewArmourButton);
        connect(viewArmourButton, SIGNAL(clicked()), this, SLOT(clickedViewArmour()));

        QPushButton *viewMagicButton = new QPushButton(tr("Magic"));
        game_g->initButton(viewMagicButton);
        viewMagicButton->setFont(font_small);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        viewMagicButton->setToolTip(tr("Display only magical items (F6)"));
#endif
        viewMagicButton->setShortcut(QKeySequence(Qt::Key_F6));
        h_layout->addWidget(viewMagicButton);
        connect(viewMagicButton, SIGNAL(clicked()), this, SLOT(clickedViewMagic()));

        QPushButton *viewMiscButton = new QPushButton(tr("Misc"));
        game_g->initButton(viewMiscButton);
        viewMiscButton->setFont(font_small);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        viewMiscButton->setToolTip(tr("Display only miscellaneous items (F7)"));
#endif
        viewMiscButton->setShortcut(QKeySequence(Qt::Key_F7));
        h_layout->addWidget(viewMiscButton);
        connect(viewMiscButton, SIGNAL(clicked()), this, SLOT(clickedViewMisc()));
    }

    list = new ScrollingListWidget();
    if( items_icon_mode ) {
        list->setViewMode(QListView::IconMode);
        //list->setMovement(QListView::Static);
        list->setWordWrap(true);
    }
    else {
        if( !smallscreen_c ) {
            QFont list_font = list->font();
            list_font.setPointSize( list_font.pointSize() + 8 );
            list->setFont(list_font);
        }
    }
    {
        QFontMetrics fm(list->font());
        int icon_size = fm.height();
        //int icon_size = fm.width("LONGSWORD");
        if( items_icon_mode ) {
            icon_size *= 4;
        }
        list->setIconSize(QSize(icon_size, icon_size));
        LOG("icon size now %d, %d\n", list->iconSize().width(), list->iconSize().height());
        if( items_icon_mode ) {
            list->setGridSize(QSize(2*icon_size, 2*icon_size));
        }
    }
    layout->addWidget(list);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->grabKeyboard();

    this->refreshList();

    connect(list, SIGNAL(currentRowChanged(int)), this, SLOT(changedSelectedItem(int)));
    connect(list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(clickedInfo()));

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *goldLabel = new QLabel(tr("Gold") + ": " + QString::number( player->getGold() ));
        //goldLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // needed to fix problem of having too little vertical space (on Qt Smartphone Simulator at least)
        h_layout->addWidget(goldLabel);

        weightLabel = new QLabel(""); // label set in setWeightLabel()
        //weightLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(weightLabel);
        this->setWeightLabel();
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        armButton = new QPushButton(""); // text set in changedSelectedItem()
        game_g->initButton(armButton);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        armButton->setToolTip(tr("Arm/disarm weapon or shield (Space)"));
#endif
        h_layout->addWidget(armButton);
        connect(armButton, SIGNAL(clicked()), this, SLOT(clickedArmWeapon()));

        wearButton = new QPushButton(""); // text set in changedSelectedItem()
        game_g->initButton(wearButton);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        wearButton->setToolTip(tr("Wear/take off armour (Space)"));
#endif
        h_layout->addWidget(wearButton);
        connect(wearButton, SIGNAL(clicked()), this, SLOT(clickedWear()));

        useButton = new QPushButton(""); // text set in changedSelectedItem()
        game_g->initButton(useButton);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        useButton->setToolTip(tr("Use item (Space)"));
#endif
        h_layout->addWidget(useButton);
        connect(useButton, SIGNAL(clicked()), this, SLOT(clickedUseItem()));

        dropButton = new QPushButton(tr("Drop"));
        game_g->initButton(dropButton);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        dropButton->setToolTip(tr("Drop item onto the ground (D))"));
#endif
        dropButton->setShortcut(QKeySequence(Qt::Key_D));
        h_layout->addWidget(dropButton);
        connect(dropButton, SIGNAL(clicked()), this, SLOT(clickedDropItem()));

        infoButton = new QPushButton(tr("Info"));
        game_g->initButton(infoButton);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        infoButton->setToolTip(tr("More information on this item (I))"));
#endif
        infoButton->setShortcut(QKeySequence(Qt::Key_I));
        h_layout->addWidget(infoButton);
        connect(infoButton, SIGNAL(clicked()), this, SLOT(clickedInfo()));
    }

    QPushButton *closeButton = new QPushButton(tr("Continue"));
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeSubWindow()));

    /*if( list_items.size() > 0 ) {
        list->setCurrentRow(0);
    }*/
    int index = list->currentRow();
    this->changedSelectedItem(index);
}

void ItemsWindow::refreshList() {
    qDebug("ItemsWindow::refreshList()");
    list_items.clear();
    list->clear();
    Character *player = playing_gamestate->getPlayer();
    vector<Item *> sorted_list;
    for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
        Item *item = *iter;
        sorted_list.push_back(item);
    }
    std::stable_sort(sorted_list.begin(), sorted_list.end(), ItemCompare());
    for(vector<Item *>::iterator iter = sorted_list.begin(); iter != sorted_list.end(); ++iter) {
        Item *item = *iter;
        if( view_type == VIEWTYPE_WEAPONS && item->getType() != ITEMTYPE_WEAPON ) {
            continue;
        }
        else if( view_type == VIEWTYPE_SHIELDS && item->getType() != ITEMTYPE_SHIELD ) {
            continue;
        }
        else if( view_type == VIEWTYPE_ARMOUR && item->getType() != ITEMTYPE_ARMOUR ) {
            continue;
        }
        else if( view_type == VIEWTYPE_AMMO && item->getType() != ITEMTYPE_AMMO ) {
            continue;
        }
        else if( view_type == VIEWTYPE_MAGIC && !item->isMagical() ) {
            continue;
        }
        else if( view_type == VIEWTYPE_MISC && !( ( item->getType() == ITEMTYPE_GENERAL || item->getType() == ITEMTYPE_RING ) && !item->isMagical() ) ) {
            continue;
        }
        QString item_str = playing_gamestate->getItemString(item, true, items_icon_mode);
        //list->addItem( item_str );
        QListWidgetItem *list_item = new QListWidgetItem(item_str);
        QPixmap pixmap = playing_gamestate->getItemImage( item->getImageName() );
        pixmap = pixmap.scaledToHeight(list->iconSize().height());
        QIcon icon( pixmap );
        list_item->setIcon(icon);
        list->addItem(list_item);
        list_items.push_back(item);
    }
    if( list->count() > 0 ) {
        list->setCurrentRow(0);
    }
}

void ItemsWindow::refreshListTexts() {
    qDebug("ItemsWindow::refreshListTexts()");
    for(size_t i=0;i<list_items.size();i++) {
        const Item *item = list_items.at(i);
        QListWidgetItem *item_widget = list->item(i);
        item_widget->setText( playing_gamestate->getItemString(item, true, items_icon_mode) );
    }
}

void ItemsWindow::changeView(ViewType view_type) {
    if( this->view_type != view_type ) {
        this->view_type = view_type;
        this->refreshList();
    }
}

void ItemsWindow::clickedViewAll() {
    this->changeView(VIEWTYPE_ALL);
}

void ItemsWindow::clickedViewWeapons() {
    this->changeView(VIEWTYPE_WEAPONS);
}

void ItemsWindow::clickedViewAmmo() {
    this->changeView(VIEWTYPE_AMMO);
}

void ItemsWindow::clickedViewShields() {
    this->changeView(VIEWTYPE_SHIELDS);
}

void ItemsWindow::clickedViewArmour() {
    this->changeView(VIEWTYPE_ARMOUR);
}

void ItemsWindow::clickedViewMagic() {
    this->changeView(VIEWTYPE_MAGIC);
}

void ItemsWindow::clickedViewMisc() {
    this->changeView(VIEWTYPE_MISC);
}

void ItemsWindow::changedSelectedItem(int currentRow) {
    LOG("changedSelectedItem(%d)\n", currentRow);

    if( currentRow == -1 ) {
        dropButton->setVisible(false);
        armButton->setVisible(false);
        wearButton->setVisible(false);
        useButton->setVisible(false);
        infoButton->setVisible(false);
        return;
    }
    dropButton->setVisible(true);
    infoButton->setVisible(true);
    Item *item = list_items.at(currentRow);

    if( item->getType() == ITEMTYPE_WEAPON ) {
        armButton->setVisible(true);
        if( playing_gamestate->getPlayer()->getCurrentWeapon() == item ) {
            armButton->setText(tr("Disarm"));
        }
        else {
            armButton->setText(tr("Arm"));
        }
    }
    else if( item->getType() == ITEMTYPE_AMMO ) {
        if( playing_gamestate->getPlayer()->getCurrentAmmo() == item ) {
            armButton->setVisible(false);
        }
        else {
            armButton->setVisible(true);
            armButton->setText(tr("Select Ammo"));
        }
    }
    else if( item->getType() == ITEMTYPE_SHIELD ) {
        if( playing_gamestate->getPlayer()->getCurrentWeapon() != NULL && playing_gamestate->getPlayer()->getCurrentWeapon()->isTwoHanded() ) {
            armButton->setVisible(false);
        }
        else {
            armButton->setVisible(true);
            if( playing_gamestate->getPlayer()->getCurrentShield() == item ) {
                armButton->setText(tr("Disarm"));
            }
            else {
                armButton->setText(tr("Arm"));
            }
        }
    }
    else {
        armButton->setVisible(false);
    }

    if( item->getType() == ITEMTYPE_ARMOUR ) {
        wearButton->setVisible(true);
        if( playing_gamestate->getPlayer()->getCurrentArmour() == item ) {
            wearButton->setText(tr("Take Off"));
        }
        else {
            wearButton->setText(tr("Wear"));
        }
    }
    else if( item->getType() == ITEMTYPE_RING ) {
        wearButton->setVisible(true);
        if( playing_gamestate->getPlayer()->getCurrentRing() == item ) {
            wearButton->setText(tr("Take Off"));
        }
        else {
            wearButton->setText(tr("Wear"));
        }
    }
    else {
        wearButton->setVisible(false);
    }

    if( !item->canUse() ) {
        useButton->setVisible(false);
    }
    else {
        useButton->setVisible(true);
        useButton->setText(item->getUseVerb().c_str());
    }

    armButton->setShortcut(QKeySequence(Qt::Key_Space));
    wearButton->setShortcut(QKeySequence(Qt::Key_Space));
    useButton->setShortcut(QKeySequence(Qt::Key_Space));
}

void ItemsWindow::setWeightLabel() {
    Character *player = this->playing_gamestate->getPlayer();
    string weight_str = player->getWeightString();
    this->weightLabel->setText(weight_str.c_str());
}

void ItemsWindow::clickedArmWeapon() {
    LOG("clickedArmWeapon()\n");
    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    Item *item = list_items.at(index);
    if( item->getType() == ITEMTYPE_WEAPON ) {
        Weapon *weapon = static_cast<Weapon *>(item);
        if( playing_gamestate->getPlayer()->getCurrentWeapon() == weapon ) {
            // disarm instead
            LOG("player disarmed weapon\n");
            playing_gamestate->getPlayer()->armWeapon(NULL);
        }
        else {
            LOG("player armed weapon: %s\n", item->getName().c_str());
            playing_gamestate->getPlayer()->armWeapon(weapon);
            playing_gamestate->playSound("weapon_unsheath");
        }
    }
    else if( item->getType() == ITEMTYPE_AMMO ) {
        Ammo *ammo = static_cast<Ammo *>(item);
        if( playing_gamestate->getPlayer()->getCurrentAmmo() != ammo ) {
            LOG("player selected ammo: %s\n", item->getName().c_str());
            playing_gamestate->getPlayer()->selectAmmo(ammo);
        }
    }
    else if( item->getType() == ITEMTYPE_SHIELD ) {
        Shield *shield = static_cast<Shield *>(item);
        if( playing_gamestate->getPlayer()->getCurrentShield() == shield ) {
            // disarm instead
            LOG("player disarmed shield");
            playing_gamestate->getPlayer()->armShield(NULL);
        }
        else {
            LOG("player armed shield: %s\n", item->getName().c_str());
            playing_gamestate->getPlayer()->armShield(shield);
        }
    }
    else {
        LOG("not a weapon or shield?!\n");
    }

    this->refreshListTexts();
    this->changedSelectedItem(index);
}

void ItemsWindow::clickedWear() {
    LOG("clickedWearArmour()\n");
    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    Item *item = list_items.at(index);
    if( item->getType() == ITEMTYPE_ARMOUR ) {
        Armour *armour = static_cast<Armour *>(item);
        if( playing_gamestate->getPlayer()->getCurrentArmour() == armour ) {
            // take off instead
            LOG("player took off armour\n");
            playing_gamestate->getPlayer()->wearArmour(NULL);
        }
        else {
            LOG("player put on armour: %s\n", item->getName().c_str());
            playing_gamestate->getPlayer()->wearArmour(armour);
            playing_gamestate->playSound("wear_armour");
        }
    }
    else if( item->getType() == ITEMTYPE_RING ) {
        Ring *ring = static_cast<Ring *>(item);
        if( playing_gamestate->getPlayer()->getCurrentRing() == ring ) {
            // take off instead
            LOG("player took off ring");
            playing_gamestate->getPlayer()->wearRing(NULL);
        }
        else {
            LOG("player put on ring: %s\n", item->getName().c_str());
            playing_gamestate->getPlayer()->wearRing(ring);
        }
    }
    else {
        LOG("not an armour or ring?!\n");
    }

    this->refreshListTexts();
    this->changedSelectedItem(index);
}

void ItemsWindow::clickedUseItem() {
    LOG("clickedUseItem()\n");
    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    Item *item = list_items.at(index);
    Character *player = this->playing_gamestate->getPlayer();
    if( item->useItem(this->playing_gamestate, player) ) {
        // item is deleted
        player->takeItem(item);
        delete item;
        item = NULL;
        this->itemIsDeleted(index);
    }
    if( player->isDead() ) {
        this->playing_gamestate->closeSubWindow();
    }
}

void ItemsWindow::clickedDropItem() {
    LOG("clickedDropItem()\n");
    /*QList<QListWidgetItem *> selected_items = list->selectedItems();
    if( selected_items.size() == 1 ) {
        QListWidgetItem *selected_item = selected_items.at(0);*/
    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    Item *item = list_items.at(index);
    Character *player = playing_gamestate->getPlayer();
    player->dropItem(item);

    /*list->clear();
    list_items.clear();
    for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
        Item *item = *iter;
        list->addItem( item->getName().c_str() );
        list_items.push_back(item);
    }*/
    this->itemIsDeleted(index);
}

void ItemsWindow::clickedInfo() {
    LOG("clickedInfo()\n");
    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    const Item *item = list_items.at(index);
    string info = item->getDetailedDescription(playing_gamestate->getPlayer());
    info = convertToHTML(info);
    playing_gamestate->showInfoDialog(info, &playing_gamestate->getItemImage(item->getImageName()));
}

void ItemsWindow::itemIsDeleted(size_t index) {
    QListWidgetItem *list_item = list->takeItem(index);
    delete list_item;
    list_items.erase(list_items.begin() + index);
    if( list_items.size() > 0 ) {
        if( index > list_items.size()-1 )
            index = list_items.size()-1;
        list->setCurrentRow(index);
    }

    this->setWeightLabel();
}

TradeWindow::TradeWindow(PlayingGamestate *playing_gamestate, const vector<const Item *> &items, const vector<int> &costs) :
    CloseSubWindowWidget(playing_gamestate), goldLabel(NULL), weightLabel(NULL), list(NULL), items(items), costs(costs), player_list(NULL)
{
    playing_gamestate->addWidget(this, true);

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    Character *player = playing_gamestate->getPlayer();

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *label = new QLabel(tr("What would you like to buy?"));
        //label->setWordWrap(true);
        h_layout->addWidget(label);

        goldLabel = new QLabel("");
        h_layout->addWidget(goldLabel);
        this->updateGoldLabel();

        weightLabel = new QLabel("");
        h_layout->addWidget(weightLabel);
        this->setWeightLabel();
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *sellLabel = new QLabel(tr("Your items:"));
        h_layout->addWidget(sellLabel);

        QLabel *buyLabel = new QLabel(tr("Items to buy:"));
        h_layout->addWidget(buyLabel);
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        player_list = new ScrollingListWidget();
        if( !smallscreen_c ) {
            QFont list_font = player_list->font();
            list_font.setPointSize( list_font.pointSize() + 8 );
            player_list->setFont(list_font);
        }
        {
            QFontMetrics fm(player_list->font());
            int icon_size = fm.height();
            player_list->setIconSize(QSize(icon_size, icon_size));
        }
        h_layout->addWidget(player_list);
        vector<Item *> sorted_list;
        for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
            Item *item = *iter;
            sorted_list.push_back(item);
        }
        std::stable_sort(sorted_list.begin(), sorted_list.end(), ItemCompare());
        for(vector<Item *>::iterator iter = sorted_list.begin(); iter != sorted_list.end(); ++iter) {
            Item *item = *iter;
            int cost = 0;
            for(size_t i=0;i<items.size();i++) {
                const Item *item2 = items.at(i);
                if( item->getKey() == item2->getKey() || item->getBaseTemplate() == item2->getKey() ) {
                    cost = costs.at(i);
                    cost += item->getWorthBonus();
                }
            }
            addPlayerItem(item, cost);
        }

        list = new ScrollingListWidget();
        if( !smallscreen_c ) {
            QFont list_font = list->font();
            list_font.setPointSize( list_font.pointSize() + 8 );
            list->setFont(list_font);
        }
        {
            QFontMetrics fm(list->font());
            int icon_size = fm.height();
            list->setIconSize(QSize(icon_size, icon_size));
        }
        h_layout->addWidget(list);
        for(size_t i=0;i<items.size();i++) {
            const Item *item = items.at(i);
            int cost = costs.at(i);
            QString item_str = QString(item->getName().c_str()) + QString(" (") + QString::number(cost) + QString(" " + tr("gold") + ")");
            QListWidgetItem *list_item = new QListWidgetItem(item_str);
            QPixmap pixmap = playing_gamestate->getItemImage( item->getImageName() );
            pixmap = pixmap.scaledToHeight(list->iconSize().height());
            QIcon icon( pixmap );
            list_item->setIcon(icon);
            list->addItem(list_item);
        }
        connect(list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(clickedInfo()));
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *sellButton = new QPushButton(tr("Sell"));
        game_g->initButton(sellButton);
        sellButton->setFocusPolicy(Qt::NoFocus);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        sellButton->setToolTip(tr("Sell the item selected of yours (on the left) (S)"));
#endif
        sellButton->setShortcut(QKeySequence(Qt::Key_S));
        h_layout->addWidget(sellButton);
        connect(sellButton, SIGNAL(clicked()), this, SLOT(clickedSell()));

        QPushButton *buyButton = new QPushButton(tr("Buy"));
        game_g->initButton(buyButton);
        buyButton->setFocusPolicy(Qt::NoFocus);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        buyButton->setToolTip(tr("Buy the item selected (on the right) (B)"));
#endif
        buyButton->setShortcut(QKeySequence(Qt::Key_B));
        h_layout->addWidget(buyButton);
        connect(buyButton, SIGNAL(clicked()), this, SLOT(clickedBuy()));

        QPushButton *infoButton = new QPushButton(tr("Info"));
        game_g->initButton(infoButton);
        infoButton->setFocusPolicy(Qt::NoFocus);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        infoButton->setToolTip(tr("Display more information about the selected item for sale (I)"));
#endif
        infoButton->setShortcut(QKeySequence(Qt::Key_I));
        h_layout->addWidget(infoButton);
        connect(infoButton, SIGNAL(clicked()), this, SLOT(clickedInfo()));
    }

    QPushButton *closeButton = new QPushButton(tr("Finish Trading"));
    game_g->initButton(closeButton);
    closeButton->setFocusPolicy(Qt::NoFocus);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeSubWindow()));

    list->setFocus();
    list->setCurrentRow(0);
}

void TradeWindow::updateGoldLabel() {
    Character *player = playing_gamestate->getPlayer();
    goldLabel->setText(tr("Gold") + ": " + QString::number( player->getGold() ));
}

void TradeWindow::setWeightLabel() {
    Character *player = this->playing_gamestate->getPlayer();
    string weight_str = player->getWeightString();
    this->weightLabel->setText(weight_str.c_str());
}

void TradeWindow::addPlayerItem(Item *item, int buy_cost) {
    buy_cost *= 0.5f;
    //QString item_str = QString(item->getName().c_str()) + QString(" (") + QString::number(buy_cost) + QString(" gold)");
    QString item_str = playing_gamestate->getItemString(item, false, false) + QString(" (") + QString::number(buy_cost) + QString(" " + tr("gold") + ")");
    QListWidgetItem *list_item = new QListWidgetItem(item_str);
    if( buy_cost == 0 ) {
        list_item->setTextColor(Qt::gray);
        list_item->setFlags( list_item->flags() & ~Qt::ItemIsSelectable );
    }
    QPixmap pixmap = playing_gamestate->getItemImage( item->getImageName() );
    pixmap = pixmap.scaledToHeight(player_list->iconSize().height());
    QIcon icon( pixmap );
    list_item->setIcon(icon);
    player_list->addItem(list_item);
    player_items.push_back(item);

    player_costs.push_back(buy_cost);
}

void TradeWindow::clickedBuy() {
    LOG("TradeWindow::clickedBuy()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < items.size() );

    const Item *selected_item = items.at(index);
    int cost = costs.at(index);
    Character *player = playing_gamestate->getPlayer();
    if( player->getGold() >= cost ) {
        LOG("player buys: %s\n", selected_item->getName().c_str());
        player->addGold(-cost);
        this->updateGoldLabel();
        Item *item = playing_gamestate->cloneStandardItem(selected_item->getKey());
        player->addItem(item);
        this->setWeightLabel();
        this->addPlayerItem(item, cost);
        list->setCurrentRow(-1);
    }
    else {
        LOG("not enough money\n");
        //game_g->showInfoDialog("Trade", "You do not have enough money to purchase this item.");
        playing_gamestate->showInfoDialog(tr("You do not have enough money to purchase this item.").toStdString());
    }
}

void TradeWindow::clickedSell() {
    LOG("TradeWindow::clickedSell()\n");

    int index = player_list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < player_items.size() );

    Item *selected_item = player_items.at(index);
    int cost = player_costs.at(index);
    if( cost > 0 ) {
        LOG("player sells: %s\n", selected_item->getName().c_str());
        Character *player = playing_gamestate->getPlayer();
        player->addGold(cost);
        this->updateGoldLabel();
        player->takeItem(selected_item);
        delete selected_item;
        this->setWeightLabel();
        QListWidgetItem *list_item = player_list->takeItem(index);
        delete list_item;
        player_items.erase(player_items.begin() + index);
        player_costs.erase(player_costs.begin() + index);
    }
    /*else {
        // no longer needed, as items that can't be bought are grayed out
        playing_gamestate->showInfoDialog(tr("This shop doesn't buy that item."));
    }*/
}

void TradeWindow::clickedInfo() {
    LOG("TradeWindow::clickedInfo()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < items.size() );

    const Item *selected_item = items.at(index);
    string info = selected_item->getDetailedDescription(playing_gamestate->getPlayer());
    info = convertToHTML(info);
    playing_gamestate->showInfoDialog(info, &playing_gamestate->getItemImage(selected_item->getImageName()));
}

ItemsPickerWindow::ItemsPickerWindow(PlayingGamestate *playing_gamestate, vector<Item *> items) :
    CloseSubWindowWidget(playing_gamestate), list(NULL), items(items), player_list(NULL), weightLabel(NULL)
{
    playing_gamestate->addWidget(this, false);

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *groundLabel = new QLabel(tr("Items to pick up:"));
        h_layout->addWidget(groundLabel);

        QLabel *playerLabel = new QLabel(tr("Your items:"));
        h_layout->addWidget(playerLabel);
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        list = new ScrollingListWidget();
        list->grabKeyboard();
        if( !smallscreen_c ) {
            QFont list_font = list->font();
            list_font.setPointSize( list_font.pointSize() + 8 );
            list->setFont(list_font);
        }
        {
            QFontMetrics fm(list->font());
            int icon_size = fm.height();
            list->setIconSize(QSize(icon_size, icon_size));
        }
        h_layout->addWidget(list);
        for(size_t i=0;i<items.size();i++) {
            const Item *item = items.at(i);
            this->addGroundItem(item);
        }
        if( items.size() > 0 ) {
            list->setCurrentRow(0);
        }
        connect(list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(clickedPickUp()));

        player_list = new ScrollingListWidget();
        if( !smallscreen_c ) {
            QFont list_font = player_list->font();
            list_font.setPointSize( list_font.pointSize() + 8 );
            player_list->setFont(list_font);
        }
        {
            QFontMetrics fm(player_list->font());
            int icon_size = fm.height();
            player_list->setIconSize(QSize(icon_size, icon_size));
        }
        h_layout->addWidget(player_list);
        this->refreshPlayerItems();
    }

    weightLabel = new QLabel("");
    layout->addWidget(weightLabel);
    this->setWeightLabel();

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *pickUpButton = new QPushButton(tr("Pick Up"));
        game_g->initButton(pickUpButton);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        pickUpButton->setToolTip(tr("Pick up the selected item (Space)"));
#endif
        pickUpButton->setShortcut(QKeySequence(Qt::Key_Space));
        h_layout->addWidget(pickUpButton);
        connect(pickUpButton, SIGNAL(clicked()), this, SLOT(clickedPickUp()));

        QPushButton *dropButton = new QPushButton(tr("Drop"));
        game_g->initButton(dropButton);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        dropButton->setToolTip(tr("Drop the selected item of yours onto the ground"));
#endif
        h_layout->addWidget(dropButton);
        connect(dropButton, SIGNAL(clicked()), this, SLOT(clickedDrop()));

        QPushButton *infoButton = new QPushButton(tr("Info"));
        game_g->initButton(infoButton);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        infoButton->setToolTip(tr("Display more information about the selected item (I)"));
#endif
        infoButton->setShortcut(QKeySequence(Qt::Key_I));
        h_layout->addWidget(infoButton);
        connect(infoButton, SIGNAL(clicked()), this, SLOT(clickedInfo()));
    }

    QPushButton *closeButton = new QPushButton(tr("Close"));
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeSubWindow()));
}

void ItemsPickerWindow::addGroundItem(const Item *item) {
    QString item_str = QString(item->getName().c_str());
    QListWidgetItem *list_item = new QListWidgetItem(item_str);
    QPixmap pixmap = playing_gamestate->getItemImage( item->getImageName() );
    pixmap = pixmap.scaledToHeight(list->iconSize().height());
    QIcon icon( pixmap );
    list_item->setIcon(icon);
    list->addItem(list_item);
    // n.b., don't add to items list - should already be there, or otherwise caller should add it manually
}

void ItemsPickerWindow::addPlayerItem(Item *item) {
    QString item_str = QString(item->getName().c_str());
    QListWidgetItem *list_item = new QListWidgetItem(item_str);
    QPixmap pixmap = playing_gamestate->getItemImage( item->getImageName() );
    pixmap = pixmap.scaledToHeight(player_list->iconSize().height());
    QIcon icon( pixmap );
    list_item->setIcon(icon);
    player_list->addItem(list_item);
    player_items.push_back(item);
}

void ItemsPickerWindow::refreshPlayerItems() {
    player_list->clear();
    player_items.clear();
    Character *player = playing_gamestate->getPlayer();
    vector<Item *> sorted_list;
    for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
        Item *item = *iter;
        sorted_list.push_back(item);
    }
    std::stable_sort(sorted_list.begin(), sorted_list.end(), ItemCompare());
    for(vector<Item *>::iterator iter = sorted_list.begin(); iter != sorted_list.end(); ++iter) {
        Item *item = *iter;
        addPlayerItem(item);
    }
}

void ItemsPickerWindow::clickedPickUp() {
    LOG("ItemsPickerWindow::clickedPickUp()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < items.size() );

    Item *selected_item = items.at(index);
    Character *player = playing_gamestate->getPlayer();
    LOG("player picks up: %s\n", selected_item->getName().c_str());
    if( selected_item->getType() == ITEMTYPE_CURRENCY ) {
        this->playing_gamestate->playSound("coin");
    }
    player->pickupItem(selected_item);
    this->setWeightLabel();
    this->refreshPlayerItems();

    items.erase(items.begin() + index);
    delete list->takeItem(index);

    playing_gamestate->checkQuestComplete();
}

void ItemsPickerWindow::clickedDrop() {
    LOG("ItemsPickerWindow::clickedDrop()\n");

    int index = player_list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < player_items.size() );

    Item *selected_item = player_items.at(index);
    LOG("player drops: %s\n", selected_item->getName().c_str());
    Character *player = playing_gamestate->getPlayer();
    player->dropItem(selected_item);
    this->setWeightLabel();
    player_items.erase(player_items.begin() + index);
    delete player_list->takeItem(index);

    items.push_back(selected_item);
    this->addGroundItem(selected_item);
}

void ItemsPickerWindow::clickedInfo() {
    LOG("ItemsPickerWindow::clickedInfo()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < items.size() );

    const Item *selected_item = items.at(index);
    string info = selected_item->getDetailedDescription(playing_gamestate->getPlayer());
    info = convertToHTML(info);
    playing_gamestate->showInfoDialog(info, &playing_gamestate->getItemImage(selected_item->getImageName()));
}

void ItemsPickerWindow::setWeightLabel() {
    Character *player = this->playing_gamestate->getPlayer();
    string weight_str = player->getWeightString();
    this->weightLabel->setText(weight_str.c_str());
}

const int n_level_up_stats_c = 2;

LevelUpWindow::LevelUpWindow(PlayingGamestate *playing_gamestate) :
    playing_gamestate(playing_gamestate), closeButton(NULL) {
    playing_gamestate->addWidget(this, false);

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    Character *player = playing_gamestate->getPlayer();

    QString text = tr("You have advanced to level") + " " + QString::number(player->getLevel()) + " (" + QString::number(player->getXP()) + " XP)";
    text += "\n\n " + tr("Select") + " " + QString::number(n_level_up_stats_c) + " " + tr("statistics that you wish to improve:");
    QLabel *label = new QLabel(text);
    //label->setFont(game_g->getFontSmall());
    label->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    label->setWordWrap(true);
    //label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(label);

    {
        QGridLayout *g_layout = new QGridLayout();
        layout->addLayout(g_layout);

        int row = 0;
        g_layout->addWidget( addProfileCheckBox(profile_key_FP_c, tr("Hand-to-hand combat").toStdString()), row++, 0 );
        // mobile platforms may be too small to fit the labels (e.g., Symbian 640x360)
        // also need to be careful not to make the labels too long, for low resolution non-mobiles (e.g., 1024x600 netbooks)
        /*if( !smallscreen_c ) {
            g_layout->addWidget( new QLabel(tr("Hand-to-hand combat")), row++, 0 );
        }*/
        g_layout->addWidget( addProfileCheckBox(profile_key_BS_c, tr("Ranged combat").toStdString()), row++, 0 );
        /*if( !smallscreen_c ) {
            g_layout->addWidget( new QLabel(tr("Ranged combat")), row++, 0 );
        }*/
        g_layout->addWidget( addProfileCheckBox(profile_key_S_c, tr("How strong you are").toStdString()), row++, 0 );
        /*if( !smallscreen_c ) {
            g_layout->addWidget( new QLabel(tr("How strong you are")), row++, 0 );
        }*/

        row = 0;
        g_layout->addWidget( addProfileCheckBox(profile_key_M_c, tr("Mental and psychic abilities").toStdString()), row++, 1 );
        /*if( !smallscreen_c ) {
            g_layout->addWidget( new QLabel(tr("Mental and psychic abilities")), row++, 1 );
        }*/
        g_layout->addWidget( addProfileCheckBox(profile_key_D_c, tr("For avoiding traps").toStdString()), row++, 1 );
        /*if( !smallscreen_c ) {
            g_layout->addWidget( new QLabel(tr("For avoiding traps")), row++, 1 );
        }*/
        g_layout->addWidget( addProfileCheckBox(profile_key_B_c, tr("Your courage in the face of terrifying enemies").toStdString()), row++, 1 );
    }

    int initial_level = player->getInitialLevel();
    if( initial_level != 0 ) {
        // see which can be improved
        int n_levels = player->getLevel() - initial_level;
        qDebug("player has advanced %d levels to level %d", n_levels, player->getLevel());
        int max_stat_inc = (n_levels+1)/3;
        max_stat_inc++;
        qDebug("max_stat_inc = %d", max_stat_inc);
        for(map<string, QAbstractButton *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
            string key = (*iter).first;
            int initial_val = player->getInitialBaseProfile()->getIntProperty(key);
            int val = player->getBaseProfileIntProperty(key);
            qDebug("### check stat: %s %d vs %d", key.c_str(), val, initial_val);
            if( val - initial_val >= max_stat_inc ) {
                // already increased to max
                (*iter).second->setEnabled(false);
            }
        }

        // check we haven't disabled too many!
        int n_enabled = 0;
        for(map<string, QAbstractButton *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
            if( (*iter).second->isEnabled() ) {
                n_enabled++;
            }
        }
        ASSERT_LOGGER(n_enabled >= n_level_up_stats_c);
        if( n_enabled < n_level_up_stats_c ) {
            // runtime workaround
            LOG("error, not enough available level up stats: %d vs %d\n", n_enabled, n_level_up_stats_c);
            for(map<string, QAbstractButton *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
                (*iter).second->setEnabled(true);
            }
        }
    }

    // select some defaults - the min and max
    {
        int max_val = -1, min_val = -1;
        string min_key, max_key;
        for(map<string, QAbstractButton *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
            string key = (*iter).first;
            //qDebug("### check stat: %s", key.c_str());
            if( !(*iter).second->isEnabled() ) {
                continue;
            }
            int val = player->getBaseProfileIntProperty(key);
            if( min_val == -1 || val < min_val ) {
                min_val = val;
                min_key = key;
            }
            if( max_val == -1 || val > max_val ) {
                max_val = val;
                max_key = key;
            }
        }
        if( min_val == max_val ) {
            // all the same, set some defaults
            int count = 0;
            for(map<string, QAbstractButton *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
                QAbstractButton *check_box = (*iter).second;
                if( !check_box->isEnabled() ) {
                    continue;
                }
                check_box->setChecked(true);
                selected.push_back(check_box);
                count++;
                if( count == n_level_up_stats_c ) {
                    break;
                }
            }
        }
        else {
            check_boxes[max_key]->setChecked(true);
            selected.push_back(check_boxes[max_key]);
            check_boxes[min_key]->setChecked(true);
            selected.push_back(check_boxes[min_key]);
        }
    }

    for(map<string, QAbstractButton *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
        QAbstractButton *check_box = (*iter).second;
        //connect(check_box, SIGNAL(stateChanged(int)), this, SLOT(clickedCheckBox(int)));
        connect(check_box, SIGNAL(toggled(bool)), this, SLOT(toggledCheckBox(bool)));
        //connect(check_box, SIGNAL(clicked(bool)), this, SLOT(toggledCheckBox(bool)));
    }

    closeButton = new QPushButton(tr("Level Up!"));
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(clickedLevelUp()));
}

QAbstractButton *LevelUpWindow::addProfileCheckBox(const string &key, const string &help) {
    // n.b., actually returns QPushButton rather than QCheckBox - looks nicer, and check boxes don't work properly on Android if not using necessitas
    string long_string = getProfileLongString(key);
/*#ifdef Q_OS_ANDROID
    // problem on Android that text overlaps with checkbox
    QString android_hack = "            ";
#else
    QString android_hack = "";
#endif
    QCheckBox * check_box = new QCheckBox(android_hack + long_string.c_str());*/
    QPushButton *check_box = new QPushButton(long_string.c_str());
    check_box->setCheckable(true);
#ifndef Q_OS_ANDROID
    // for some reason, this sometimes shows on Android when it shouldn't?
    if( help.length() > 0 ) {
        check_box->setToolTip(help.c_str());
    }
#endif
    check_box->setStyleSheet("QPushButton:checked {color: black; background-color: green;}"); // needed as Android default style isn't very clear whether a button is enabled or not
    check_box->setFont(game_g->getFontStd());
    check_boxes[key] = check_box;
    return check_box;
}

void LevelUpWindow::toggledCheckBox(bool checked) {
    qDebug("LevelUpWindow::toggledCheckBox(%d)", checked);
    QObject *sender = this->sender();
    ASSERT_LOGGER( sender != NULL );
    // count how many now selected (includes this one)
    int count = 0;
    for(map<string, QAbstractButton *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
        QAbstractButton *check_box = (*iter).second;
        if( check_box->isChecked() ) {
            count++;
        }
    }
    qDebug("    count = %d", count);
    if( !checked ) {
        // still have to check count, as this function is called when we uncheck a state (because there are too many), rather than the user unchecking
        ASSERT_LOGGER( count <= n_level_up_stats_c );
        if( count < n_level_up_stats_c ) {
            this->closeButton->setEnabled(false);
        }
        // remove from the selected list
        for(vector<QAbstractButton *>::iterator iter = selected.begin(); iter != selected.end(); ++iter) {
            QAbstractButton *check_box = *iter;
            if( sender == check_box ) {
                selected.erase(iter);
                break;
            }
        }
    }
    else {
        ASSERT_LOGGER( count > 0 );
        ASSERT_LOGGER( count <= n_level_up_stats_c+1 );
        selected.push_back( static_cast<QAbstractButton *>(sender) );
        if( count >= n_level_up_stats_c ) {
            // can re-enable
            this->closeButton->setEnabled(true);
        }
        if( count > n_level_up_stats_c ) {
            // need to unselect one!
            ASSERT_LOGGER( selected.size() >= n_level_up_stats_c+1 );
            QAbstractButton *check_box = selected.at( selected.size() - n_level_up_stats_c - 1 );
            ASSERT_LOGGER( check_box->isChecked() );
            check_box->setChecked(false);
        }
    }
}

void LevelUpWindow::clickedLevelUp() {
    playing_gamestate->closeSubWindow();
    Character *player = playing_gamestate->getPlayer();

    int count = 0;
    for(map<string, QAbstractButton *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
        QAbstractButton *check_box = (*iter).second;
        if( check_box->isChecked() ) {
            count++;
            string key = (*iter).first;
            player->changeBaseProfileIntProperty(key, 1);
            if( count == n_level_up_stats_c ) {
                // no more allowed
                break;
            }
        }
    }

    stringstream xp_str;
    xp_str << "Advanced to level " << player->getLevel() << "!";
    playing_gamestate->addTextEffect(xp_str.str(), player->getPos(), 2000, 255, 255, 0);
}

CampaignWindow::CampaignWindow(PlayingGamestate *playing_gamestate) :
    playing_gamestate(playing_gamestate)
{
    playing_gamestate->addWidget(this, true);

    game_g->playSound(music_key_trade_c, true);

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    QString close_text;
    if( playing_gamestate->getQuest()->isCompleted() && playing_gamestate->isLastQuest() ) {
        QLabel *label = new QLabel(tr("Congratulations, you have completed all the quests!\n\nErebus is still under development, with more quests planned to be added in future. Thanks for playing!"));
        label->setFont(game_g->getFontSmall());
        label->setWordWrap(true);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(label);

        close_text = "Finish game";
    }
    else {
        QLabel *label = new QLabel(tr("You have left the dungeon, and returned to your village to rest. You may also take the time to visit the local shops to buy supplies, and sell any wares that you have."));
        label->setFont(game_g->getFontSmall());
        label->setWordWrap(true);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(label);

        for(vector<Shop *>::const_iterator iter = playing_gamestate->shopsBegin(); iter != playing_gamestate->shopsEnd(); ++iter) {
            const Shop *shop = *iter;
            if( !shop->isCampaign() )
                continue;
            QPushButton *shopButton = new QPushButton(shop->getName().c_str());
            game_g->initButton(shopButton);
            shopButton->setShortcut(QKeySequence(shop->getName().at(0)));
            QVariant variant = qVariantFromValue((void *)shop);
            shopButton->setProperty("shop", variant);
            //shopButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            layout->addWidget(shopButton);
            connect(shopButton, SIGNAL(clicked()), this, SLOT(clickedShop()));
        }

        /*QPushButton *trainingButton = new QPushButton(tr("Training"));
        game_g->initButton(trainingButton);
        //trainingButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(trainingButton);
        connect(trainingButton, SIGNAL(clicked()), this, SLOT(clickedTraining()));*/

        close_text = playing_gamestate->getQuest()->isCompleted() ? tr("Start next Quest") : tr("Continue your Quest");
    }

    QPushButton *closeButton = new QPushButton(close_text);
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(clickedClose()));
}

void CampaignWindow::clickedClose() {
    LOG("CampaignWindow::clickedClose()\n");
    this->playing_gamestate->closeSubWindow();
    if( playing_gamestate->getQuest()->isCompleted() ) {
        LOG("quest is completed\n");
        if( playing_gamestate->isLastQuest() ) {
            LOG("last quest, so game is complete\n");
            GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
            game_g->pushMessage(game_message);
        }
        else {
            LOG("move onto next quest\n");
            playing_gamestate->advanceQuest();
            const QuestInfo &c_quest_info = playing_gamestate->getCQuestInfo();
            QString qt_filename = DEPLOYMENT_PATH + QString(c_quest_info.getFilename().c_str());
            LOG("move onto next quest: %s\n", c_quest_info.getFilename().c_str());
            playing_gamestate->loadQuest(qt_filename, false);
        }
    }
#if !defined(Q_OS_SYMBIAN) // autosave disabled due to being slow on Nokia 5800 at least
    playing_gamestate->autoSave();
#endif
    game_g->cancelCurrentStream();
    //game_g->playSound(music_key_ingame_c, true);
}

void CampaignWindow::clickedShop() {
    LOG("CampaignWindow::clickedShop()\n");
    QObject *sender = this->sender();
    ASSERT_LOGGER( sender != NULL );
    const Shop *shop = static_cast<const Shop *>(sender->property("shop").value<void *>());
    ASSERT_LOGGER( shop != NULL );
    LOG("visit shop: %s\n", shop->getName().c_str());
    new TradeWindow(playing_gamestate, shop->getItems(), shop->getCosts());
}

/*void CampaignWindow::clickedArmourer() {
    LOG("CampaignWindow::clickedArmourer()\n");
}

void CampaignWindow::clickedGeneralStores() {
    LOG("CampaignWindow::clickedGeneralStores()\n");
}

void CampaignWindow::clickedMagicShop() {
    LOG("CampaignWindow::clickedMagicShop()\n");
}*/

/*void CampaignWindow::clickedTraining() {
    LOG("CampaignWindow::clickedTraining()\n");
    //game_g->showInfoDialog("Training", "This feature is not yet implemented");
    playing_gamestate->showInfoDialog("This feature is not yet implemented");
}*/

SaveGameWindow::SaveGameWindow(PlayingGamestate *playing_gamestate) :
    CloseAllSubWindowsWidget(playing_gamestate), list(NULL), edit(NULL)
{
    playing_gamestate->addWidget(this, false);

    QFont font = game_g->getFontBig();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    list = new ScrollingListWidget();
    list->grabKeyboard();
    if( !smallscreen_c ) {
        QFont list_font = list->font();
        list_font.setPointSize( list_font.pointSize() + 8 );
        list->setFont(list_font);
    }
    layout->addWidget(list);
    list->setSelectionMode(QAbstractItemView::SingleSelection);

    list->addItem(tr("New Save Game File"));
    save_filenames.push_back(""); // dummy entry

    /*QDir dir( QString(game_g->getApplicationFilename(savegame_folder).c_str()) );
    QStringList filter;
    filter << "*" + QString( savegame_ext.c_str() );
    QStringList files = dir.entryList(filter);
    for(int i=0;i<files.size();i++) {
        const QString &file = files.at(i);
        list->addItem(file);
    }*/
    game_g->fillSaveGameFiles(&list, &save_filenames);
    ASSERT_LOGGER(list->count() == save_filenames.size());

    list->setCurrentRow(0);

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *saveButton = new QPushButton(tr("Save"));
        game_g->initButton(saveButton);
        saveButton->setShortcut(QKeySequence(Qt::Key_Return));
        //saveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(saveButton);
        connect(saveButton, SIGNAL(clicked()), this, SLOT(clickedSave()));

        QPushButton *deleteButton = new QPushButton(tr("Delete File"));
        game_g->initButton(deleteButton);
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        deleteButton->setToolTip(tr("Delete the selected save game file (D)"));
#endif
        deleteButton->setShortcut(QKeySequence(Qt::Key_D));
        //deleteButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(deleteButton);
        connect(deleteButton, SIGNAL(clicked()), this, SLOT(clickedDelete()));
    }

    QPushButton *closeButton = new QPushButton(tr("Cancel"));
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Escape));
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeAllSubWindows()));
}

void SaveGameWindow::requestNewSaveGame() {
    QString filename;
    /*if( !playing_gamestate->isPermadeath() ) {
        filename = QString(savegame_root.c_str());
        QDateTime date_time = QDateTime::currentDateTime();
        QDate date = date_time.date();
        filename += QString("%1").arg(QString::number(date.year()), 2, QChar('0'));
        filename += "-";
        filename += QString("%1").arg(QString::number(date.month()), 2, QChar('0'));
        filename += "-";
        filename += QString("%1").arg(QString::number(date.day()), 2, QChar('0'));
        filename += "_";
        QTime time = date_time.time();
        filename += QString("%1").arg(QString::number(time.hour()), 2, QChar('0'));
        filename += "-";
        filename += QString("%1").arg(QString::number(time.minute()), 2, QChar('0'));
        filename += "-";
        filename += QString("%1").arg(QString::number(time.second()), 2, QChar('0'));
        //filename += date_time.toString();
    }*/

    QWidget *subwindow = new CloseAllSubWindowsWidget(playing_gamestate);
    playing_gamestate->addWidget(subwindow, false);

    QVBoxLayout *layout = new QVBoxLayout();
    subwindow->setLayout(layout);

    QLabel *label = new QLabel(tr("Choose a filename:"));
    layout->addWidget(label);

    QHBoxLayout *h_layout = new QHBoxLayout();
    layout->addLayout(h_layout);

    this->edit = new QLineEdit(filename);
    edit->grabKeyboard(); // needed, due to previously having set the savegame list to grab the keyboard
    // disallow: \ / : * ? " < > |
    QRegExp rx("[^\\\\/:*?\"<>|]*");
    QValidator *validator = new QRegExpValidator(rx, this);
    this->edit->setValidator(validator);
    h_layout->addWidget(edit);
    this->edit->setFocus();
    this->edit->setInputMethodHints(Qt::ImhNoPredictiveText); // needed on Android at least due to buggy behaviour (both with default keyboard - problem that pressing "Finished" we don't pick up latest text, and makes Swype crash); probably useful on other platforms

#if defined(_WIN32)
    QPushButton *oskButton = game_g->createOSKButton(edit);
    h_layout->addWidget(oskButton);
#endif

    QPushButton *saveButton = new QPushButton(tr("Save game"));
    game_g->initButton(saveButton);
    saveButton->setShortcut(QKeySequence(Qt::Key_Return));
    saveButton->setFont(game_g->getFontBig());
    saveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(saveButton);
    connect(saveButton, SIGNAL(clicked()), this, SLOT(clickedSaveNew()));

    QPushButton *closeButton = new QPushButton(tr("Cancel"));
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Escape));
    closeButton->setFont(game_g->getFontBig());
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeAllSubWindows()));
}

void SaveGameWindow::clickedSave() {
    LOG("SaveGameWindow::clickedSave()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(list->count() == save_filenames.size());
    ASSERT_LOGGER(index >= 0 && index < list->count());
    ASSERT_LOGGER(index >= 0 && index < save_filenames.size());
    if( index == 0 ) {
        this->requestNewSaveGame();
    }
    else {
        QString filename = save_filenames.at(index);
        LOG("save as existing file: %s\n", filename.toUtf8().data());
        if( playing_gamestate->saveGame(filename, false) ) {
            playing_gamestate->addTextEffect(tr("The game has been successfully saved").toStdString(), 5000);
        }
        else {
            game_g->showErrorDialog(tr("Failed to save game!").toStdString());
        }
        playing_gamestate->closeAllSubWindows();
    }
}

void SaveGameWindow::clickedDelete() {
    LOG("SaveGameWindow::clickedDelete()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(list->count() == save_filenames.size());
    ASSERT_LOGGER(index >= 0 && index < list->count());
    ASSERT_LOGGER(index >= 0 && index < save_filenames.size());
    if( index >= 1 ) {
        QString filename = save_filenames.at(index);
        if( playing_gamestate->askQuestionDialog(tr("Are you sure you wish to delete save game:").toStdString() + " " + filename.toStdString() + "?") ) {
            LOG("delete existing file: %s\n", filename.toUtf8().data());
            QString full_path = game_g->getApplicationFilename(savegame_folder + filename);
            LOG("full path: %s\n", full_path.toUtf8().data());
            //remove(full_path.c_str());
            QFile::remove(full_path);
            QListWidgetItem *list_item = list->takeItem(index);
            delete list_item;
            save_filenames.erase(save_filenames.begin() + index);
            ASSERT_LOGGER(list->count() == save_filenames.size());
        }
    }
}

void SaveGameWindow::clickedSaveNew() {
    LOG("SaveGameWindow::clickedSaveNew()\n");
    ASSERT_LOGGER(this->edit != NULL);
#if QT_VERSION >= 0x050000
    if( qApp->inputMethod() != NULL ) {
        // needed on Android, Galaxy Nexus, otherwise intermittent crashes when saving game by pressing "Finished" on on-screen-keyboard
        qApp->inputMethod()->hide();
    }
#endif
    QString filename = this->edit->text();

    if( filename.length() == 0 ) {
        playing_gamestate->showInfoDialog(tr("Please enter a filename.").toStdString());
        edit->grabKeyboard(); // needed on Symbian at least
    }
    else {
        filename += savegame_ext;
        //LOG("save as new file: %s\n", filename.toStdString().c_str());
        LOG("save as new file: %s\n", filename.toUtf8().data());
        QString full_path = game_g->getApplicationFilename(savegame_folder + filename);
        QFile qfile( full_path );
        bool ok = true;
        if( qfile.exists() ) {
            LOG("file exists!\n");
            if( !playing_gamestate->askQuestionDialog(tr("Are you sure you wish to overwrite an existing save game file?").toStdString()) ) {
                LOG("user says to not save\n");
                ok = false;
            }
        }
        if( ok ) {
            if( playing_gamestate->saveGame(full_path, true) ) {
                playing_gamestate->addTextEffect(tr("The game has been successfully saved").toStdString(), 5000);
            }
            else {
                game_g->showErrorDialog(tr("Failed to save game!").toStdString());
            }
        }
        playing_gamestate->closeAllSubWindows();
    }
}

PlayingGamestate::PlayingGamestate(bool is_savegame, GameType gameType, const string &player_type, const string &player_name, bool permadeath, bool cheat_mode, int cheat_start_level) :
    scene(NULL), view(NULL), gui_overlay(NULL),
    view_transform_3d(false), view_walls_3d(false),
    /*main_stacked_widget(NULL),*/
    turboButton(NULL), quickSaveButton(NULL), targetButton(NULL), zoomoutButton(NULL), zoominButton(NULL), centreButton(NULL),
    difficulty(DIFFICULTY_MEDIUM), permadeath(permadeath), permadeath_has_savefilename(false), player(NULL), time_hours(1), c_quest_indx(0), c_location(NULL), quest(NULL), gameType(gameType),
    is_keyboard_moving(false),
    target_animation_layer(NULL), target_item(NULL),
    time_last_complex_update_ms(0),
    need_visibility_update(false),
    has_ingame_music(false),
    music_mode(MUSICMODE_SILENCE), time_combat_ended(-1),
    is_created(false)
{
    // n.b., if we're loading a game, gameType will default to GAMETYPE_CAMPAIGN and will be set to the actual type when we load the quest
    try {
        LOG("PlayingGamestate::PlayingGamestate()\n");
        playingGamestate = this;

        srand( clock() );

        MainWindow *window = game_g->getMainWindow();
        window->setEnabled(false);
        game_g->setPaused(true, true);

        {
#if defined(Q_OS_SYMBIAN)
            const int res_c = 16;
#else
            const int res_c = 64;
#endif
            QPixmap pixmap(res_c, res_c);
            pixmap.fill(Qt::transparent);
            QRadialGradient radialGrad((res_c-1)/2, (res_c-1)/2, (res_c-1)/2);
            radialGrad.setColorAt(0.0, QColor(255, 255, 255, 63));
            radialGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
            QPainter painter(&pixmap);
            painter.setPen(Qt::NoPen);
            painter.fillRect(0, 0, res_c, res_c, radialGrad);
            painter.end();
            this->smoke_pixmap = pixmap;
        }
        {
#if defined(Q_OS_SYMBIAN)
            const int res_c = 16;
#else
            const int res_c = 64;
#endif
            QPixmap pixmap(res_c, res_c);
            pixmap.fill(Qt::transparent);
            QRadialGradient radialGrad((res_c-1)/2, (res_c-1)/2, (res_c-1)/2);
            radialGrad.setColorAt(0.0, QColor(255, 127, 0, 255));
            radialGrad.setColorAt(1.0, QColor(255, 127, 0, 0));
            QPainter painter(&pixmap);
            painter.setPen(Qt::NoPen);
            painter.fillRect(0, 0, res_c, res_c, radialGrad);
            painter.end();
            this->fireball_pixmap = pixmap;
        }
        {
#if defined(Q_OS_SYMBIAN)
            const int res_c = 16;
#else
            const int res_c = 64;
#endif
            /*QPixmap pixmap(res_c, res_c);
            pixmap.fill(Qt::transparent);
            QRadialGradient radialGrad((res_c-1)/2, (res_c-1)/2, (res_c-1)/2);
            radialGrad.setColorAt(0.0, QColor(255, 0, 0, 0));
            radialGrad.setColorAt(0.7, QColor(255, 0, 0, 0));
            radialGrad.setColorAt(0.75, QColor(255, 0, 0, 160));
            radialGrad.setColorAt(0.95, QColor(255, 0, 0, 160));
            radialGrad.setColorAt(1.0, QColor(255, 0, 0, 0));
            QPainter painter(&pixmap);
            painter.setPen(Qt::NoPen);
            painter.fillRect(0, 0, res_c, res_c, radialGrad);
            painter.end();
            this->target_pixmap = pixmap;*/
            int n_frames = 4;
            QPixmap full_pixmap(n_frames*res_c, res_c);
            full_pixmap.fill(Qt::transparent);
            QPainter painter(&full_pixmap);
            for(int i=0;i<n_frames;i++) {
                QRadialGradient radialGrad(i*res_c+(res_c-1)/2, (res_c-1)/2, (res_c-1)/2);
                float alpha = n_frames==1 ? 1.0f : i/(float)(n_frames-1);
                float size = (1.0f-alpha)*0.6f + alpha*1.0f;
                radialGrad.setColorAt(0.0*size, QColor(255, 0, 0, 0));
                radialGrad.setColorAt(0.7*size, QColor(255, 0, 0, 0));
                radialGrad.setColorAt(0.75*size, QColor(255, 0, 0, 160));
                radialGrad.setColorAt(0.95*size, QColor(255, 0, 0, 160));
                radialGrad.setColorAt(1.0*size, QColor(255, 0, 0, 0));
                QBrush brush(radialGrad);
                painter.fillRect(i*res_c, 0, res_c, res_c, brush);
            }
            this->target_pixmap = full_pixmap;
            vector<AnimationLayerDefinition> target_animation_layer_definition;
            target_animation_layer_definition.push_back( AnimationLayerDefinition("", 0, n_frames, AnimationSet::ANIMATIONTYPE_BOUNCE, 100) );
            this->target_animation_layer = AnimationLayer::create(this->target_pixmap, target_animation_layer_definition, true, 0, 0, res_c, res_c, res_c, res_c, n_frames*res_c, 1);
        }

        // create UI
        LOG("create UI\n");
        QFont font = game_g->getFontStd();
        window->setFont(font);

        scene = new QGraphicsScene(window);
        //scene->setSceneRect(0, 0, scene_w_c, scene_h_c);
        //scene->setItemIndexMethod(QGraphicsScene::NoIndex); // doesn't seem to help

        //view = new QGraphicsView(scene, window);
        view = new MainGraphicsView(this, scene, window);

        view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setBackgroundBrush(QBrush(Qt::black));
        view->setFrameStyle(QFrame::NoFrame);
        view->setFocusPolicy(Qt::NoFocus); // so clicking doesn't take focus away from the main window
        view->setCursor(Qt::OpenHandCursor);
        //view->grabGesture(Qt::PinchGesture);
        view->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
        view->setCacheMode(QGraphicsView::CacheBackground);
        //view->setOptimizationFlag(QGraphicsView::DontSavePainterState); // doesn't seem to help
        //view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate); // force full update every time
        view->setViewportUpdateMode(QGraphicsView::NoViewportUpdate); // we manually force full update every time (better performance than FullViewportUpdate)
        view->setAttribute(Qt::WA_TranslucentBackground, false); // may help with performance?
        //view->setDragMode(QGraphicsView::ScrollHandDrag);
        view->viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
        view->viewport()->setAttribute(Qt::WA_NoSystemBackground);

        /*QGLFormat fmt = QGLFormat::defaultFormat();
        fmt.setDoubleBuffer(true);
        fmt.setSwapInterval(1);
        QGLWidget *glWidget = new QGLWidget(fmt);
        view->setViewport(glWidget);*/
        /*if( QGLFormat::hasOpenGL() ) {
            LOG("enable OpenGL\n");
            view->setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers)));
            view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate); // force full update every time
        }
        else {
            LOG("OpenGL not available\n");
        }*/

        /*QWidget *centralWidget = new QWidget(window);
        this->mainwindow = centralWidget;
        LOG("mainwindow: %d\n", mainwindow);
        centralWidget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
        window->setCentralWidget(centralWidget);*/
        /*if( mobile_c ) {
            this->main_stacked_widget = new QStackedWidget();
            main_stacked_widget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
            window->setCentralWidget(main_stacked_widget);
        }*/

        QWidget *centralWidget = new QWidget();
        /*if( mobile_c ) {
            main_stacked_widget->addWidget(centralWidget);
        }
        else*/ {
            window->setCentralWidget(centralWidget);
        }
        this->widget_stack.push_back(centralWidget);

        QHBoxLayout *layout = new QHBoxLayout();
        centralWidget->setLayout(layout);

        {
            QVBoxLayout *v_layout = new QVBoxLayout();
            layout->addLayout(v_layout);

            QPushButton *statsButton = new QPushButton(tr("Stats"));
            game_g->initButton(statsButton);
            statsButton->setShortcut(QKeySequence(Qt::Key_F1));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            statsButton->setToolTip(tr("Display statistics of your character (F1)"));
#endif
            statsButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
            connect(statsButton, SIGNAL(clicked()), this, SLOT(clickedStats()));
            v_layout->addWidget(statsButton);

            QPushButton *itemsButton = new QPushButton(tr("Items"));
            game_g->initButton(itemsButton);
            itemsButton->setShortcut(QKeySequence(Qt::Key_F2));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            itemsButton->setToolTip(tr("Display the items that you are carrying (F2)"));
#endif
            itemsButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
            connect(itemsButton, SIGNAL(clicked()), this, SLOT(clickedItems()));
            v_layout->addWidget(itemsButton);

            /*QPushButton *spellsButton = new QPushButton(tr("Spells"));
            game_g->initButton(spellsButton);
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            spellsButton->setToolTip(tr("Not supported yet"));
#endif
            spellsButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
            v_layout->addWidget(spellsButton);*/

            QPushButton *journalButton = new QPushButton(tr("Journal"));
            game_g->initButton(journalButton);
            journalButton->setShortcut(QKeySequence(Qt::Key_F3));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            journalButton->setToolTip(tr("Displays information about your quests (F3)"));
#endif
            journalButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
            connect(journalButton, SIGNAL(clicked()), this, SLOT(clickedJournal()));
            v_layout->addWidget(journalButton);

            /*QPushButton *quitButton = new QPushButton(tr("Quit"));
            game_g->initButton(quitButton);
            quitButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
            v_layout->addWidget(quitButton);
            connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));*/

            QPushButton *pauseButton = new QPushButton(tr("Pause"));
            game_g->initButton(pauseButton);
            pauseButton->setShortcut(QKeySequence(Qt::Key_P));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            pauseButton->setToolTip(tr("Pause the game (P)"));
#endif
            pauseButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
            connect(pauseButton, SIGNAL(clicked()), this, SLOT(clickedPause()));
            v_layout->addWidget(pauseButton);

            QPushButton *restButton = new QPushButton(tr("Rest"));
            game_g->initButton(restButton);
            restButton->setShortcut(QKeySequence(Qt::Key_R));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            restButton->setToolTip(tr("Rest until you are healed (R)"));
#endif
            restButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
            connect(restButton, SIGNAL(clicked()), this, SLOT(clickedRest()));
            v_layout->addWidget(restButton);

            QPushButton *optionsButton = new QPushButton(tr("Options"));
            game_g->initButton(optionsButton);
            optionsButton->setShortcut(QKeySequence(Qt::Key_Escape));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            optionsButton->setToolTip(tr("Options to save game or quit (Escape)"));
#endif
            optionsButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
            connect(optionsButton, SIGNAL(clicked()), this, SLOT(clickedOptions()));
            v_layout->addWidget(optionsButton);
        }

        layout->addWidget(view);
        view->showFullScreen();

        //gui_overlay = new GUIOverlay(this, NULL);
        gui_overlay = new GUIOverlay(this, view);
        view->setGUIOverlay(gui_overlay);
        gui_overlay->setFadeIn();

        LOG("display UI\n");
        gui_overlay->setProgress(0);
        qApp->processEvents();
        qApp->processEvents(); // need to call this twice to get window to update, on Windows at least, for some reason?

        // create RPG data
        LOG("create RPG data\n");

        if( !is_savegame ) {
            LOG("create player\n");
            this->player = game_g->createPlayer(player_type, player_name);
        }

        //throw string("Failed to open images xml file"); // test
        //throw "unexpected error"; // test
        LOG("load images\n");
        {
            QString filename = QString(DEPLOYMENT_PATH) + "data/images.xml";
            QFile file(filename);
            if( !file.open(QFile::ReadOnly | QFile::Text) ) {
                qDebug("failed to open: %s", filename.toUtf8().data());
                throw string("Failed to open images xml file");
            }
            QXmlStreamReader reader(&file);
            while( !reader.atEnd() && !reader.hasError() ) {
                reader.readNext();
                if( reader.isStartElement() )
                {
                    if( reader.name() == "image" ) {
                        QStringRef type_s = reader.attributes().value("type");
                        if( type_s.length() == 0 ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("image element has no type attribute or is zero length");
                        }
                        QString type = type_s.toString();
                        QStringRef name_s = reader.attributes().value("name");
                        if( name_s.length() == 0 ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("image element has no name attribute or is zero length");
                        }
                        QString name = name_s.toString(); // need to take copy of string reference
                        QStringRef imagetype_s = reader.attributes().value("imagetype");
                        QString imagetype = imagetype_s.toString();
                        qDebug("image element type: %s name: %s imagetype: %s", type_s.toString().toStdString().c_str(), name_s.toString().toStdString().c_str(), imagetype_s.toString().toStdString().c_str());
                        QString filename;
                        QPixmap pixmap;
                        bool clip = false;
                        int xpos = 0, ypos = 0, width = 0, height = 0, expected_width = 0;
                        int stride_x = 0, stride_y = 0;
                        const int def_width_c = 128, def_height_c = 128;
                        int n_dimensions = 0;
                        QStringRef n_dimensions_s = reader.attributes().value("n_dimensions");
                        if( n_dimensions_s.length() > 0 ) {
                            n_dimensions = parseInt(n_dimensions_s.toString());
                        }
                        if( imagetype_s.length() == 0 ) {
                            // load file
                            QStringRef filename_s = reader.attributes().value("filename");
                            if( filename_s.length() == 0 ) {
                                LOG("error at line %d\n", reader.lineNumber());
                                throw string("image element has no filename attribute or is zero length");
                            }
                            filename = filename_s.toString();
                            qDebug("    filename: %s", filename.toStdString().c_str());
                            QStringRef xpos_s = reader.attributes().value("xpos");
                            QStringRef ypos_s = reader.attributes().value("ypos");
                            QStringRef width_s = reader.attributes().value("width");
                            QStringRef height_s = reader.attributes().value("height");
                            QStringRef expected_width_s = reader.attributes().value("expected_width");
                            if( xpos_s.length() > 0 || ypos_s.length() > 0 || width_s.length() > 0 || height_s.length() > 0 ) {
                                clip = true;
                                xpos = parseInt(xpos_s.toString(), true);
                                ypos = parseInt(ypos_s.toString(), true);
                                width = parseInt(width_s.toString());
                                height = parseInt(height_s.toString());
                                expected_width = parseInt(expected_width_s.toString());
                                qDebug("    clip to: %d, %d, %d, %d (expected width %d)", xpos, ypos, width, height, expected_width);

                                QStringRef stride_x_s = reader.attributes().value("stride_x");
                                if( stride_x_s.length() > 0 ) {
                                    stride_x = parseInt(stride_x_s.toString());
                                }
                                else {
                                    // set a default in case needed for animation
                                    stride_x = def_width_c;
                                }
                                QStringRef stride_y_s = reader.attributes().value("stride_y");
                                if( stride_y_s.length() > 0 ) {
                                    stride_y = parseInt(stride_y_s.toString());
                                }
                                else {
                                    // set a default in case needed for animation
                                    stride_y = def_height_c;
                                }
                            }
                            else {
                                // set up defaults in case needed for animation
                                if( expected_width_s.length() > 0 ) {
                                    expected_width = parseInt(expected_width_s.toString());
                                }
                                else {
                                    expected_width = def_width_c;
                                }

                                width = def_width_c;
                                height = def_height_c;
                                stride_x = def_width_c;
                                stride_y = def_height_c;
                            }
                            // image loaded later, lazily
                        }
                        else if( imagetype_s == "solid" ) {
                            QStringRef red_s = reader.attributes().value("red");
                            QStringRef green_s = reader.attributes().value("green");
                            QStringRef blue_s = reader.attributes().value("blue");
                            int red = parseInt(red_s.toString());
                            int green = parseInt(green_s.toString());
                            int blue = parseInt(blue_s.toString());
                            pixmap = QPixmap(16, 16);
                            pixmap.fill(QColor(red, green, blue));
                        }
                        else if( imagetype_s == "perlin" ) {
                            QStringRef max_red_s = reader.attributes().value("max_red");
                            QStringRef max_green_s = reader.attributes().value("max_green");
                            QStringRef max_blue_s = reader.attributes().value("max_blue");
                            QStringRef min_red_s = reader.attributes().value("min_red");
                            QStringRef min_green_s = reader.attributes().value("min_green");
                            QStringRef min_blue_s = reader.attributes().value("min_blue");
                            unsigned char filter_max[3] = {255, 255, 255};
                            unsigned char filter_min[3] = {255, 255, 255};
                            filter_max[0] = parseInt(max_red_s.toString());
                            filter_max[1] = parseInt(max_green_s.toString());
                            filter_max[2] = parseInt(max_blue_s.toString());
                            filter_min[0] = parseInt(min_red_s.toString());
                            filter_min[1] = parseInt(min_green_s.toString());
                            filter_min[2] = parseInt(min_blue_s.toString());
                            pixmap = createNoise(64, 64, 4.0f, 4.0f, filter_max, filter_min, NOISEMODE_PERLIN, 4);
                        }
                        else {
                            LOG("error at line %d\n", reader.lineNumber());
                            LOG("image element has unknown imagetype: %s\n", imagetype_s.string()->toStdString().c_str());
                            throw string("image element has unknown imagetype");
                        }
                        // now read any animations (only relevant for some types)
                        vector<AnimationLayerDefinition> animation_layer_definition;
                        while( !reader.atEnd() && !reader.hasError() && !(reader.isEndElement() && reader.name() == "image") ) {
                            reader.readNext();
                            if( reader.isStartElement() )
                            {
                                if( reader.name() == "animation" ) {
                                    QStringRef sub_name_s = reader.attributes().value("name");
                                    QStringRef sub_start_s = reader.attributes().value("start");
                                    QStringRef sub_length_s = reader.attributes().value("length");
                                    QStringRef sub_type_s = reader.attributes().value("type");
                                    QStringRef ms_per_frame_s = reader.attributes().value("ms_per_frame");
                                    int sub_start = parseInt(sub_start_s.toString());
                                    int sub_length = parseInt(sub_length_s.toString());
                                    AnimationSet::AnimationType animation_type = AnimationSet::ANIMATIONTYPE_SINGLE;
                                    if( sub_type_s == "single" ) {
                                        animation_type = AnimationSet::ANIMATIONTYPE_SINGLE;
                                    }
                                    else if( sub_type_s == "loop" ) {
                                        animation_type = AnimationSet::ANIMATIONTYPE_LOOP;
                                    }
                                    else if( sub_type_s == "bounce" ) {
                                        animation_type = AnimationSet::ANIMATIONTYPE_BOUNCE;
                                    }
                                    else {
                                        LOG("error at line %d\n", reader.lineNumber());
                                        throw string("image has unknown animation type");
                                    }
                                    int ms_per_frame = 100;
                                    if( ms_per_frame_s.length() > 0 ) {
                                        ms_per_frame = parseInt(ms_per_frame_s.toString());
                                    }
                                    animation_layer_definition.push_back( AnimationLayerDefinition(sub_name_s.toString().toStdString(), sub_start, sub_length, animation_type, ms_per_frame) );
                                }
                                else {
                                    LOG("error at line %d\n", reader.lineNumber());
                                    throw string("unknown xml tag within image section");
                                }
                            }
                        }

                        if( animation_layer_definition.size() > 0 && filename.length() == 0 ) {
                            throw string("animations can only load from filenames");
                        }
                        if( animation_layer_definition.size() > 0 && !clip ) {
                            // animations must have clip data specified - we'll use the defaults set above
                            clip = true;
                        }

                        if( filename.length() > 0 ) {
                            filename = DEPLOYMENT_PATH + filename;
                        }

                        if( type == "generic") {
                            if( animation_layer_definition.size() > 0 ) {
                                LOG("error at line %d\n", reader.lineNumber());
                                throw string("animations not supported for this animation type");
                            }
                            if( filename.length() > 0 )
                                pixmap = game_g->loadImage(filename.toStdString(), clip, xpos, ypos, width, height, expected_width);
                            this->builtin_images[name.toStdString()] = pixmap;
                        }
                        else if( type == "item") {
                            if( animation_layer_definition.size() > 0 ) {
                                LOG("error at line %d\n", reader.lineNumber());
                                throw string("animations not supported for this animation type");
                            }
                            if( filename.length() > 0 )
                                pixmap = game_g->loadImage(filename.toStdString(), clip, xpos, ypos, width, height, expected_width);
                            this->item_images[name.toStdString()] = pixmap;
                        }
                        else if( type == "projectile") {
                            if( animation_layer_definition.size() > 0 ) {
                                LOG("error at line %d\n", reader.lineNumber());
                                throw string("animations not supported for this animation type");
                            }
                            if( n_dimensions == 0 ) {
                                n_dimensions = 8;
                            }
                            animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 1, AnimationSet::ANIMATIONTYPE_SINGLE, 100) );
                            if( filename.length() > 0 )
                                this->projectile_animation_layers[name.toStdString()] = AnimationLayer::create(filename.toStdString(), animation_layer_definition, clip, xpos, ypos, width, height, stride_x, stride_y, expected_width, n_dimensions);
                            else
                                this->projectile_animation_layers[name.toStdString()] = AnimationLayer::create(pixmap, animation_layer_definition, clip, xpos, ypos, width, height, stride_x, stride_y, expected_width, n_dimensions);
                        }
                        else if( type == "scenery" ) {
                            if( animation_layer_definition.size() == 0 ) {
                                animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 1, AnimationSet::ANIMATIONTYPE_SINGLE, 100) );
                            }
                            if( filename.length() > 0 )
                                this->scenery_animation_layers[name.toStdString()] = new LazyAnimationLayer(filename.toStdString(), animation_layer_definition, clip, xpos, ypos, width, height, stride_x, stride_y, expected_width, 1);
                            else
                                this->scenery_animation_layers[name.toStdString()] = new LazyAnimationLayer(pixmap, animation_layer_definition, clip, xpos, ypos, width, height, stride_x, stride_y, expected_width, 1);
                        }
                        else if( type == "npc" ) {
                            unsigned int n_dimensions = animation_layer_definition.size() > 0 ? N_DIRECTIONS : 1;
                            if( animation_layer_definition.size() == 0 ) {
                                animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 1, AnimationSet::ANIMATIONTYPE_SINGLE, 100) );
                            }
                            if( filename.length() > 0 )
                                this->animation_layers[name.toStdString()] = new LazyAnimationLayer(filename.toStdString(), animation_layer_definition, clip, xpos, ypos, width, height, stride_x, stride_y, expected_width, n_dimensions);
                            else
                                this->animation_layers[name.toStdString()] = new LazyAnimationLayer(pixmap, animation_layer_definition, clip, xpos, ypos, width, height, stride_x, stride_y, expected_width, n_dimensions);
                        }
                        else {
                            LOG("error at line %d\n", reader.lineNumber());
                            LOG("unknown type attribute: %s\n", type.toStdString().c_str());
                            throw string("image element has unknown type attribute");
                        }
                    }
                }
            }
            if( reader.hasError() ) {
                LOG("error at line %d\n", reader.lineNumber());
                LOG("error reading images.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
                throw string("error reading images xml file");
            }
        }

        /*{
            // force all lazily loaded data to be loaded
            for(map<string, LazyAnimationLayer *>::iterator iter = this->animation_layers.begin(); iter != this->animation_layers.end(); ++iter) {
                LazyAnimationLayer *animation_layer = (*iter).second;
                animation_layer->getAnimationLayer();
            }
            for(map<string, LazyAnimationLayer *>::iterator iter = this->scenery_animation_layers.begin(); iter != this->scenery_animation_layers.end(); ++iter) {
                LazyAnimationLayer *animation_layer = (*iter).second;
                animation_layer->getAnimationLayer();
            }
        }*/

        gui_overlay->setProgress(20);
        qApp->processEvents();

        LOG("load items\n");
        {
            QFile file(QString(DEPLOYMENT_PATH) + "data/items.xml");
            if( !file.open(QFile::ReadOnly | QFile::Text) ) {
                throw string("Failed to open items xml file");
            }
            enum ItemsXMLType {
                ITEMS_XML_TYPE_NONE = 0,
                ITEMS_XML_TYPE_SHOP = 1,
                ITEMS_XML_TYPE_PLAYER_DEFAULT = 2
            };
            ItemsXMLType itemsXMLType = ITEMS_XML_TYPE_NONE;
            Shop *shop = NULL;
            string player_default_type;
            QXmlStreamReader reader(&file);
            while( !reader.atEnd() && !reader.hasError() ) {
                reader.readNext();
                if( reader.isStartElement() )
                {
                    /*qDebug("read xml: %s", reader.name().toString().toStdString().c_str());
                    qDebug("    type: %d", reader.tokenType());
                    qDebug("    n attributes: %d", reader.attributes().size());*/
                    if( reader.name() == "shop" ) {
                        if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unexpected items xml: shop element wasn't expected here");
                        }
                        QStringRef name_s = reader.attributes().value("name");
                        shop = new Shop(name_s.toString().toStdString());
                        QStringRef allow_random_npc_s = reader.attributes().value("allow_random_npc");
                        if( allow_random_npc_s.length() > 0 ) {
                            bool allow_random_npc = parseBool(allow_random_npc_s.toString());
                            shop->setAllowRandomNPC(allow_random_npc);
                        }
                        QStringRef campaign_s = reader.attributes().value("campaign");
                        if( campaign_s.length() > 0 ) {
                            bool campaign = parseBool(campaign_s.toString());
                            shop->setCampaign(campaign);
                        }
                        shops.push_back(shop);
                        itemsXMLType = ITEMS_XML_TYPE_SHOP;
                    }
                    else if( reader.name() == "purchase" ) {
                        if( itemsXMLType != ITEMS_XML_TYPE_SHOP ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unexpected items xml: purchase element wasn't expected here");
                        }
                        QStringRef template_s = reader.attributes().value("template");
                        QStringRef cost_s = reader.attributes().value("cost");
                        if( template_s.length() == 0 ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("purchase element has no template attribute");
                        }
                        int cost = parseInt(cost_s.toString());
                        Item *item = this->cloneStandardItem(template_s.toString().toStdString());
                        shop->addItem(item, cost);
                    }
                    else if( reader.name() == "player_default_items" ) {
                        if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unexpected items xml: player_default_items element wasn't expected here");
                        }
                        itemsXMLType = ITEMS_XML_TYPE_PLAYER_DEFAULT;
                        QStringRef type_s = reader.attributes().value("type");
                        player_default_type = type_s.toString().toStdString();
                    }
                    else if( reader.name() == "player_default_item" ) {
                        if( itemsXMLType != ITEMS_XML_TYPE_PLAYER_DEFAULT ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unexpected items xml: player_default_item element wasn't expected here");
                        }
                        if( !is_savegame && player_default_type == player_type ) {
                            QStringRef template_s = reader.attributes().value("template");
                            if( template_s.length() == 0 ) {
                                LOG("error at line %d\n", reader.lineNumber());
                                throw string("player_default_item element has no template attribute");
                            }
                            Item *item = this->cloneStandardItem(template_s.toString().toStdString());
                            player->addItem(item);
                        }
                    }
                    else {
                        if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unexpected items xml: element wasn't expected here");
                        }
                        Item *item = parseXMLItem( reader );
                        if( item != NULL ) {
                            this->addStandardItem( item );
                        }
                        // else ignore unknown element
                    }
                }
                else if( reader.isEndElement() ) {
                    if( reader.name() == "shop" ) {
                        if( itemsXMLType != ITEMS_XML_TYPE_SHOP ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unexpected items xml: shop end element wasn't expected here");
                        }
                        shop = NULL;
                        itemsXMLType = ITEMS_XML_TYPE_NONE;
                    }
                    else if( reader.name() == "player_default_items" ) {
                        if( itemsXMLType != ITEMS_XML_TYPE_PLAYER_DEFAULT ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unexpected items xml: player_default_items end element wasn't expected here");
                        }
                        itemsXMLType = ITEMS_XML_TYPE_NONE;
                    }
                }
            }
            if( reader.hasError() ) {
                LOG("error at line %d\n", reader.lineNumber());
                LOG("error reading items.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
                throw string("error reading items xml file");
            }
        }

        gui_overlay->setProgress(30);
        qApp->processEvents();

        LOG("load NPCs\n");
        {
            QFile file(QString(DEPLOYMENT_PATH) + "data/npcs.xml");
            if( !file.open(QFile::ReadOnly | QFile::Text) ) {
                throw string("Failed to open npcs xml file");
            }
            QXmlStreamReader reader(&file);
            while( !reader.atEnd() && !reader.hasError() ) {
                reader.readNext();
                if( reader.isStartElement() )
                {
                    if( reader.name() == "npc" ) {
                        QStringRef name_s = reader.attributes().value("name");
                        qDebug("found npc template: %s", name_s.toString().toStdString().c_str());
                        QStringRef type_s = reader.attributes().value("type");
                        QStringRef animation_name_s = reader.attributes().value("animation_name");
                        QStringRef static_image_s = reader.attributes().value("static_image");
                        bool static_image = parseBool(static_image_s.toString(), true);
                        QStringRef bounce_s = reader.attributes().value("bounce");
                        bool bounce = parseBool(bounce_s.toString(), true);
                        QStringRef image_size_s = reader.attributes().value("image_size");
                        QStringRef FP_s = reader.attributes().value("FP");
                        int FP = parseInt(FP_s.toString());
                        QStringRef BS_s = reader.attributes().value("BS");
                        int BS = parseInt(BS_s.toString());
                        QStringRef S_s = reader.attributes().value("S");
                        int S = parseInt(S_s.toString());
                        QStringRef A_s = reader.attributes().value("A");
                        int A = parseInt(A_s.toString());
                        QStringRef M_s = reader.attributes().value("M");
                        int M = parseInt(M_s.toString());
                        QStringRef D_s = reader.attributes().value("D");
                        int D = parseInt(D_s.toString());
                        QStringRef B_s = reader.attributes().value("B");
                        int B = parseInt(B_s.toString());
                        QStringRef Sp_s = reader.attributes().value("Sp");
                        float Sp = parseFloat(Sp_s.toString());
                        QStringRef health_min_s = reader.attributes().value("health_min");
                        int health_min = parseInt(health_min_s.toString());
                        QStringRef health_max_s = reader.attributes().value("health_max");
                        int health_max = parseInt(health_max_s.toString());
                        QStringRef gold_min_s = reader.attributes().value("gold_min");
                        int gold_min = parseInt(gold_min_s.toString());
                        QStringRef gold_max_s = reader.attributes().value("gold_max");
                        int gold_max = parseInt(gold_max_s.toString());
                        QStringRef xp_worth_s = reader.attributes().value("xp_worth");
                        int xp_worth = parseInt(xp_worth_s.toString());
                        QStringRef causes_terror_s = reader.attributes().value("causes_terror");
                        bool causes_terror = parseBool(causes_terror_s.toString(), true);
                        QStringRef terror_effect_s = reader.attributes().value("terror_effect");
                        int terror_effect = parseInt(terror_effect_s.toString(), true);
                        QStringRef causes_disease_s = reader.attributes().value("causes_disease");
                        int causes_disease = parseInt(causes_disease_s.toString(), true);
                        QStringRef causes_paralysis_s = reader.attributes().value("causes_paralysis");
                        int causes_paralysis = parseInt(causes_paralysis_s.toString(), true);
                        QStringRef requires_magical_s = reader.attributes().value("requires_magical");
                        bool requires_magical = parseBool(requires_magical_s.toString(), true);
                        QStringRef unholy_s = reader.attributes().value("unholy");
                        bool unholy = parseBool(unholy_s.toString(), true);

                        CharacterTemplate *character_template = new CharacterTemplate(animation_name_s.toString().toStdString(), FP, BS, S, A, M, D, B, Sp, health_min, health_max, gold_min, gold_max);

                        character_template->setXPWorth(xp_worth);
                        if( causes_terror ) {
                            character_template->setCausesTerror(terror_effect);
                        }
                        character_template->setCausesDisease(causes_disease);
                        character_template->setCausesParalysis(causes_paralysis);
                        character_template->setStaticImage(static_image);
                        character_template->setBounce(bounce);
                        if( image_size_s.length() > 0 ) {
                            float image_size = parseFloat(image_size_s.toString());
                            character_template->setImageSize(image_size);
                        }
                        character_template->setRequiresMagical(requires_magical);
                        character_template->setUnholy(unholy);

                        QStringRef natural_damageX_s = reader.attributes().value("natural_damageX");
                        QStringRef natural_damageY_s = reader.attributes().value("natural_damageY");
                        QStringRef natural_damageZ_s = reader.attributes().value("natural_damageZ");
                        if( natural_damageX_s.length() > 0 || natural_damageY_s.length() > 0 || natural_damageZ_s.length() > 0 ) {
                            int natural_damageX = parseInt(natural_damageX_s.toString());
                            int natural_damageY = parseInt(natural_damageY_s.toString());
                            int natural_damageZ = parseInt(natural_damageZ_s.toString());
                            character_template->setNaturalDamage(natural_damageX, natural_damageY, natural_damageZ);
                        }

                        QStringRef can_fly_s = reader.attributes().value("can_fly");
                        bool can_fly = parseBool(can_fly_s.toString(), true);
                        character_template->setCanFly(can_fly);

                        QStringRef weapon_resist_class_s = reader.attributes().value("weapon_resist_class");
                        if( weapon_resist_class_s.length() > 0 ) {
                            QStringRef weapon_resist_percentage_s = reader.attributes().value("weapon_resist_percentage");
                            int weapon_resist_percentage = parseInt(weapon_resist_percentage_s.toString());
                            character_template->setWeaponResist(weapon_resist_class_s.toString().toStdString(), weapon_resist_percentage);
                        }

                        QStringRef regeneration_s = reader.attributes().value("regeneration");
                        int regeneration = parseInt(regeneration_s.toString(), true);
                        character_template->setRegeneration(regeneration);

                        QStringRef death_explodes_s = reader.attributes().value("death_explodes");
                        bool death_explodes = parseBool(death_explodes_s.toString(), true);
                        if( death_explodes ) {
                            QStringRef death_explodes_damage_s = reader.attributes().value("death_explodes_damage");
                            int death_explodes_damage = parseInt(death_explodes_damage_s.toString());
                            character_template->setDeathExplodes(death_explodes_damage);
                        }

                        if( type_s.length() > 0 ) {
                            character_template->setType(type_s.toString().toStdString());
                        }

                        this->character_templates[ name_s.toString().toStdString() ] = character_template;
                    }
                }
            }
            if( reader.hasError() ) {
                LOG("error at line %d\n", reader.lineNumber());
                LOG("error reading npcs.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
                throw string("error reading npcs xml file");
            }
        }

        gui_overlay->setProgress(40);
        qApp->processEvents();

        LOG("load Spells\n");
        {
            QFile file(QString(DEPLOYMENT_PATH) + "data/spells.xml");
            if( !file.open(QFile::ReadOnly | QFile::Text) ) {
                throw string("Failed to open spells xml file");
            }
            QXmlStreamReader reader(&file);
            while( !reader.atEnd() && !reader.hasError() ) {
                reader.readNext();
                if( reader.isStartElement() )
                {
                    if( reader.name() == "spell" ) {
                        QStringRef name_s = reader.attributes().value("name");
                        qDebug("found spell template: %s", name_s.toString().toStdString().c_str());
                        QStringRef type_s = reader.attributes().value("type");
                        QStringRef effect_s = reader.attributes().value("effect");
                        QStringRef rollX_s = reader.attributes().value("rollX");
                        QStringRef rollY_s = reader.attributes().value("rollY");
                        QStringRef rollZ_s = reader.attributes().value("rollZ");
                        QStringRef damage_armour_s = reader.attributes().value("damage_armour");
                        QStringRef damage_shield_s = reader.attributes().value("damage_shield");
                        QStringRef mind_test_s = reader.attributes().value("mind_test");
                        int rollX = parseInt(rollX_s.toString(), true);
                        int rollY = parseInt(rollY_s.toString(), true);
                        int rollZ = parseInt(rollZ_s.toString(), true);
                        bool damage_armour = parseBool(damage_armour_s.toString(), true);
                        bool damage_shield = parseBool(damage_shield_s.toString(), true);
                        bool mind_test = parseBool(mind_test_s.toString(), true);
                        Spell *spell = new Spell(name_s.toString().toStdString(), type_s.toString().toStdString(), effect_s.toString().toStdString());
                        spell->setRoll(rollX, rollY, rollZ);
                        spell->setDamage(damage_armour, damage_shield);
                        spell->setMindTest(mind_test);
                        this->spells[ name_s.toString().toStdString() ] = spell;
                    }
                }
            }
            if( reader.hasError() ) {
                LOG("error at line %d\n", reader.lineNumber());
                LOG("error reading npcs.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
                throw string("error reading npcs xml file");
            }
        }

        gui_overlay->setProgress(60);
        qApp->processEvents();

        if( !is_savegame ) {
            this->loadPlayerAnimation();
        }

        gui_overlay->setProgress(70);
        qApp->processEvents();

        //if( !mobile_c )
        //if( game_g->isSoundEnabled() )
#ifndef Q_OS_ANDROID
        if( game_g->getGlobalSoundVolume() > 0 )
#endif
        {
            LOG("load sound effects\n");
            game_g->loadSound("click", string(DEPLOYMENT_PATH) + "sound/click_short.wav", false);
            game_g->loadSound("coin", string(DEPLOYMENT_PATH) + "sound/coin.wav", false);
            game_g->loadSound("container", string(DEPLOYMENT_PATH) + "sound/container.wav", false);
            game_g->loadSound("door", string(DEPLOYMENT_PATH) + "sound/door.wav", false);
            game_g->loadSound("drink", string(DEPLOYMENT_PATH) + "sound/bubble2.wav", false);
            game_g->loadSound("lock", string(DEPLOYMENT_PATH) + "sound/lock.wav", false);
            game_g->loadSound("turn_page", string(DEPLOYMENT_PATH) + "sound/turn_page.wav", false);
            game_g->loadSound("weapon_unsheath", string(DEPLOYMENT_PATH) + "sound/sword-unsheathe5.wav", false);
            game_g->loadSound("wear_armour", string(DEPLOYMENT_PATH) + "sound/chainmail1.wav", false);
            game_g->loadSound("spell_attack", string(DEPLOYMENT_PATH) + "sound/spell.wav", false);
#if !defined(Q_OS_SYMBIAN) && !defined(Q_WS_SIMULATOR)
            game_g->loadSound("swing", string(DEPLOYMENT_PATH) + "sound/swing2.wav", false);  // playing this sample causes strange pauses on Symbian?? (Nokia 5800)
#endif
#if !defined(Q_OS_SYMBIAN) && !defined(Q_WS_SIMULATOR)
            // strange pauses on Symbian?
            game_g->loadSound("footsteps", string(DEPLOYMENT_PATH) + "sound/stepdirt_1.wav", false);
#endif
            // remember to call freeSound in the PlayingGamestate destructor!
            LOG("done loading sound effects\n");

            if( !lightdistribution_c ) {
#ifndef USING_PHONON
                // only supported for SFML, as Phonon doesn't support looping
                this->has_ingame_music = true;
                //game_g->loadSound(music_key_ingame_c, string(DEPLOYMENT_PATH) + "music/exploring_loop.ogg", true);
                //game_g->setSoundVolume(music_key_ingame_c, 0.1f);
                game_g->loadSound(music_key_combat_c, string(DEPLOYMENT_PATH) + "music/battle_scene.ogg", true);
                game_g->setSoundVolume(music_key_combat_c, 0.1f);
                game_g->loadSound(music_key_trade_c, string(DEPLOYMENT_PATH) + "music/bazar_traide.ogg", true);
                game_g->setSoundVolume(music_key_trade_c, 0.1f);
                game_g->loadSound(music_key_game_over_c, string(DEPLOYMENT_PATH) + "music/your_fail.ogg", true);
                game_g->setSoundVolume(music_key_game_over_c, 1.0f);
#endif
            }
        }
#ifndef Q_OS_ANDROID
        else {
            LOG("sound is disabled\n");
        }
#endif

        gui_overlay->setProgress(90);
        qApp->processEvents();

        LOG("load quests\n");
        this->quest_list = game_g->loadQuests();

        gui_overlay->setProgress(100);
        qApp->processEvents();

        {
            QGridLayout *layout = new QGridLayout();
            view->setLayout(layout);

            layout->setColumnStretch(0, 1);
            layout->setRowStretch(1, 1);
            int col = 1;
            const int icon_size = game_g->getIconSize();
            const int button_size = game_g->getIconSize();

            QIcon turboIcon(this->builtin_images["gui_time"]);
            turboButton = new QPushButton(turboIcon, "");
            turboButton->setShortcut(QKeySequence(Qt::Key_T));
            turboButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            turboButton->setIconSize(QSize(icon_size, icon_size));
            turboButton->setStyleSheet("QPushButton:checked {color: black; background-color: coral;}"); // needed as Android default style isn't very clear whether a button is enabled or not
            turboButton->setMinimumSize(button_size, button_size);
            turboButton->setMaximumSize(button_size, button_size);
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            turboButton->setToolTip(tr("Toggle turbo mode: make game time go faster (T)"));
#endif
            turboButton->setCheckable(true);
            turboToggled(false); // initialise game speed to standard
            connect(turboButton, SIGNAL(toggled(bool)), this, SLOT(turboToggled(bool)));
            layout->addWidget(turboButton, 0, col++, Qt::AlignCenter);

            if( !this->permadeath ) {
                QIcon quickSaveIcon(this->builtin_images["gui_quicksave"]);
                quickSaveButton = new QPushButton(quickSaveIcon, "");
                quickSaveButton->setShortcut(QKeySequence(Qt::Key_F5));
#ifndef Q_OS_ANDROID
                // for some reason, this sometimes shows on Android when it shouldn't?
                quickSaveButton->setToolTip(tr("Quick-save (F5)"));
#endif
                quickSaveButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                quickSaveButton->setIconSize(QSize(icon_size, icon_size));
                quickSaveButton->setMinimumSize(button_size, button_size);
                quickSaveButton->setMaximumSize(button_size, button_size);
                connect(quickSaveButton, SIGNAL(clicked()), this, SLOT(quickSave()));
                layout->addWidget(quickSaveButton, 0, col++, Qt::AlignCenter);
            }

            QIcon targetIcon(this->builtin_images["gui_target"]);
            targetButton = new QPushButton(targetIcon, "");
            targetButton->setShortcut(QKeySequence(Qt::Key_N));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            targetButton->setToolTip(tr("Target/cycle through available enemies (N)"));
#endif
            targetButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            targetButton->setIconSize(QSize(icon_size, icon_size));
            targetButton->setMinimumSize(button_size, button_size);
            targetButton->setMaximumSize(button_size, button_size);
            connect(targetButton, SIGNAL(clicked()), this, SLOT(cycleTargetNPC()));
            layout->addWidget(targetButton, 0, col++, Qt::AlignCenter);

            QIcon zoomoutIcon(this->builtin_images["gui_zoomout"]);
            zoomoutButton = new QPushButton(zoomoutIcon, "");
            zoomoutButton->setShortcut(QKeySequence(Qt::Key_Minus));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            zoomoutButton->setToolTip(tr("Zoom out (-)"));
#endif
            zoomoutButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            zoomoutButton->setIconSize(QSize(icon_size, icon_size));
            zoomoutButton->setMinimumSize(button_size, button_size);

            zoomoutButton->setMaximumSize(button_size, button_size);
            connect(zoomoutButton, SIGNAL(clicked()), view, SLOT(zoomOut()));
            layout->addWidget(zoomoutButton, 0, col++, Qt::AlignCenter);

            QIcon zoominIcon(this->builtin_images["gui_zoomin"]);
            zoominButton = new QPushButton(zoominIcon, "");
            zoominButton->setShortcut(QKeySequence(Qt::Key_Plus));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            zoominButton->setToolTip(tr("Zoom in (+)"));
#endif
            zoominButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            zoominButton->setIconSize(QSize(icon_size, icon_size));
            zoominButton->setMinimumSize(button_size, button_size);
            zoominButton->setMaximumSize(button_size, button_size);
            connect(zoominButton, SIGNAL(clicked()), view, SLOT(zoomIn()));
            layout->addWidget(zoominButton, 0, col++, Qt::AlignCenter);

            QIcon centreIcon(this->builtin_images["gui_centre"]);
            centreButton = new QPushButton(centreIcon, "");
            centreButton->setShortcut(QKeySequence(Qt::Key_C));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            centreButton->setToolTip(tr("Centre view on your player's location (C)"));
#endif
            centreButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            centreButton->setIconSize(QSize(icon_size, icon_size));
            centreButton->setMinimumSize(button_size, button_size);
            centreButton->setMaximumSize(button_size, button_size);
            connect(centreButton, SIGNAL(clicked()), view, SLOT(centreOnPlayer()));
            layout->addWidget(centreButton, 0, col++, Qt::AlignCenter);

            //v_layout->addStretch();
        }

        window->setEnabled(true);
        game_g->setPaused(false, true);

        if( !is_savegame ) {
            //this->player->initialiseHealth(600); // CHEAT
            //player->addGold( 1000 ); // CHEAT
            //player->addXP(this, 200); // CHEAT
        }
        if( !is_savegame && cheat_mode ) {
            this->c_quest_indx = cheat_start_level % this->quest_list.size();
#if 0
            if( this->c_quest_indx == 1 ) {
                // CHEAT, simulate start of quest 2:
                player->setXP(70);
                player->setGold( 166 );
                player->deleteItem("Leather Armour");
                player->addItem(this->cloneStandardItem("Long Sword"), true);
                player->addItem(this->cloneStandardItem("Shield"), true);
                player->addItem(this->cloneStandardItem("Chain Mail Armour"), true);
                player->addItem(this->cloneStandardItem("Longbow"), true);
                player->addItem(this->cloneStandardItem("Arrows"), true);
                player->addItem(this->cloneStandardItem("Arrows"), true);
                player->addItem(this->cloneStandardItem("Arrows"), true);
            }
            else if( this->c_quest_indx == 2 ) {
                // CHEAT, simulate start of quest 3:
                {
                    // set profile directly
                    player->setXP(1140);
                    player->setLevel(5);
                    player->addProfile(2, 1, 1, 0, 2, 1, 1, 0.4f);
                    player->increaseMaxHealth(24);
                }
                player->setGold( 1241 + 300 );
                player->deleteItem("Leather Armour");
                player->deleteItem("Long Sword");
                player->armWeapon(NULL);
                player->addItem(this->cloneStandardItem("Two Handed Sword"), true);
                player->addItem(this->cloneStandardItem("Plate Armour"), true);
                player->addItem(this->cloneStandardItem("Longbow"), true);
                player->addItem(this->cloneStandardItem("Arrows"), true);
                player->addItem(this->cloneStandardItem("Arrows"), true);
                player->addItem(this->cloneStandardItem("Arrows"), true);
                {
                    Item *item = this->cloneStandardItem("Long Sword");
                    item->setName("Magical Long Sword");
                    item->setMagical(true);
                    item->setBaseTemplate("Long Sword");
                    player->addItem(item, true);
                }
            }
            else if( this->c_quest_indx == 3 ) {
                // CHEAT, simulate start of quest 4:
                /*{
                    // set profile directly
                    player->setXP(2110);
                    player->setLevel(6);
                    player->addProfile(2, 2, 1, 0, 2, 1, 2, 0.5f);
                    player->increaseMaxHealth(24);
                }*/
                player->setGold( 1998 + 350 );
                player->deleteItem("Leather Armour");
                player->deleteItem("Long Sword");
                player->armWeapon(NULL);
                //player->addItem(this->cloneStandardItem("Two Handed Sword"), true); // lose this, as no better than the Long Sword 2D10+2
                player->addItem(this->cloneStandardItem("Shield"), true);
                player->addItem(this->cloneStandardItem("Arrows"), true);
                player->addItem(this->cloneStandardItem("Arrows"), true);
                player->addItem(this->cloneStandardItem("Arrows"), true);
                {
                    Item *item = this->cloneStandardItem("Long Sword");
                    item->setBaseTemplate("Long Sword");
                    Weapon *weapon = static_cast<Weapon *>(item);
                    weapon->setDamage(2, 10, 2);
                    player->addItem(item, true);
                }
                {
                    Item *item = this->cloneStandardItem("Long Sword");
                    item->setName("Magical Long Sword");
                    item->setMagical(true);
                    item->setBaseTemplate("Long Sword");
                    player->addItem(item, true);
                }
                {
                    Item *item = this->cloneStandardItem("Plate Armour");
                    item->setName("Derrin's Armour");
                    item->setRating(7);
                    item->setMagical(true);
                    item->setBaseTemplate("Plate Armour");
                    player->addItem(item, true);
                }
                {
                    Ring *item = static_cast<Ring *>(this->cloneStandardItem("Gold Ring"));
                    item->setName("Derrin's Ring");
                    item->setProfileBonusIntProperty(profile_key_D_c, 1);
                    item->setMagical(true);
                    item->setBaseTemplate("Gold Ring");
                    player->addItem(item, true);
                    player->wearRing(item);
                }
                {
                    Item *item = this->cloneStandardItem("Arrows");
                    item->setName("Arrows +1");
                    item->setRating(2);
                    item->setMagical(true);
                    item->setBaseTemplate("Arrows");
                    Ammo *ammo = static_cast<Ammo *>(item);
                    ammo->setAmount(10);
                    player->addItem(item, true);
                }
                player->addItem(this->cloneStandardItem("Longbow"), true);

                // further test for Middleham, after Vaults:
                /*player->setXP(2945);
                player->setGold( 1597 );
                player->addItem(this->cloneStandardItem("Holy Water"), true);
                player->addItem(this->cloneStandardItem("Holy Water"), true);
                player->addItem(this->cloneStandardItem("Potion of Cure Disease"), true);
                player->addItem(this->cloneStandardItem("Potion of Cure Disease"), true);
                player->addItem(this->cloneStandardItem("Potion of Cure Disease"), true);*/
                // further test for Stockton, after killing all zombies and returning heirloom:
                /*{
                    // set profile directly
                    player->setXP(3580);
                    player->setLevel(7);
                    player->addProfile(3, 2, 1, 0, 2, 1, 3, 0.52f);
                    player->increaseMaxHealth(26);
                }
                player->setGold( 1645 );
                player->addItem(this->cloneStandardItem("Holy Water"), true);
                player->addItem(this->cloneStandardItem("Holy Water"), true);
                player->addItem(this->cloneStandardItem("Potion of Cure Disease"), true);*/
                // further test for Wentbridge Fort, start of Dungeons:
                {
                    // set profile directly
                    player->setXP(3695);
                    player->setLevel(7);
                    player->addProfile(3, 2, 1, 0, 2, 1, 3, 0.52f);
                    player->increaseMaxHealth(26);
                }
                player->setGold( 1645 );
                player->addItem(this->cloneStandardItem("Holy Water"), true);
                player->addItem(this->cloneStandardItem("Holy Water"), true);
                player->addItem(this->cloneStandardItem("Potion of Cure Disease"), true);
                player->addItem(this->cloneStandardItem("Potion of Cure Disease"), true);
                player->addItem(this->cloneStandardItem("Potion of Cure Disease"), true);
                player->addItem(this->cloneStandardItem("Potion of Healing"), true);
            }
#endif
        }
    }
    catch(...) {
        // clean up stuff created in the constructor
        LOG("PlayingGamestate constructor throwing an exception");
        this->cleanup();
        throw;
    }
}

PlayingGamestate::~PlayingGamestate() {
    LOG("PlayingGamestate::~PlayingGamestate()\n");
    this->cleanup();
}

void PlayingGamestate::cleanup() {
    LOG("PlayingGamestate::cleanup()\n");
    //this->closeSubWindow();
    this->closeAllSubWindows();

    MainWindow *window = game_g->getMainWindow();
    window->centralWidget()->deleteLater();
    window->setCentralWidget(NULL);

    playingGamestate = NULL;

    delete quest;
    quest = NULL;
    c_location = NULL;

    for(vector<CharacterAction *>::iterator iter = this->character_actions.begin(); iter != this->character_actions.end(); ++iter) {
        CharacterAction *character_action = *iter;
        delete character_action;
    }
    if( this->target_animation_layer != NULL ) {
        delete this->target_animation_layer;
    }
    /*for(map<string, AnimationLayer *>::iterator iter = this->animation_layers.begin(); iter != this->animation_layers.end(); ++iter) {
        AnimationLayer *animation_layer = (*iter).second;
        delete animation_layer;
    }*/
    for(map<string, LazyAnimationLayer *>::iterator iter = this->animation_layers.begin(); iter != this->animation_layers.end(); ++iter) {
        LazyAnimationLayer *animation_layer = (*iter).second;
        delete animation_layer;
    }
    /*for(map<string, AnimationLayer *>::iterator iter = this->scenery_animation_layers.begin(); iter != this->scenery_animation_layers.end(); ++iter) {
        AnimationLayer *animation_layer = (*iter).second;
        delete animation_layer;
    }*/
    for(map<string, LazyAnimationLayer *>::iterator iter = this->scenery_animation_layers.begin(); iter != this->scenery_animation_layers.end(); ++iter) {
        LazyAnimationLayer *animation_layer = (*iter).second;
        delete animation_layer;
    }
    for(map<string, AnimationLayer *>::iterator iter = this->projectile_animation_layers.begin(); iter != this->projectile_animation_layers.end(); ++iter) {
        AnimationLayer *animation_layer = (*iter).second;
        delete animation_layer;
    }
    for(map<string, Item *>::iterator iter = this->standard_items.begin(); iter != this->standard_items.end(); ++iter) {
        Item *item = iter->second;
        qDebug("about to delete standard item: %p", item);
        qDebug("    name: %s", item->getName().c_str());
        delete item;
    }
    for(map<string, CharacterTemplate *>::iterator iter = this->character_templates.begin(); iter != this->character_templates.end(); ++iter) {
        CharacterTemplate *character_template = iter->second;
        qDebug("about to delete character template: %p", character_template);
        delete character_template;
    }
    for(vector<Shop *>::iterator iter = shops.begin(); iter != shops.end(); ++iter) {
        Shop *shop = *iter;
        delete shop;
    }

    //if( game_g->isSoundEnabled() )
#ifndef Q_OS_ANDROID
    if( game_g->getGlobalSoundVolume() )
#endif
    {
        game_g->freeSound("click");
        game_g->freeSound("coin");
        game_g->freeSound("container");
        game_g->freeSound("door");
        game_g->freeSound("drink");
        game_g->freeSound("lock");
        game_g->freeSound("turn_page");
        game_g->freeSound("weapon_unsheath");
        game_g->freeSound("wear_armour");
        game_g->freeSound("spell_attack");
        game_g->freeSound("swing");
        game_g->freeSound("footsteps");
        //game_g->freeSound(music_key_ingame_c);
        game_g->freeSound(music_key_combat_c);
        game_g->freeSound(music_key_trade_c);
        game_g->freeSound(music_key_game_over_c);
    }
    LOG("done\n");
}

void PlayingGamestate::loadPlayerAnimation() {
    LOG("PlayingGamestate::loadPlayerAnimation()\n");

    vector<AnimationLayerDefinition> player_animation_layer_definition;
    // if we change the offsets, remember to update the hotspot in PlayingGamestate::locationAddCharacter !
    const int off_x = 32, off_y = 40, width = 64, height = 64;
    //const int off_x = 0, off_y = 0, width = 128, height = 128;
    const int expected_stride_x = 128, expected_stride_y = 128, expected_total_width = 4096;
    //float off_x = 32.0f/128.0f, off_y = 40.0f/128.0f, width = 64.0f/128.0f, height = 64.0f/128.0f;
    //float off_x = 0.0f/128.0f, off_y = 0.0f/128.0f, width = 128.0f/128.0f, height = 128.0f/128.0f;
    player_animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 4, AnimationSet::ANIMATIONTYPE_BOUNCE, 100) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("run", 4, 8, AnimationSet::ANIMATIONTYPE_LOOP, 100) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("attack", 12, 4, AnimationSet::ANIMATIONTYPE_SINGLE, 100) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("ranged", 28, 4, AnimationSet::ANIMATIONTYPE_SINGLE, 100) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("death", 18, 6, AnimationSet::ANIMATIONTYPE_SINGLE, 100) );

    string animation_folder = player->getAnimationFolder();
    if( animation_folder.length() == 0 ) {
        // needed for backwards compatibility with save games where we didn't store animation_folder!
        animation_folder = "isometric_hero";
    }
    string player_folder = "gfx/textures/" + animation_folder + "/";
    LOG("player_folder: %s\n", player_folder.c_str());
    this->animation_layers["player"] = new LazyAnimationLayer(AnimationLayer::create(string(DEPLOYMENT_PATH) + player_folder + "player.png", player_animation_layer_definition, true, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS));
    this->animation_layers["longsword"] = new LazyAnimationLayer(AnimationLayer::create(string(DEPLOYMENT_PATH) + player_folder + "longsword.png", player_animation_layer_definition, true, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS));
    this->animation_layers["sling"] = new LazyAnimationLayer(AnimationLayer::create(string(DEPLOYMENT_PATH) + player_folder + "sling.png", player_animation_layer_definition, true, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS));
    this->animation_layers["longbow"] = new LazyAnimationLayer(AnimationLayer::create(string(DEPLOYMENT_PATH) + player_folder + "longbow.png", player_animation_layer_definition, true, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS));
    this->animation_layers["dagger"] = new LazyAnimationLayer(AnimationLayer::create(string(DEPLOYMENT_PATH) + player_folder + "dagger.png", player_animation_layer_definition, true, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS));
    this->animation_layers["shield"] = new LazyAnimationLayer(AnimationLayer::create(string(DEPLOYMENT_PATH) + player_folder + "shield.png", player_animation_layer_definition, true, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS));
}

float PlayingGamestate::getDifficultyModifier() const {
    float factor = 1.0;
    switch( difficulty ) {
    case DIFFICULTY_EASY:
        factor = 1.2f;
        break;
    case DIFFICULTY_MEDIUM:
        factor = 1.0f;
        break;
    case DIFFICULTY_HARD:
        factor = 0.85f;
        break;
    default:
        ASSERT_LOGGER(false);
        break;
    }
    return factor;
}

void PlayingGamestate::playBackgroundMusic() {
    // needed for looping
    qDebug("PlayingGamestate::playBackgroundMusic()");
/*#ifndef Q_OS_ANDROID
    if( game_g->isSoundEnabled() ) {
        this->sound_effects["background"]->stop();
        this->sound_effects["background"]->play();
    }
#endif*/
}

void PlayingGamestate::turboToggled(bool checked) {
    LOG("PlayingGamestate::turboToggled(%d)\n", checked);
    turboButton->clearFocus(); // workaround for Android still showing selection
    game_g->setGameTimeMultiplier(checked ? 2 : 1);
}

void PlayingGamestate::quickSave() {
    qDebug("quickSave()");
    if( quickSaveButton != NULL ) {
        quickSaveButton->clearFocus(); // workaround for Android still showing selection
    }
    if( !this->permadeath ) {
        if( !this->canSaveHere() ) {
            this->showInfoDialog(tr("You cannot save here - enemies are nearby.").toStdString());
            return;
        }
        if( this->saveGame("quicksave.xml", false) )  {
            this->addTextEffect(tr("The game has been successfully saved").toStdString(), 5000);
        }
        else {
            game_g->showErrorDialog(tr("Failed to save game!").toStdString());
        }
    }
}

void PlayingGamestate::parseXMLItemProfileAttributeInt(Item *item, const QXmlStreamReader &reader, const string &key) const {
    string attribute = "bonus_" + key;
    QStringRef attribute_sr = reader.attributes().value(attribute.c_str());
    if( attribute_sr.length() != 0 ) {
        int value = parseInt(attribute_sr.toString());
        item->setProfileBonusIntProperty(key, value);
    }
}

void PlayingGamestate::parseXMLItemProfileAttributeFloat(Item *item, const QXmlStreamReader &reader, const string &key) const {
    string attribute = "bonus_" + key;
    QStringRef attribute_sr = reader.attributes().value(attribute.c_str());
    if( attribute_sr.length() != 0 ) {
        float value = parseFloat(attribute_sr.toString());
        item->setProfileBonusFloatProperty(key, value);
    }
}

Item *PlayingGamestate::parseXMLItem(QXmlStreamReader &reader) const {
    Item *item = NULL;

    QStringRef base_template_sr = reader.attributes().value("base_template");
    QStringRef arg1_sr = reader.attributes().value("arg1");
    QStringRef arg2_sr = reader.attributes().value("arg2");
    QStringRef arg1_s_sr = reader.attributes().value("arg1_s");
    QStringRef weight_sr = reader.attributes().value("weight");
    QStringRef rating_sr = reader.attributes().value("rating");
    QStringRef magical_sr = reader.attributes().value("magical");
    QStringRef worth_bonus_sr = reader.attributes().value("worth_bonus");
    QString base_template = base_template_sr.toString();
    int weight = parseInt(weight_sr.toString(), true);
    int arg1 = parseInt(arg1_sr.toString(), true);
    int arg2 = parseInt(arg2_sr.toString(), true);
    QString arg1_s = arg1_s_sr.toString();
    int rating = parseInt(rating_sr.toString(), true);
    if( rating == 0 )
        rating = 1; // so the default of 0 defaults instead to 1
    bool magical = parseBool(magical_sr.toString(), true);
    int worth_bonus = parseInt(worth_bonus_sr.toString(), true);
    QStringRef name_s = reader.attributes().value("name");
    QStringRef image_name_s = reader.attributes().value("image_name");
    QStringRef icon_width_s = reader.attributes().value("icon_width");

    if( reader.name() == "item" ) {
        QStringRef use_s = reader.attributes().value("use");
        QStringRef use_verb_s = reader.attributes().value("use_verb");
        item = new Item(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight);
        if( use_s.length() > 0 ) {
            item->setUse(use_s.toString().toStdString(), use_verb_s.toString().toStdString());
        }
    }
    else if( reader.name() == "weapon" ) {
        //qDebug("    weapon:");
        QStringRef animation_name_s = reader.attributes().value("animation_name");
        QStringRef two_handed_s = reader.attributes().value("two_handed");
        QStringRef ranged_s = reader.attributes().value("ranged");
        QStringRef thrown_s = reader.attributes().value("thrown");
        QStringRef ammo_s = reader.attributes().value("ammo");
        QStringRef damageX_s = reader.attributes().value("damageX");
        QStringRef damageY_s = reader.attributes().value("damageY");
        QStringRef damageZ_s = reader.attributes().value("damageZ");
        QStringRef min_strength_s = reader.attributes().value("min_strength");
        QStringRef unholy_only_s = reader.attributes().value("unholy_only");
        QStringRef unholy_bonus_s = reader.attributes().value("unholy_bonus");
        /*qDebug("    name: %s", name_s.toString().toStdString().c_str());
        qDebug("    image_name: %s", image_name_s.toString().toStdString().c_str());
        qDebug("    animation_name: %s", animation_name_s.toString().toStdString().c_str());
        qDebug("    weight: %s", weight_s.toString().toStdString().c_str());
        qDebug("    two_handed_s: %s", two_handed_s.toString().toStdString().c_str());
        qDebug("    ranged_s: %s", ranged_s.toString().toStdString().c_str());
        qDebug("    ammo_s: %s", ammo_s.toString().toStdString().c_str());*/
        bool two_handed = parseBool(two_handed_s.toString(), true);
        bool ranged = parseBool(ranged_s.toString(), true);
        bool thrown = parseBool(thrown_s.toString(), true);
        int damageX = parseInt(damageX_s.toString());
        int damageY = parseInt(damageY_s.toString());
        int damageZ = parseInt(damageZ_s.toString());
        int min_strength = parseInt(min_strength_s.toString(), true);
        bool unholy_only = parseBool(unholy_only_s.toString(), true);
        int unholy_bonus = parseInt(unholy_bonus_s.toString(), true);
        Weapon *weapon = new Weapon(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, animation_name_s.toString().toStdString(), damageX, damageY, damageZ);
        item = weapon;
        weapon->setTwoHanded(two_handed);
        if( ranged && thrown ) {
            throw string("Weapons can't' be ranged and thrown");
        }
        else if( ranged ) {
            weapon->setWeaponType(Weapon::WEAPONTYPE_RANGED);
        }
        else if( thrown ) {
            weapon->setWeaponType(Weapon::WEAPONTYPE_THROWN);
        }
        else {
            weapon->setWeaponType(Weapon::WEAPONTYPE_HAND);
        }
        //weapon->setRanged(ranged);
        if( ammo_s.length() > 0 ) {
            weapon->setRequiresAmmo(true, ammo_s.toString().toStdString());
        }
        weapon->setMinStrength(min_strength);
        weapon->setUnholyOnly(unholy_only);
        weapon->setUnholyBonus(unholy_bonus);

        QStringRef weapon_class_s = reader.attributes().value("weapon_class");
        if( weapon_class_s.length() > 0 ) {
            weapon->setWeaponClass(weapon_class_s.toString().toStdString());
        }
    }
    else if( reader.name() == "shield" ) {
        QStringRef animation_name_s = reader.attributes().value("animation_name");
        Shield *shield = new Shield(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, animation_name_s.toString().toStdString());
        item = shield;
    }
    else if( reader.name() == "armour" ) {
        //qDebug("    armour:");
        QStringRef min_strength_s = reader.attributes().value("min_strength");
        int min_strength = parseInt(min_strength_s.toString(), true);
        Armour *armour = new Armour(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, rating);
        item = armour;
        armour->setMinStrength(min_strength);
    }
    else if( reader.name() == "ring" ) {
        //qDebug("    ring:");
        Ring *ring = new Ring(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight);
        item = ring;
    }
    else if( reader.name() == "ammo" ) {
        QStringRef ammo_type_s = reader.attributes().value("ammo_type");
        QStringRef projectile_image_name_s = reader.attributes().value("projectile_image_name");
        QStringRef amount_s = reader.attributes().value("amount");
        int amount = parseInt(amount_s.toString());
        QString ammo_type = ammo_type_s.toString();
        if( ammo_type.length() == 0 ) {
            ammo_type = name_s.toString(); // needed for backwards compatibility
        }
        Ammo *ammo = new Ammo(name_s.toString().toStdString(), image_name_s.toString().toStdString(), ammo_type.toStdString(), projectile_image_name_s.toString().toStdString(), weight, amount);
        item = ammo;
    }
    else if( reader.name() == "currency" ) {
        QStringRef value_s = reader.attributes().value("value");
        Currency *currency = new Currency(name_s.toString().toStdString(), image_name_s.toString().toStdString());
        if( value_s.length() > 0 ) {
            // n.b., when currency is defined as a template, the value is left as 0, and overridden when we instantiate it by the special "gold" element
            int value = parseInt(value_s.toString());
            currency->setValue(value);
        }
        item = currency;
    }
    // else ignore unknown element - leave item as NULL
    if( item != NULL ) {
        if( name_s.length() == 0 ) {
            LOG("error at line %d\n", reader.lineNumber());
            throw string("no name specified for item");
        }
        else if( image_name_s.length() == 0 ) {
            LOG("error at line %d\n", reader.lineNumber());
            throw string("no image name specified for item");
        }

        if( icon_width_s.length() > 0 ) {
            float icon_width = parseFloat(icon_width_s.toString());
            //LOG("icon_width: %f\n", icon_width);
            item->setIconWidth(icon_width);
        }
        item->setArg1(arg1);
        item->setArg2(arg2);
        item->setArg1s(arg1_s.toStdString());
        item->setRating(rating);
        item->setMagical(magical);
        item->setBaseTemplate(base_template.toStdString());
        item->setWorthBonus(worth_bonus);

        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_FP_c);
        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_BS_c);
        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_S_c);
        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_A_c);
        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_M_c);
        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_D_c);
        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_B_c);
        this->parseXMLItemProfileAttributeFloat(item, reader, profile_key_Sp_c);

        // must be done last - will read to the end element
        QString description = reader.readElementText(QXmlStreamReader::IncludeChildElements);
        // get rid of initial whitespace
        while( description.length() > 0 && description.at(0).isSpace() ) {
            description = description.mid(1);
        }
        item->setDescription(description.toStdString());
    }
    return item;
}

void PlayingGamestate::setDifficulty(Difficulty difficulty) {
    LOG("set difficulty to: %d\n", difficulty);
    this->difficulty = difficulty;
}

void PlayingGamestate::moveToLocation(Location *location, Vector2D pos) {
    LOG("PlayingGamestate::moveToLocation(%s, %f, %f)\n", location->getName().c_str(), pos.x, pos.y);
    if( this->player->getLocation() != NULL ) {
        LOG("remove player from location\n");
        this->player->getLocation()->removeCharacter(this->player);
        this->player->setListener(NULL, NULL);
    }
    view->clear();
    this->target_item = NULL;
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        character->setListener(NULL, NULL);
    }
    this->c_location->setListener(NULL, NULL);

    this->c_location = location;
    this->c_location->addCharacter(player, pos.x, pos.y);
    this->c_location->setListener(this, NULL); // must do after creating the location and its contents, so it doesn't try to add items to the scene, etc
    // reset NPCs to their default positions
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        if( character != player && character->hasDefaultPosition() ) {
            //qDebug("### move %s from %f, %f to %f, %f", character->getName().c_str(), character->getX(), character->getY(), character->getDefaultX(), character->getDefaultY());
            character->setPos(character->getDefaultX(), character->getDefaultY());
        }
    }
    this->player->setTargetNPC(NULL);

    this->setupView();
}

void PlayingGamestate::setupView() {
    LOG("PlayingGamestate::setupView()\n");
    // set up the view on the RPG world
    MainWindow *window = game_g->getMainWindow();

    view->createLightingMap(this->c_location->getLightingMin());
    //view->centerOn(player->getPos().x, player->getPos().y);

    const float offset_y = 0.5f;
    float location_width = 0.0f, location_height = 0.0f;
    c_location->calculateSize(&location_width, &location_height);
    const float extra_offset_c = 5.0f; // so we can still scroll slightly past the boundary, and also that multitouch works beyond the boundary
    //scene->setSceneRect(0, -offset_y, location_width, location_height + 2*offset_y);
    scene->setSceneRect(-extra_offset_c, -offset_y-extra_offset_c, location_width+2*extra_offset_c, location_height + 2*offset_y + 2*extra_offset_c);
    view->setInitialScale(window->width(), this->view_transform_3d);

    QBrush floor_brush(builtin_images[c_location->getFloorImageName()]);

    QBrush wall_brush;
    QBrush dropwall_brush;
    float wall_brush_ratio = 1.0f;
    const float dropwall_brush_ratio = 3.0f;
    float wall_scale = 1.0f;
    float dropwall_scale = 1.0f;
    float floor_scale = 4.0f/(float)builtin_images[c_location->getFloorImageName()].width();
    floor_brush.setTransform(QTransform::fromScale(floor_scale, floor_scale));
    if( c_location->getWallImageName().length() > 0 ) {
        wall_brush_ratio = c_location->getWallXScale();
        wall_brush.setTexture(builtin_images[c_location->getWallImageName()]);
        wall_scale = 0.9f/(float)builtin_images[c_location->getWallImageName()].height();
        wall_brush.setTransform(QTransform::fromScale(wall_brush_ratio*wall_scale, wall_scale));
    }
    if( c_location->getDropWallImageName().length() > 0 ) {
        dropwall_brush.setTexture(builtin_images[c_location->getDropWallImageName()]);
        dropwall_scale = 0.9f/(float)builtin_images[c_location->getDropWallImageName()].height();
        dropwall_brush.setTransform(QTransform::fromScale(dropwall_brush_ratio*dropwall_scale, dropwall_scale));
    }

    float background_scale = 1.0f/32.0f;
    QBrush background_brush(builtin_images[c_location->getBackgroundImageName()]);
    background_brush.setTransform(QTransform::fromScale(background_scale, background_scale));
    view->setBackgroundBrush(background_brush);

    for(size_t i=0;i<c_location->getNFloorRegions();i++) {
        FloorRegion *floor_region = c_location->getFloorRegion(i);
        QPolygonF polygon;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D point = floor_region->getPoint(j);
            QPointF qpoint(point.x, point.y);
            polygon.push_back(qpoint);
        }
        QBrush this_floor_brush = floor_brush;
        if( floor_region->getFloorImageName().length() > 0 ) {
            this_floor_brush = QBrush(builtin_images[floor_region->getFloorImageName()]);
            this_floor_brush.setTransform(QTransform::fromScale(floor_scale, floor_scale));
        }
        QGraphicsPolygonItem *item = scene->addPolygon(polygon, Qt::NoPen, this_floor_brush);
        floor_region->setUserGfxData(item);
        item->setVisible(false); // default to false, visibility is checked afterwards
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            if( floor_region->getEdgeType(j) == FloorRegion::EDGETYPE_INTERNAL ) {
                continue;
            }
            size_t n_j = j==floor_region->getNPoints()-1 ? 0 : j+1;
            Vector2D p0 = floor_region->getPoint(j);
            Vector2D p1 = floor_region->getPoint(n_j);
            Vector2D dp = p1 - p0;
            float dp_length = dp.magnitude();
            if( dp_length == 0.0f ) {
                continue;
            }
            dp /= dp_length;
            Vector2D normal_into_wall = - dp.perpendicularYToX();

            if( c_location->getWallImageName().length() > 0 ) {
                if( !this->view_walls_3d || normal_into_wall.y > -E_TOL_LINEAR ) {
                    const float wall_dist = 0.1f;
                    if( fabs(p0.y - p1.y) < E_TOL_LINEAR ) {
                        QGraphicsRectItem *wall_item = new QGraphicsRectItem(std::min(p0.x, p1.x), std::min(p0.y, p0.y - wall_dist), fabs(p1.x - p0.x), wall_dist, item);
                        wall_item->setPen(Qt::NoPen);
                        wall_item->setBrush(wall_brush);
                    }
                    else {
                        QPolygonF wall_polygon;
                        wall_polygon.push_back(QPointF(p0.x, p0.y));
                        wall_polygon.push_back(QPointF(p0.x + wall_dist * normal_into_wall.x, p0.y + wall_dist * normal_into_wall.y));
                        wall_polygon.push_back(QPointF(p1.x + wall_dist * normal_into_wall.x, p1.y + wall_dist * normal_into_wall.y));
                        wall_polygon.push_back(QPointF(p1.x, p1.y));
                        QGraphicsPolygonItem *wall_item = new QGraphicsPolygonItem(wall_polygon, item);
                        wall_item->setPen(Qt::NoPen);
                        wall_item->setBrush(wall_brush);
                    }
                }

                if( this->view_walls_3d ) {
                    const float wall_height = this->view_transform_3d ? 0.9f : 0.5f;
                    if( fabs(normal_into_wall.y) > E_TOL_LINEAR ) {
                        if( normal_into_wall.y < 0.0f )
                        {
                            if( fabs(p0.y - p1.y) < E_TOL_LINEAR ) {
                                QBrush wall_brush_3d = wall_brush;
                                QTransform transform;
                                transform.translate(0.0f, (p1.y - wall_height)/wall_scale);
                                transform *= wall_brush_3d.transform();
                                wall_brush_3d.setTransform(transform);
                                QGraphicsRectItem *wall_item_3d = new QGraphicsRectItem(std::min(p0.x, p1.x), p0.y - wall_height, fabs(p1.x - p0.x), wall_height, item);
                                wall_item_3d->setPen(Qt::NoPen);
                                wall_item_3d->setBrush(wall_brush_3d);
                            }
                            else {
                                QBrush wall_brush_3d = wall_brush;
                                QTransform transform;
                                transform.translate(0.0f, (p1.y - wall_height)/wall_scale - p1.x/wall_scale * (p1.y - p0.y) / (p1.x - p0.x));
                                transform.shear(0.0f, wall_brush_ratio*(p1.y - p0.y) / (p1.x - p0.x));
                                transform *= wall_brush_3d.transform();
                                wall_brush_3d.setTransform(transform);
                                QPolygonF wall_polygon_3d;
                                wall_polygon_3d.push_back(QPointF(p0.x, p0.y));
                                wall_polygon_3d.push_back(QPointF(p0.x, p0.y - wall_height));
                                wall_polygon_3d.push_back(QPointF(p1.x, p1.y - wall_height));
                                wall_polygon_3d.push_back(QPointF(p1.x, p1.y));
                                QGraphicsPolygonItem *wall_item_3d = new QGraphicsPolygonItem(wall_polygon_3d, item);
                                wall_item_3d->setPen(Qt::NoPen);
                                wall_item_3d->setBrush(wall_brush_3d);
                            }
                        }
                    }
                }
            }
            if( c_location->getDropWallImageName().length() > 0 && view_walls_3d ) {
                const float dropwall_height = 0.9f;
                if( normal_into_wall.y > E_TOL_LINEAR ) {
                    QPolygonF dropwall_polygon_3d;
                    QBrush dropwall_brush_3d = dropwall_brush;
                    QTransform transform;
                    transform.translate(0.0f, (p1.y - dropwall_height)/dropwall_scale - p1.x/dropwall_scale * (p1.y - p0.y) / (p1.x - p0.x));
                    transform.shear(0.0f, dropwall_brush_ratio*(p1.y - p0.y) / (p1.x - p0.x));
                    transform *= dropwall_brush_3d.transform();
                    dropwall_brush_3d.setTransform(transform);
                    dropwall_polygon_3d.push_back(QPointF(p0.x, p0.y + dropwall_height));
                    dropwall_polygon_3d.push_back(QPointF(p0.x, p0.y));
                    dropwall_polygon_3d.push_back(QPointF(p1.x, p1.y));
                    dropwall_polygon_3d.push_back(QPointF(p1.x, p1.y + dropwall_height));
                    QGraphicsPolygonItem *dropwall_item_3d = new QGraphicsPolygonItem(dropwall_polygon_3d, item);
                    dropwall_item_3d->setPen(Qt::NoPen);
                    dropwall_item_3d->setBrush(dropwall_brush_3d);
                }
            }
        }
    }

    LOG("add graphics for tilemaps");
    for(size_t i=0;i<c_location->getNTilemaps();i++) {
        const Tilemap *tilemap = c_location->getTilemap(i);
        const string &imagemap = tilemap->getImagemap();
        QPixmap image = builtin_images[imagemap];
        int image_w = image.width();
        int image_h = image.height();
        int tile_width = tilemap->getTileWidth();
        int tile_height = tilemap->getTileHeight();
        qDebug("Tilemap: image: %s w %d h %d tile w %d tile h %d", imagemap.c_str(), image_w, image_h, tile_width, tile_height);
        int n_x = image_w / tile_width;
        int n_y = image_h / tile_height;
        for(int y=0;y<tilemap->getHeighti();y++) {
            for(int x=0;x<tilemap->getWidthi();x++) {
                char ch = tilemap->getTileAt(x, y);
                if( ( ch >= '0' && ch <= '9' ) || ( ch >= 'a' && ch <= 'z' ) ) {
                    int value = 0;
                    int ch_i = (int)ch;
                    if( ch >= '0' && ch <= '9' ) {
                        value = ch_i - (int)'0';
                    }
                    else {
                        value = 10 + ch_i - (int)'a';
                    }
                    if( value >= n_x*n_y ) {
                        LOG("tilemap at %d, %d has value %d, out of bounds for %d x %d\n", x, y, value, n_x, n_y);
                        throw string("tilemap value out of bounds for imagemap");
                    }
                    int pos_x = value % n_x;
                    int pos_y = value / n_x;
                    QPixmap tile = image.copy(pos_x*tile_width, pos_y*tile_height, tile_width, tile_height);
                    //QPixmap tile = image;
                    //qDebug("### %d, %d :")
                    QGraphicsPixmapItem *item = new QGraphicsPixmapItem(tile);
                    item->setPos(tilemap->getX() + x, tilemap->getY() + y);
                    item->setZValue(z_value_tilemap);
                    float item_scale = 1.0f / item->boundingRect().width();
                    item->setScale(item_scale);
                    scene->addItem(item);
                    //this->addGraphicsItem(item, 1.0f);
                }
            }
        }
    }

#ifdef _DEBUG
    this->debug_items.clear(); // clear any from previous view, as no longer valid
    this->refreshDebugItems();
#endif

    LOG("add graphics for items\n");
    for(set<Item *>::iterator iter = c_location->itemsBegin(); iter != c_location->itemsEnd(); ++iter) {
        Item *item = *iter;
        this->locationAddItem(c_location, item, false);
    }
    LOG("add graphics for scenery\n");
    for(set<Scenery *>::iterator iter = c_location->scenerysBegin(); iter != c_location->scenerysEnd(); ++iter) {
        Scenery *scenery = *iter;
        this->locationAddScenery(c_location, scenery);
    }

    LOG("add graphics for characters\n");
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        this->locationAddCharacter(c_location, character);
    }
    view->centreOnPlayer(); // must be done after player character graphic object is created

    /*const int size_c = 256;
    const int alpha = 255;
    QRadialGradient radialGrad(size_c/2, size_c/2, size_c/2);
    radialGrad.setColorAt(0.0, QColor(255, 255, 255, alpha));
    radialGrad.setColorAt(1.0, Qt::transparent);
    //radialGrad.setColorAt(0.0, QColor(0, 0, 0, 0));
    //radialGrad.setColorAt(1.0, QColor(0, 0, 0, alpha));

    QPixmap pixmap(size_c, size_c);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setPen(Qt::NoPen);
    painter.setBrush(radialGrad);
    painter.drawEllipse(0, 0, size_c, size_c);
    painter.end();

    AnimatedObject *object = static_cast<AnimatedObject *>(player->getListenerData());
    //QGraphicsPixmapItem *light_source = scene->addPixmap(pixmap);
    QGraphicsPixmapItem *light_source = new QGraphicsPixmapItem(pixmap);
    light_source->setParentItem(object);
    const float scale_c = 8.0f;
    light_source->setPos(object->getWidth()/2-size_c*scale_c/2, object->getHeight()/2-size_c*scale_c/2);
    light_source->setScale(scale_c);
    //light_source->setOpacity(0.5f);
    light_source->setZValue(10000.0f);*/

    /*{
        TextEffect *text_effect = new TextEffect(tr("Welcome to Erebus"), 1000);
        text_effect->setPos( player->getPos().x, player->getPos().y );
        scene->addItem(text_effect);
    }*/
    //this->addTextEffect(tr("Welcome to Erebus"), player->getPos(), 2000);

    /*SmokeParticleSystem *ps = new SmokeParticleSystem(smoke_pixmap);
    ps->setBirthRate(1.0f);
    ps->setPos(player->getX(), player->getY());
    ps->setScale(0.5f / smoke_pixmap.width());
    ps->setZValue(1000.0f);
    scene->addItem(ps);*/

    LOG("init visibility\n");
    //c_location->initVisibility(player->getPos());
    c_location->updateVisibility(player->getPos());
    // n.b., we look through all the regions rather than just those updated, so that this works when loading a game (as then, other floor regions may have been previously set to be visible)
    for(size_t i=0;i<c_location->getNFloorRegions();i++) {
        FloorRegion *floor_region = c_location->getFloorRegion(i);
        //floor_region->setVisible(true); // test for all regions visible
        if( floor_region->isVisible() ) {
            this->updateVisibilityForFloorRegion(floor_region);
        }
    }
    this->testFogOfWar();

    LOG("done\n");
}

#ifdef _DEBUG
void PlayingGamestate::refreshDebugItems() {
    qDebug("PlayingGamestate::refreshDebugItems()");
    for(vector<QGraphicsItem *>::iterator iter = this->debug_items.begin(); iter != this->debug_items.end(); ++iter) {
        QGraphicsItem *item = *iter;
        delete item;
    }
    this->debug_items.clear();

#ifdef DEBUG_SHOW_PATH
    {
        // DEBUG ONLY
        QPen wall_pen(Qt::red);
        // the pen width is affected by the transformation, but setting 0 forces it to ignore the transformation
        // note that in Qt 4, QPen defaulted to width 0, but in Qt 5 it defaults to width 1, so we set it explicitly
        wall_pen.setWidth(0);
        for(size_t i=0;i<c_location->getNBoundaries();i++) {
            const Polygon2D *boundary = c_location->getBoundary(i);
            //qDebug("boundary %d:", i);
            for(size_t j=0;j<boundary->getNPoints();j++) {
                Vector2D p0 = boundary->getPoint(j);
                Vector2D p1 = boundary->getPoint((j+1) % boundary->getNPoints());
                //scene->addLine(p0.x, p0.y + offset_y, p1.x, p1.y + offset_y, wall_pen);
                //qDebug("    %f, %f to %f, %f", p0.x, p0.y, p1.x, p1.y);
                QGraphicsItem *item = scene->addLine(p0.x, p0.y, p1.x, p1.y, wall_pen);
                item->setZValue(1000.0f);
                this->debug_items.push_back(item);
            }
        }
    }
    {
        // DEBUG ONLY
        QPen wall_pen(Qt::yellow);
        // the pen width is affected by the transformation, but setting 0 forces it to ignore the transformation
        // note that in Qt 4, QPen defaulted to width 0, but in Qt 5 it defaults to width 1, so we set it explicitly
        wall_pen.setWidth(0);
        const Graph *distance_graph = c_location->getDistanceGraph();
        for(size_t i=0;i<distance_graph->getNVertices();i++) {
            const GraphVertex *vertex = distance_graph->getVertex(i);
            Vector2D path_way_point = vertex->getPos();
            const float radius = 0.05f;
            QGraphicsItem *item = scene->addEllipse(path_way_point.x - radius, path_way_point.y - radius, 2.0f*radius, 2.0f*radius, wall_pen);
            this->debug_items.push_back(item);
            // n.b., draws edges twice, but doesn't matter for debug purposes...
            for(size_t j=0;j<vertex->getNNeighbours();j++) {
                const GraphVertex *o_vertex = vertex->getNeighbour(distance_graph, j);
                Vector2D o_path_way_point = o_vertex->getPos();
                float x1 = path_way_point.x;
                float y1 = path_way_point.y;
                float x2 = o_path_way_point.x;
                float y2 = o_path_way_point.y;
                item = scene->addLine(x1, y1, x2, y2, wall_pen);
                this->debug_items.push_back(item);
            }
        }
    }
#endif
}
#endif

Character *PlayingGamestate::loadNPC(bool *is_player, Vector2D *pos, QXmlStreamReader &reader) const {
    qDebug("PlayingGamestate::loadNPC");
    QString attribute_name = reader.name().toString();
    qDebug("attribute_name: %s", attribute_name.toStdString().c_str());

    Character *npc = NULL;
    *is_player = false;
    pos->set(0.0f, 0.0f);

    QStringRef name_s = reader.attributes().value("name");
    QStringRef template_s = reader.attributes().value("template");
    QStringRef pos_x_s = reader.attributes().value("x");
    QStringRef pos_y_s = reader.attributes().value("y");
    if( pos_x_s.length() > 0 && pos_y_s.length() > 0 ) {
        float pos_x = parseFloat(pos_x_s.toString());
        float pos_y = parseFloat(pos_y_s.toString());
        pos->set(pos_x, pos_y);
    }

    QStringRef default_pos_x_s = reader.attributes().value("default_x");
    QStringRef default_pos_y_s = reader.attributes().value("default_y");
    if( template_s.length() > 0 ) {
        if( reader.name() == "player" ) {
            LOG("error at line %d\n", reader.lineNumber());
            throw string("didn't expect player element to load by template");
        }
        // load from template
        npc = this->createCharacter(name_s.toString().toStdString(), template_s.toString().toStdString());
        QStringRef is_hostile_s = reader.attributes().value("is_hostile");
        bool is_hostile = is_hostile_s.length() == 0 ? true : parseBool(is_hostile_s.toString());
        npc->setHostile(is_hostile);
    }
    else {
        if( reader.name() == "player" ) {
            qDebug("player: %s", name_s.toString().toStdString().c_str());
            npc = new Character(name_s.toString().toStdString(), "", false);
            *is_player = true;
        }
        else {
            qDebug("npc: %s", name_s.toString().toStdString().c_str());
            QStringRef animation_name_s = reader.attributes().value("animation_name");
            if( animation_name_s.length() == 0 ) {
                throw string("npc has no animation_name");
            }
            QStringRef static_image_s = reader.attributes().value("static_image");
            bool static_image = parseBool(static_image_s.toString(), true);
            QStringRef bounce_s = reader.attributes().value("bounce");
            bool bounce = parseBool(bounce_s.toString(), true);
            QStringRef is_hostile_s = reader.attributes().value("is_hostile");
            bool is_hostile = is_hostile_s.length() == 0 ? true : parseBool(is_hostile_s.toString());

            npc = new Character(name_s.toString().toStdString(), animation_name_s.toString().toStdString(), true);
            npc->setStaticImage(static_image);
            npc->setBounce(bounce);
            npc->setHostile(is_hostile);

            QStringRef weapon_resist_class_s = reader.attributes().value("weapon_resist_class");
            if( weapon_resist_class_s.length() > 0 ) {
                QStringRef weapon_resist_percentage_s = reader.attributes().value("weapon_resist_percentage");
                int weapon_resist_percentage = parseInt(weapon_resist_percentage_s.toString());
                npc->setWeaponResist(weapon_resist_class_s.toString().toStdString(), weapon_resist_percentage);
            }
        }
        QStringRef regeneration_s = reader.attributes().value("regeneration");
        int regeneration = parseInt(regeneration_s.toString(), true);
        npc->setRegeneration(regeneration);
        QStringRef death_explodes_s = reader.attributes().value("death_explodes");
        bool death_explodes = parseBool(death_explodes_s.toString(), true);
        if( death_explodes ) {
            QStringRef death_explodes_damage_s = reader.attributes().value("death_explodes_damage");
            int death_explodes_damage = parseInt(death_explodes_damage_s.toString());
            npc->setDeathExplodes(death_explodes_damage);
        }
        QStringRef image_size_s = reader.attributes().value("image_size");
        if( image_size_s.length() > 0 ) {
            float image_size = parseFloat(image_size_s.toString());
            npc->setImageSize(image_size);
        }
        QStringRef type_s = reader.attributes().value("type");
        if( type_s.length() > 0 ) {
            npc->setType(type_s.toString().toStdString());
        }
        QStringRef portrait_s = reader.attributes().value("portrait");
        if( portrait_s.length() > 0 ) {
            npc->setPortrait(portrait_s.toString().toStdString());
        }
        QStringRef animation_folder_s = reader.attributes().value("animation_folder");
        if( animation_folder_s.length() > 0 ) {
            npc->setAnimationFolder(animation_folder_s.toString().toStdString());
        }
        QStringRef is_dead_s = reader.attributes().value("is_dead");
        npc->setDead( parseBool(is_dead_s.toString(), true) );
        QStringRef health_s = reader.attributes().value("health");
        QStringRef max_health_s = reader.attributes().value("max_health");
        npc->initialiseHealth( parseInt( max_health_s.toString()) );
        npc->setHealth( parseInt( health_s.toString()) );

        QStringRef FP_s = reader.attributes().value("FP");
        int FP = parseInt(FP_s.toString());
        QStringRef BS_s = reader.attributes().value("BS");
        int BS = parseInt(BS_s.toString());
        QStringRef S_s = reader.attributes().value("S");
        int S = parseInt(S_s.toString());
        QStringRef A_s = reader.attributes().value("A");
        int A = parseInt(A_s.toString());
        QStringRef M_s = reader.attributes().value("M");
        int M = parseInt(M_s.toString());
        QStringRef D_s = reader.attributes().value("D");
        int D = parseInt(D_s.toString());
        QStringRef B_s = reader.attributes().value("B");
        int B = parseInt(B_s.toString());
        QStringRef Sp_s = reader.attributes().value("Sp");
        float Sp = parseFloat(Sp_s.toString());

        QStringRef level_s = reader.attributes().value("level");
        int level = parseInt(level_s.toString(), true);
        QStringRef xp_s = reader.attributes().value("xp");
        int xp = parseInt(xp_s.toString(), true);
        npc->setXP(xp);
        npc->initialiseProfile(level, FP, BS, S, A, M, D, B, Sp);

        QStringRef initial_level_s = reader.attributes().value("initial_level");
        if( initial_level_s.length() > 0 ) {
            int initial_level = parseInt(initial_level_s.toString());
            QStringRef initial_FP_s = reader.attributes().value("initial_FP");
            int initial_FP = parseInt(initial_FP_s.toString());
            QStringRef initial_BS_s = reader.attributes().value("initial_BS");
            int initial_BS = parseInt(initial_BS_s.toString());
            QStringRef initial_S_s = reader.attributes().value("initial_S");
            int initial_S = parseInt(initial_S_s.toString());
            QStringRef initial_A_s = reader.attributes().value("initial_A");
            int initial_A = parseInt(initial_A_s.toString());
            QStringRef initial_M_s = reader.attributes().value("initial_M");
            int initial_M = parseInt(initial_M_s.toString());
            QStringRef initial_D_s = reader.attributes().value("initial_D");
            int initial_D = parseInt(initial_D_s.toString());
            QStringRef initial_B_s = reader.attributes().value("initial_B");
            int initial_B = parseInt(initial_B_s.toString());
            QStringRef initial_Sp_s = reader.attributes().value("initial_Sp");
            float initial_Sp = parseFloat(initial_Sp_s.toString());
            npc->setInitialProfile(initial_level, initial_FP, initial_BS, initial_S, initial_A, initial_M, initial_D, initial_B, initial_Sp);
        }

        QStringRef natural_damageX_s = reader.attributes().value("natural_damageX");
        QStringRef natural_damageY_s = reader.attributes().value("natural_damageY");
        QStringRef natural_damageZ_s = reader.attributes().value("natural_damageZ");
        if( natural_damageX_s.length() > 0 || natural_damageY_s.length() > 0 || natural_damageZ_s.length() > 0 ) {
            int natural_damageX = parseInt(natural_damageX_s.toString());
            int natural_damageY = parseInt(natural_damageY_s.toString());
            int natural_damageZ = parseInt(natural_damageZ_s.toString());
            npc->setNaturalDamage(natural_damageX, natural_damageY, natural_damageZ);
        }
        QStringRef can_fly_s = reader.attributes().value("can_fly");
        bool can_fly = parseBool(can_fly_s.toString(), true);
        npc->setCanFly(can_fly);
        QStringRef is_paralysed_s = reader.attributes().value("is_paralysed");
        bool is_paralysed = parseBool(is_paralysed_s.toString(), true);
        if( is_paralysed ) {
            QStringRef paralysed_time_s = reader.attributes().value("paralysed_time");
            int paralysed_time = parseInt(paralysed_time_s.toString());
            npc->paralyse(paralysed_time);
        }
        QStringRef is_diseased_s = reader.attributes().value("is_diseased");
        bool is_diseased = parseBool(is_diseased_s.toString(), true);
        npc->setDiseased(is_diseased);
        QStringRef xp_worth_s = reader.attributes().value("xp_worth");
        int xp_worth = parseInt(xp_worth_s.toString());
        npc->setXPWorth(xp_worth);
        QStringRef causes_terror_s = reader.attributes().value("causes_terror");
        bool causes_terror = parseBool(causes_terror_s.toString(), true);
        if( causes_terror ) {
            QStringRef terror_effect_s = reader.attributes().value("terror_effect");
            int terror_effect = parseInt(terror_effect_s.toString());
            npc->setCausesTerror(terror_effect);
        }
        QStringRef done_terror_s = reader.attributes().value("done_terror");
        bool done_terror = parseBool(done_terror_s.toString(), true);
        npc->setDoneTerror(done_terror);
        QStringRef is_fleeing_s = reader.attributes().value("is_fleeing");
        bool is_fleeing = parseBool(is_fleeing_s.toString(), true);
        npc->setFleeing(is_fleeing);
        QStringRef causes_disease_s = reader.attributes().value("causes_disease");
        int causes_disease = parseInt(causes_disease_s.toString(), true);
        npc->setCausesDisease(causes_disease);
        QStringRef causes_paralysis_s = reader.attributes().value("causes_paralysis");
        int causes_paralysis = parseInt(causes_paralysis_s.toString(), true);
        npc->setCausesParalysis(causes_paralysis);
        QStringRef requires_magical_s = reader.attributes().value("requires_magical");
        bool requires_magical = parseBool(requires_magical_s.toString(), true);
        npc->setRequiresMagical(requires_magical);
        QStringRef unholy_s = reader.attributes().value("unholy");
        bool unholy = parseBool(unholy_s.toString(), true);
        npc->setUnholy(unholy);
        QStringRef gold_s = reader.attributes().value("gold");
        npc->addGold( parseInt( gold_s.toString(), true) );
    }

    QStringRef can_talk_s = reader.attributes().value("can_talk");
    if( can_talk_s.length() > 0 ) {
        bool can_talk = parseBool(can_talk_s.toString());
        npc->setCanTalk(can_talk);
    }
    QStringRef has_talked_s = reader.attributes().value("has_talked");
    if( has_talked_s.length() > 0 ) {
        bool has_talked = parseBool(has_talked_s.toString());
        npc->setHasTalked(has_talked);
    }
    QStringRef interaction_completed_s = reader.attributes().value("interaction_completed");
    if( interaction_completed_s.length() > 0 ) {
        bool interaction_completed = parseBool(interaction_completed_s.toString());
        npc->setInteractionCompleted(interaction_completed);
    }
    QStringRef interaction_type_s = reader.attributes().value("interaction_type");
    npc->setInteractionType(interaction_type_s.toString().toStdString());
    QStringRef interaction_data_s = reader.attributes().value("interaction_data");
    npc->setInteractionData(interaction_data_s.toString().toStdString());
    QStringRef interaction_xp_s = reader.attributes().value("interaction_xp");
    int interaction_xp = parseInt(interaction_xp_s.toString(), true);
    npc->setInteractionXP(interaction_xp);
    QStringRef interaction_reward_item_s = reader.attributes().value("interaction_reward_item");
    npc->setInteractionRewardItem(interaction_reward_item_s.toString().toStdString());
    QStringRef interaction_reward_gold_s = reader.attributes().value("interaction_reward_gold");
    if( interaction_reward_gold_s.length() > 0 ) {
        int interaction_reward_gold = parseInt(interaction_reward_gold_s.toString());
        npc->setInteractionRewardGold(interaction_reward_gold);
    }
    QStringRef interaction_journal_s = reader.attributes().value("interaction_journal");
    npc->setInteractionJournal(interaction_journal_s.toString().toStdString());
    QStringRef interaction_set_flag_s = reader.attributes().value("interaction_set_flag");
    npc->setInteractionSetFlag(interaction_set_flag_s.toString().toStdString());
    QStringRef shop_s = reader.attributes().value("shop");
    if( shop_s.length() > 0 ) {
        npc->setShop(shop_s.toString().toStdString());
    }
    QStringRef objective_id_s = reader.attributes().value("objective_id");
    if( objective_id_s.length() > 0 ) {
        npc->setObjectiveId(objective_id_s.toString().toStdString());
    }

    if( default_pos_x_s.length() > 0 && default_pos_y_s.length() > 0 ) {
        float default_pos_x = parseFloat(default_pos_x_s.toString());
        float default_pos_y = parseFloat(default_pos_y_s.toString());
        npc->setDefaultPosition(default_pos_x, default_pos_y);
    }

    // now read remaining elements
    while( !reader.atEnd() && !reader.hasError() ) {
        reader.readNext();
        //qDebug("read %d element: %s", reader.tokenType(), reader.name().toString().toStdString().c_str());
        if( reader.isStartElement() ) {
            if( reader.name() == "skill" ) {
                QStringRef name_s = reader.attributes().value("name");
                npc->addSkill(name_s.toString().toStdString());
            }
            else if( reader.name() == "opening_initial" ) {
                QString text = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                npc->setTalkOpeningInitial(text.toStdString());
            }
            else if( reader.name() == "opening_later") {
                QString text = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                npc->setTalkOpeningLater(text.toStdString());
            }
            else if( reader.name() == "opening_interaction_complete" ) {
                QString text = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                npc->setTalkOpeningInteractionComplete(text.toStdString());
            }
            else if( reader.name() == "talk" ) {
                QStringRef question_s = reader.attributes().value("question");
                QString question = question_s.toString();
                QStringRef action_s = reader.attributes().value("action");
                QString action = action_s.toString();
                QStringRef journal_s = reader.attributes().value("journal");
                QString journal = journal_s.toString();
                QStringRef while_not_done_s = reader.attributes().value("while_not_done");
                bool while_not_done = parseBool(while_not_done_s.toString(), true);
                QStringRef objective_s = reader.attributes().value("objective");
                bool objective = parseBool(objective_s.toString(), true);
                QString answer = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                npc->addTalkItem(question.toStdString(), answer.toStdString(), action.toStdString(), journal.toStdString(), while_not_done, objective);
            }
            else if( reader.name() == "spell" ) {
                QStringRef name_s = reader.attributes().value("name");
                QStringRef count_s = reader.attributes().value("count");
                int count = parseInt(count_s.toString());
                npc->addSpell(name_s.toString().toStdString(), count);
            }
            else if( reader.name() == "item" || reader.name() == "weapon" || reader.name() == "shield" || reader.name() == "armour" || reader.name() == "ring" || reader.name() == "ammo" || reader.name() == "currency" || reader.name() == "gold" ) {
                Vector2D pos;
                this->loadItem(&pos, reader, NULL, npc, false);
            }
        }
        else if( reader.isEndElement() ) {
            if( reader.name() == attribute_name ) {
                break;
            }
        }
    }

    return npc;
}

Item *PlayingGamestate::loadItem(Vector2D *pos, QXmlStreamReader &reader, Scenery *scenery, Character *npc, bool start_bonus_item) const {
    qDebug("PlayingGamestate::loadItem");
    QString attribute_name = reader.name().toString();
    qDebug("attribute_name: %s", attribute_name.toStdString().c_str());

    Item *item = NULL;
    pos->set(0.0f, 0.0f);

    if( scenery != NULL && npc != NULL ) {
        LOG("error at line %d\n", reader.lineNumber());
        throw string("loadItem called with both scenery and npc specified");
    }

    // need to read everything we want from the reader, before calling parseXMLItem.
    QStringRef template_s = reader.attributes().value("template");
    bool current_weapon = false, current_ammo = false, current_shield = false, current_armour = false, current_ring = false;
    if( scenery != NULL ) {
    }
    else if( npc != NULL ) {
        QStringRef current_weapon_s = reader.attributes().value("current_weapon");
        current_weapon = parseBool(current_weapon_s.toString(), true);
        QStringRef current_ammo_s = reader.attributes().value("current_ammo");
        current_ammo = parseBool(current_ammo_s.toString(), true);
        QStringRef current_shield_s = reader.attributes().value("current_shield");
        current_shield = parseBool(current_shield_s.toString(), true);
        QStringRef current_armour_s = reader.attributes().value("current_armour");
        current_armour = parseBool(current_armour_s.toString(), true);
        QStringRef current_ring_s = reader.attributes().value("current_ring");
        current_ring = parseBool(current_ring_s.toString(), true);
    }
    else if( start_bonus_item ) {
    }
    else {
        QStringRef pos_x_s = reader.attributes().value("x");
        float pos_x = parseFloat(pos_x_s.toString());
        QStringRef pos_y_s = reader.attributes().value("y");
        float pos_y = parseFloat(pos_y_s.toString());
        pos->set(pos_x, pos_y);
    }

    if( reader.name() == "gold" ) {
        // special case
        qDebug("gold");
        QStringRef amount_s = reader.attributes().value("amount");
        int amount = parseInt(amount_s.toString());
        item = this->cloneGoldItem(amount);
        // read until end tag
        reader.readElementText();
    }
    else if( template_s.length() > 0 ) {
        // load from template
        qDebug("load item from template");
        if( reader.name() != "item" ) {
            LOG("error at line %d\n", reader.lineNumber());
            throw string("only allowed item element type when loading as template");
        }
        item = this->cloneStandardItem(template_s.toString().toStdString());
        if( item == NULL ) {
            LOG("error at line %d\n", reader.lineNumber());
            LOG("can't find item: %s\n", template_s.toString().toStdString().c_str());
            throw string("can't find item");
        }
        // read until end tag
        reader.readElementText();
    }
    else {
        qDebug("load item");
        item = parseXMLItem(reader); // n.b., reader will advance to end element!
        if( item == NULL ) {
            LOG("error at line %d\n", reader.lineNumber());
            throw string("unknown item element");
        }
    }

    if( scenery != NULL ) {
        scenery->addItem(item);
    }
    else if( npc != NULL ) {
        npc->addItem(item, npc != this->player);
        if( current_weapon ) {
            if( item->getType() != ITEMTYPE_WEAPON ) {
                LOG("error at line %d\n", reader.lineNumber());
                throw string("current_weapon is not a weapon");
            }
            Weapon *weapon = static_cast<Weapon *>(item);
            npc->armWeapon(weapon);
        }
        else if( current_ammo ) {
            if( item->getType() != ITEMTYPE_AMMO ) {
                LOG("error at line %d\n", reader.lineNumber());
                throw string("current_weapon is not an ammo");
            }
            Ammo *ammo = static_cast<Ammo *>(item);
            npc->selectAmmo(ammo);
        }
        else if( current_shield ) {
            if( item->getType() != ITEMTYPE_SHIELD ) {
                LOG("error at line %d\n", reader.lineNumber());
                throw string("current_shield is not a shield");
            }
            Shield *shield = static_cast<Shield *>(item);
            npc->armShield(shield);
        }
        else if( current_armour ) {
            if( item->getType() != ITEMTYPE_ARMOUR ) {
                LOG("error at line %d\n", reader.lineNumber());
                throw string("current_armour is not an armour");
            }
            Armour *armour = static_cast<Armour *>(item);
            npc->wearArmour(armour);
        }
        else if( current_ring ) {
            if( item->getType() != ITEMTYPE_RING ) {
                LOG("error at line %d\n", reader.lineNumber());
                throw string("current_ring is not a ring");
            }
            Ring *ring = static_cast<Ring *>(item);
            npc->wearRing(ring);
        }
    }

    return item;
}

Scenery *PlayingGamestate::loadScenery(QXmlStreamReader &reader) const {
    qDebug("PlayingGamestate::loadScenery");
    QString attribute_name = reader.name().toString();
    qDebug("attribute_name: %s", attribute_name.toStdString().c_str());

    QStringRef name_s = reader.attributes().value("name");
    QStringRef image_name_s = reader.attributes().value("image_name");
    QStringRef big_image_name_s = reader.attributes().value("big_image_name");
    QStringRef opacity_s = reader.attributes().value("opacity");
    QStringRef has_smoke_s = reader.attributes().value("has_smoke");
    QStringRef smoke_x_s = reader.attributes().value("smoke_x");
    QStringRef smoke_y_s = reader.attributes().value("smoke_y");
    QStringRef draw_type_s = reader.attributes().value("draw_type");
    //QStringRef action_last_time_s = reader.attributes().value("action_last_time");
    QStringRef action_delay_s = reader.attributes().value("action_delay");
    QStringRef action_type_s = reader.attributes().value("action_type");
    QStringRef action_value_s = reader.attributes().value("action_value");
    QStringRef interact_type_s = reader.attributes().value("interact_type");
    QStringRef interact_state_s = reader.attributes().value("interact_state");
    QStringRef requires_flag_s = reader.attributes().value("requires_flag");
    QStringRef blocking_s = reader.attributes().value("blocking");
    bool blocking = parseBool(blocking_s.toString(), true);
    QStringRef block_visibility_s = reader.attributes().value("block_visibility");
    bool block_visibility = parseBool(block_visibility_s.toString(), true);
    QStringRef is_opened_s = reader.attributes().value("is_opened");
    bool is_opened = parseBool(is_opened_s.toString(), true);
    QStringRef door_s = reader.attributes().value("door");
    bool door = parseBool(door_s.toString(), true);
    QStringRef exit_s = reader.attributes().value("exit");
    bool exit = parseBool(exit_s.toString(), true);
    QStringRef exit_location_s = reader.attributes().value("exit_location");
    QStringRef locked_s = reader.attributes().value("locked");
    bool locked = parseBool(locked_s.toString(), true);
    QStringRef locked_silent_s = reader.attributes().value("locked_silent");
    bool locked_silent = parseBool(locked_silent_s.toString(), true);
    QStringRef locked_text_s = reader.attributes().value("locked_text");
    QStringRef locked_used_up_s = reader.attributes().value("locked_used_up");
    bool locked_used_up = parseBool(locked_used_up_s.toString(), true);
    QStringRef key_always_needed_s = reader.attributes().value("key_always_needed");
    bool key_always_needed = parseBool(key_always_needed_s.toString(), true);
    QStringRef unlock_item_name_s = reader.attributes().value("unlocked_by_template");
    QStringRef unlock_text_s = reader.attributes().value("unlock_text");
    QStringRef unlock_xp_s = reader.attributes().value("unlock_xp");
    QStringRef confirm_text_s = reader.attributes().value("confirm_text");
    QStringRef size_s = reader.attributes().value("size");

    bool has_size = false;
    float size = 0.0f, size_w = 0.0f, size_h = 0.0f;
    if( size_s.length() > 0 ) {
        has_size = true;
        size = parseFloat(size_s.toString());
    }
    else {
        QStringRef size_w_s = reader.attributes().value("w");
        QStringRef size_h_s = reader.attributes().value("h");
        size_w = parseFloat(size_w_s.toString());
        size_h = parseFloat(size_h_s.toString());
    }
    bool has_visual_h = false;
    float visual_h = 0.0f;
    QStringRef visual_h_s = reader.attributes().value("visual_h");
    if( visual_h_s.length() > 0 ) {
        has_visual_h = true;
        visual_h = parseFloat(visual_h_s.toString());
    }
    bool boundary_iso = false;
    float boundary_iso_ratio = 0.0f;
    QStringRef boundary_iso_s = reader.attributes().value("boundary_iso");
    if( boundary_iso_s.length() > 0 ) {
        boundary_iso = parseBool(boundary_iso_s.toString());
        QStringRef boundary_iso_ratio_s = reader.attributes().value("boundary_iso_ratio");
        boundary_iso_ratio = parseFloat(boundary_iso_ratio_s.toString());
    }

    this->querySceneryImage(&size_w, &size_h, &visual_h, image_name_s.toString().toStdString(), has_size, size, size_w, size_h, has_visual_h, visual_h);

    Scenery *scenery = new Scenery(name_s.toString().toStdString(), image_name_s.toString().toStdString(), size_w, size_h, visual_h, boundary_iso, boundary_iso_ratio);

    if( door && exit ) {
        LOG("error at line %d\n", reader.lineNumber());
        throw string("scenery can't be both a door and an exit");
    }
    else if( exit && exit_location_s.length() > 0 ) {
        LOG("error at line %d\n", reader.lineNumber());
        throw string("scenery can't be both an exit and an exit_location");
    }
    else if( exit_location_s.length() > 0 && door ) {
        LOG("error at line %d\n", reader.lineNumber());
        throw string("scenery can't be both an exit_location and a door");
    }

    map<string, LazyAnimationLayer *>::const_iterator animation_iter = this->scenery_animation_layers.find(image_name_s.toString().toStdString());
    if( animation_iter == this->scenery_animation_layers.end() ) {
        LOG("failed to find image for scenery: %s\n", name_s.toString().toStdString().c_str());
        LOG("    image name: %s\n", image_name_s.toString().toStdString().c_str());
        throw string("Failed to find scenery's image");
    }
    const AnimationLayer *animation_layer = animation_iter->second->getAnimationLayer();
    if( animation_layer->getAnimationSet("opened") != NULL ) {
        scenery->setCanBeOpened(true);
    }

    if( big_image_name_s.length() > 0 ) {
        scenery->setBigImageName(big_image_name_s.toString().toStdString());
    }
    scenery->setBlocking(blocking, block_visibility);
    if( is_opened && !scenery->canBeOpened() ) {
        qDebug("trying to set is_opened on scenery that can't be opened: %s at %f, %f", scenery->getName().c_str(), scenery->getX(), scenery->getY());
    }
    scenery->setOpened(is_opened);
    if( opacity_s.length() > 0 ) {
        float opacity = parseFloat(opacity_s.toString());
        scenery->setOpacity(opacity);
    }
    if( has_smoke_s.length() > 0 ) {
        bool has_smoke = parseBool(has_smoke_s.toString());
        // we allow a default for backwards compatibility, for when the smoke positio was hardcoded for the campfire
        Vector2D smoke_pos(0.5f, 0.4167f);
        if( smoke_x_s.length() > 0 ) {
            smoke_pos.x = parseFloat(smoke_x_s.toString());
        }
        if( smoke_y_s.length() > 0 ) {
            smoke_pos.y = parseFloat(smoke_y_s.toString());
        }
        scenery->setHasSmoke(has_smoke, smoke_pos);
    }
    if( draw_type_s.length() > 0 ) {
        if( draw_type_s == "floating" ) {
            scenery->setDrawType(Scenery::DRAWTYPE_FLOATING);
        }
        else if( draw_type_s == "background" ) {
            scenery->setDrawType(Scenery::DRAWTYPE_BACKGROUND);
        }
        else {
            LOG("unrecognised draw_type: %s\n", draw_type_s.toString().toStdString().c_str());
            LOG("error at line %d\n", reader.lineNumber());
            throw string("unrecognised draw_type for scenery");
        }
    }
    // not saved:
    /*if( action_last_time_s.length() > 0 ) {
        int action_last_time = parseInt(action_last_time_s.toString());
        scenery->setActionLastTime(action_last_time);
    }*/
    if( action_delay_s.length() > 0 ) {
        int action_delay = parseInt(action_delay_s.toString());
        scenery->setActionDelay(action_delay);
    }
    if( action_type_s.length() > 0 ) {
        scenery->setActionType(action_type_s.toString().toStdString());
    }
    if( action_value_s.length() > 0 ) {
        int action_value = parseInt(action_value_s.toString());
        scenery->setActionValue(action_value);
    }
    if( interact_type_s.length() > 0 ) {
        scenery->setInteractType(interact_type_s.toString().toStdString());
    }
    if( interact_state_s.length() > 0 ) {
        int interact_state = parseInt(interact_state_s.toString());
        scenery->setInteractState(interact_state);
    }
    if( requires_flag_s.length() > 0 ) {
        scenery->setRequiresFlag(requires_flag_s.toString().toStdString());
    }
    scenery->setDoor(door);
    scenery->setExit(exit);
    if( exit_location_s.length() > 0 ) {
        QStringRef exit_location_x_s = reader.attributes().value("exit_location_x");
        float exit_location_x = parseFloat(exit_location_x_s.toString());
        QStringRef exit_location_y_s = reader.attributes().value("exit_location_y");
        float exit_location_y = parseFloat(exit_location_y_s.toString());
        QStringRef exit_travel_time_s = reader.attributes().value("exit_travel_time");
        int exit_travel_time = parseInt(exit_travel_time_s.toString(), true);
        scenery->setExitLocation(exit_location_s.toString().toStdString(), Vector2D(exit_location_x, exit_location_y), exit_travel_time);
    }
    scenery->setLocked(locked);
    scenery->setLockedSilent(locked_silent);
    if( locked_text_s.length() > 0 ) {
        scenery->setLockedText(locked_text_s.toString().toStdString());
    }
    scenery->setLockedUsedUp(locked_used_up);
    scenery->setKeyAlwaysNeeded(key_always_needed);
    if( unlock_item_name_s.length() > 0 ) {
        scenery->setUnlockItemName(unlock_item_name_s.toString().toStdString());
    }
    if( unlock_text_s.length() > 0 ) {
        scenery->setUnlockText(unlock_text_s.toString().toStdString());
    }

    if( unlock_xp_s.length() > 0 ) {
        int unlock_xp = parseInt(unlock_xp_s.toString());
        scenery->setUnlockXP(unlock_xp);
    }
    if( confirm_text_s.length() > 0 ) {
        scenery->setConfirmText(confirm_text_s.toString().toStdString());
    }

    // now read remaining elements
    while( !reader.atEnd() && !reader.hasError() ) {
        reader.readNext();
        //qDebug("read %d element: %s", reader.tokenType(), reader.name().toString().toStdString().c_str());
        if( reader.isStartElement() ) {
            if( reader.name() == "item" || reader.name() == "weapon" || reader.name() == "shield" || reader.name() == "armour" || reader.name() == "ring" || reader.name() == "ammo" || reader.name() == "currency" || reader.name() == "gold" ) {
                Vector2D pos;
                this->loadItem(&pos, reader, scenery, NULL, false);
            }
            else if( reader.name() == "trap" ) {
                Trap *trap = loadTrap(reader);
                scenery->setTrap(trap);
            }
        }
        else if( reader.isEndElement() ) {
            if( reader.name() == attribute_name ) {
                break;
            }
        }
        else {
            QStringRef text = reader.text();
            if( stringAnyNonWhitespace(text.toString().toStdString()) ) {
                //qDebug("### : %s", text.toString().toStdString().c_str());
                scenery->setDescription(text.toString().toStdString());
            }
        }
    }

    return scenery;
}

Trap *PlayingGamestate::loadTrap(QXmlStreamReader &reader) const {
    qDebug("PlayingGamestate::loadTrap");
    QStringRef type_s = reader.attributes().value("type");
    QStringRef rating_s = reader.attributes().value("rating");
    int rating = parseInt(rating_s.toString(), true);
    QStringRef difficulty_s = reader.attributes().value("difficulty");
    int difficulty = parseInt(difficulty_s.toString(), true);

    Trap *trap = new Trap(type_s.toString().toStdString());
    trap->setRating(rating);
    trap->setDifficulty(difficulty);

    // read until end tag
    reader.readElementText();

    return trap;
}

FloorRegion *PlayingGamestate::loadFloorRegion(QXmlStreamReader &reader) const {
    qDebug("PlayingGamestate::loadFloorRegion");
    QString attribute_name = reader.name().toString();
    qDebug("attribute_name: %s", attribute_name.toStdString().c_str());

    FloorRegion *floor_region = NULL;

    bool is_polygon = false;
    QStringRef shape_s = reader.attributes().value("shape");
    if( shape_s == "rect" ) {
        QStringRef rect_x_s = reader.attributes().value("rect_x");
        float rect_x = parseFloat(rect_x_s.toString());
        QStringRef rect_y_s = reader.attributes().value("rect_y");
        float rect_y = parseFloat(rect_y_s.toString());
        QStringRef rect_w_s = reader.attributes().value("rect_w");
        float rect_w = parseFloat(rect_w_s.toString());
        QStringRef rect_h_s = reader.attributes().value("rect_h");
        float rect_h = parseFloat(rect_h_s.toString());
        //qDebug("found floor region: %f, %f, %f, %f\n", rect_x, rect_y, rect_w, rect_h);
        floor_region = FloorRegion::createRectangle(rect_x, rect_y, rect_w, rect_h);
        // will be added to location when doing the end floorregion element
    }
    else if( shape_s == "polygon" ) {
        is_polygon = true;
        floor_region = new FloorRegion();
    }
    else {
        LOG("error at line %d\n", reader.lineNumber());
        throw string("floorregion has unknown shape");
    }
    QStringRef visible_s = reader.attributes().value("visible");
    if( visible_s.length() > 0 ) {
        bool visible = parseBool(visible_s.toString());
        floor_region->setVisible(visible);
    }
    QStringRef image_name_s = reader.attributes().value("image_name");
    if( image_name_s.length() > 0 ) {
        floor_region->setFloorImageName(image_name_s.toString().toStdString());
    }

    // now read remaining elements
    while( !reader.atEnd() && !reader.hasError() ) {
        reader.readNext();
        //qDebug("read %d element: %s", reader.tokenType(), reader.name().toString().toStdString().c_str());
        if( reader.isStartElement() ) {
            if( reader.name() == "floorregion_point" ) {
                if( !is_polygon ) {
                    LOG("error at line %d\n", reader.lineNumber());
                    throw string("floorregion_point not supported for this shape");
                }
                QStringRef x_s = reader.attributes().value("x");
                float x = parseFloat(x_s.toString());
                QStringRef y_s = reader.attributes().value("y");
                float y = parseFloat(y_s.toString());
                floor_region->addPoint(Vector2D(x, y));
            }
        }
        else if( reader.isEndElement() ) {
            if( reader.name() == attribute_name ) {
                if( floor_region->getNPoints() < 3 ) {
                    LOG("floorregion only has %d points\n", floor_region->getNPoints());
                    LOG("error at line %d\n", reader.lineNumber());
                    throw string("floorregion has insufficient points");
                }
                break;
            }
        }
    }

    return floor_region;
}

void PlayingGamestate::loadStartBonus(QXmlStreamReader &reader, bool cheat_mode) const {
    qDebug("PlayingGamestate::loadStartBonus");
    QString attribute_name = reader.name().toString();
    qDebug("attribute_name: %s", attribute_name.toStdString().c_str());

    if( cheat_mode ) {
        QStringRef FP_s = reader.attributes().value("FP");
        int FP = parseInt(FP_s.toString());
        QStringRef BS_s = reader.attributes().value("BS");
        int BS = parseInt(BS_s.toString());
        QStringRef S_s = reader.attributes().value("S");
        int S = parseInt(S_s.toString());
        QStringRef A_s = reader.attributes().value("A");
        int A = parseInt(A_s.toString());
        QStringRef M_s = reader.attributes().value("M");
        int M = parseInt(M_s.toString());
        QStringRef D_s = reader.attributes().value("D");
        int D = parseInt(D_s.toString());
        QStringRef B_s = reader.attributes().value("B");
        int B = parseInt(B_s.toString());
        QStringRef Sp_s = reader.attributes().value("Sp");
        float Sp = parseFloat(Sp_s.toString());
        player->addProfile(FP, BS, S, A, M, D, B, Sp);

        QStringRef bonus_health_s = reader.attributes().value("bonus_health");
        int bonus_health = parseInt(bonus_health_s.toString());
        player->increaseMaxHealth(bonus_health);

        QStringRef XP_s = reader.attributes().value("XP");
        int XP = parseInt(XP_s.toString());
        player->setXP(XP);
        for(;;) {
            int next_level_xp = this->player->getXPForNextLevel();
            //qDebug("compare: %d vs %d", this->player->getXP(), next_level_xp);
            if( this->player->getXP() >= next_level_xp ) {
                // don't call advanceLevel(), as we set the increased profile and health directly
                this->player->setLevel( this->player->getLevel()+1 );
            }
            else {
                break;
            }
        }
    }

    // now read remaining elements
    while( !reader.atEnd() && !reader.hasError() ) {
        reader.readNext();
        //qDebug("read %d element: %s", reader.tokenType(), reader.name().toString().toStdString().c_str());
        if( reader.isStartElement() ) {
            if( reader.name() == "item" || reader.name() == "weapon" || reader.name() == "shield" || reader.name() == "armour" || reader.name() == "ring" || reader.name() == "ammo" || reader.name() == "currency" || reader.name() == "gold" ) {
                Vector2D pos;
                if( cheat_mode ) {
                    Item *item = this->loadItem(&pos, reader, NULL, NULL, true);
                    bool item_is_useful = true;
                    if( item->getType() == ITEMTYPE_WEAPON ) {
                        if( player->tooWeakForWeapon(static_cast<Weapon *>(item)) ) {
                            item_is_useful = false;
                        }
                        else if( player->getCurrentWeapon() != NULL && !static_cast<Weapon *>(item)->isRangedOrThrown() && player->getCurrentWeapon()->getDamageScore() < static_cast<Weapon *>(item)->getDamageScore() ) {
                            player->armWeapon(NULL);
                            if( static_cast<Weapon *>(item)->isTwoHanded() ) {
                                player->armShield(NULL);
                            }
                        }
                    }
                    else if( item->getType() == ITEMTYPE_ARMOUR ) {
                        if( player->tooWeakForArmour(static_cast<Armour *>(item)) ) {
                            item_is_useful = false;
                        }
                        else if( player->getCurrentArmour() != NULL && player->getCurrentArmour()->getRating() < static_cast<Armour *>(item)->getRating() ) {
                            player->wearArmour(NULL);
                        }
                    }
                    if( item_is_useful ) {
                        player->addItem(item, true);

                        while( player->carryingTooMuch() ) {
                            Item *lose_item = NULL;
                            for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
                                Item *item = *iter;
                                if( item->getType() == ITEMTYPE_ARMOUR && item != player->getCurrentArmour() ) {
                                    lose_item = item;
                                    break;
                                }
                            }
                            if( lose_item ) {
                                player->takeItem(lose_item);
                                delete lose_item;
                            }
                            else {
                                break;
                            }
                        }
                    }
                    else {
                        delete item;
                    }
                }
            }
        }
        else if( reader.isEndElement() ) {
            if( reader.name() == attribute_name ) {
                break;
            }
        }
    }
}

vector<RandomScenery> PlayingGamestate::loadRandomScenery(QXmlStreamReader &reader) const {
    qDebug("PlayingGamestate::loadRandomScenery");
    QString attribute_name = reader.name().toString();
    qDebug("attribute_name: %s", attribute_name.toStdString().c_str());

    vector<RandomScenery> random_scenery;

    // now read remaining elements
    while( !reader.atEnd() && !reader.hasError() ) {
        reader.readNext();
        //qDebug("read %d element: %s", reader.tokenType(), reader.name().toString().toStdString().c_str());
        if( reader.isStartElement() ) {
            if( reader.name() == "scenery" ) {
                QStringRef density_s = reader.attributes().value("density");
                float density = parseFloat(density_s.toString());
                Scenery *scenery = loadScenery(reader);
                random_scenery.push_back(RandomScenery(scenery, density));
                // we'll actually convert the random list into actual scenery after the location is loaded (in case the floor regions haven't been fully defined yet)
            }
        }
        else if( reader.isEndElement() ) {
            if( reader.name() == attribute_name ) {
                break;
            }
        }
    }

    return random_scenery;
}

/*XXX *PlayingGamestate::loadXXX(QXmlStreamReader &reader) const {
    qDebug("PlayingGamestate::loadXXX");
    QString attribute_name = reader.name().toString();
    qDebug("attribute_name: %s", attribute_name.toStdString().c_str());

    XXX *xxx = NULL;

    // now read remaining elements
    while( !reader.atEnd() && !reader.hasError() ) {
        reader.readNext();
        //qDebug("read %d element: %s", reader.tokenType(), reader.name().toString().toStdString().c_str());
        if( reader.isStartElement() ) {
            if( reader.name() == "" ) {
            }
        }
        else if( reader.isEndElement() ) {
            if( reader.name() == attribute_name ) {
                break;
            }
        }
    }
    // read until end tag
    //reader.readElementText();

    return xxx;
}*/

void PlayingGamestate::querySceneryImage(float *ret_size_w, float *ret_size_h, float *ret_visual_h, const string &image_name, bool has_size, float size, float size_w, float size_h, bool has_visual_h, float visual_h) const {
    // side-effect: pre-loads any lazy images
    map<string, LazyAnimationLayer *>::const_iterator animation_iter = this->scenery_animation_layers.find(image_name);
    if( animation_iter == this->scenery_animation_layers.end() ) {
        LOG("failed to find image for scenery\n");
        LOG("    image name: %s\n", image_name.c_str());
        throw string("Failed to find scenery's image");
    }
    else {
        animation_iter->second->getAnimationLayer(); // force animation to be loaded
    }

    if( has_size ) {
        const AnimationLayer *animation_layer = animation_iter->second->getAnimationLayer();
        QPixmap image = animation_layer->getAnimationSet("")->getFrame(0, 0);
        int image_w = image.width();
        int image_h = image.height();
        if( image_w > image_h ) {
            size_w = size;
            size_h = (size*image_h)/(float)image_w;
        }
        else {
            size_h = size;
            size_w = (size*image_w)/(float)image_h;
        }
        qDebug("size: %f size_w: %f size_h: %f", size, size_w, size_h);
    }

    if( !has_visual_h ) {
        // default to size_h
        visual_h = size_h;
    }

    if( this->view_transform_3d ) {
        // hack to get height right in 3D mode!
        if( !has_size ) {
            // if we have specified w/h values explicitly, we assume these are in world coordinates already
        }
        else {
            // otherwise we assume the width/height of the image are already in "isometric"/3D format
            if( !has_visual_h )
                visual_h *= 2.0f; // to counter the QGraphicsView scaling
            size_h = size_w;
            if( !has_visual_h ) {
                visual_h = std::max(visual_h, size_h);
                ASSERT_LOGGER( visual_h - size_h >= 0.0f );
            }
        }
    }

    qDebug("image %s: size_w: %f size_h: %f visual_h %f", image_name.c_str(), size_w, size_h, visual_h);
    *ret_size_w = size_w;
    *ret_size_h = size_h;
    *ret_visual_h = visual_h;
}

void PlayingGamestate::loadQuest(const QString &filename, bool is_savegame, bool cheat_mode) {
    LOG("PlayingGamestate::loadQuest(%s)\n", filename.toUtf8().data());
    // filename should be full path

    MainWindow *window = game_g->getMainWindow();
    window->setEnabled(false);
    game_g->setPaused(true, true);

    gui_overlay->setProgress(0);
    qApp->processEvents();

    if( this->player != NULL && this->player->getLocation() != NULL ) {
        qDebug("remove player from location\n");
        this->player->getLocation()->removeCharacter(this->player);
        this->player->setListener(NULL, NULL);
    }
    if( this->quest != NULL ) {
        qDebug("delete previous quest...\n");
        delete this->quest;
    }
    // delete any items from previous quests
    view->clear();
    this->target_item = NULL;
    //this->closeAllSubWindows(); // just to be safe - e.g., when moving onto next quest from campaign window

    if( !is_savegame ) {
        this->time_hours = 1; // reset
    }

    qDebug("create new quest\n");
    this->quest = new Quest();
    //this->quest->setCompleted(true); // test

    if( !is_savegame ) {
        this->view_transform_3d = true;
        this->view_walls_3d = true;
        // if not a savegame, we should be loading new quest data that ought to be compatible
        // if it is a savegame, we check the versioning when loading
    }

    /*Location *location = new Location();
    this->quest->addLocation(location);*/
    gui_overlay->setProgress(10);
    qApp->processEvents();

    {
        Location *location = NULL;
        vector<RandomScenery> random_scenery;

        int progress_lo = 10, progress_hi = 50;
        bool done_player_start = false;
        QFile file(filename);
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open quest xml file");
        }
        qint64 size = file.size();
        int progress_count = 0;
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            //qDebug("read %d element: %s", reader.tokenType(), reader.name().toString().toStdString().c_str());
            if( reader.isStartElement() )
            {
                progress_count++;
                if( progress_count % 20 == 0 ) {
                    float progress = ((float)file.pos()) / ((float)size);
                    gui_overlay->setProgress((1.0f - progress) * progress_lo + progress * progress_hi);
                    qApp->processEvents();
                }

                qDebug("read start element: %s", reader.name().toString().toStdString().c_str());
                if( reader.name() == "quest" ) {
                    if( is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: quest element not expected in save games");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    quest->setName(name_s.toString().toStdString());
                }
                else if( reader.name() == "info" ) {
                    if( is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: info element not expected in save games");
                    }
                    QString info = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    qDebug("quest info: %s\n", info.toStdString().c_str());
                    quest->setInfo(info.toStdString());
                }
                else if( reader.name() == "completed_text" ) {
                    QString completed_text = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    qDebug("quest completed_text: %s\n", completed_text.toStdString().c_str());
                    quest->setCompletedText(completed_text.toStdString());
                }
                else if( reader.name() == "savegame" ) {
                    if( !is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: savegame element only allowed in save games");
                    }
                    QStringRef savegame_version_s = reader.attributes().value("savegame_version");
                    int savegame_version = parseInt(savegame_version_s.toString());
                    LOG("savegame_version = %d\n", savegame_version);
                    if( savegame_version >= 2 ) {
                        this->view_transform_3d = true;
                        this->view_walls_3d = true;
                    }
                }
                else if( reader.name() == "current_quest" ) {
                    if( !is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: current_quest element only allowed in save games");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    bool found = false;
                    int c = 0;
                    for(vector<QuestInfo>::const_iterator iter = this->quest_list.begin(); iter != this->quest_list.end(); ++iter, c++) {
                        const QuestInfo *quest_info = &*iter;
                        if( quest_info->getFilename() == name_s.toString().toStdString() ) {
                            found = true;
                            this->c_quest_indx = c;
                        }
                    }
                    if( !found ) {
                        LOG("error, current quest not found in quest list: %s\n", name_s.toString().toStdString().c_str());
                        this->c_quest_indx = 0;
                    }
                }
                else if( reader.name() == "journal" ) {
                    if( !is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: journal element only allowed in save games");
                    }
                    QString encoded = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    QByteArray journal = QByteArray::fromPercentEncoding(encoded.toLatin1());
                    this->journal_ss.clear();
                    this->journal_ss << journal.data();
                }
                else if( reader.name() == "time_hours" ) {
                    if( !is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: time_hours element only allowed in save games");
                    }
                    QStringRef time_hours_s = reader.attributes().value("value");
                    this->time_hours = parseInt(time_hours_s.toString());
                    LOG("time_hours = %d\n", time_hours);
                }
                else if( reader.name() == "game" ) {
                    if( !is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: game element only allowed in save games");
                    }

                    QStringRef difficulty_s = reader.attributes().value("difficulty");
                    qDebug("read difficulty: %s\n", difficulty_s.toString().toStdString().c_str());
                    for(int i=0;i<N_DIFFICULTIES;i++) {
                        Difficulty test_difficulty = (Difficulty)i;
                        if( difficulty_s.toString().toStdString() == Game::getDifficultyString(test_difficulty) ) {
                            this->setDifficulty(test_difficulty);
                            break;
                        }
                    }
                    // if not defined, we keep to the default

                    QStringRef permadeath_s = reader.attributes().value("permadeath");
                    qDebug("read permadeath: %s\n", permadeath_s.toString().toStdString().c_str());
                    this->permadeath = parseBool(permadeath_s.toString(), true);

                    // if not defined, we keep to the default
                    // this is for backwards compatibility - it will mean that old "random" games are actually loaded in campaign mode, but this shouldn't be a problem, as those random games can never be "completed"
                    QStringRef game_type_s = reader.attributes().value("gametype");
                    if( game_type_s.toString() == "gametype_campaign" ) {
                        gameType = GAMETYPE_CAMPAIGN;
                    }
                    else if( game_type_s.toString() == "gametype_random" ) {
                        gameType = GAMETYPE_RANDOM;
                    }
                    else if( game_type_s.length() > 0 ) {
                        LOG("unknown gametype: %s\n", game_type_s.toString().toStdString().c_str());
                    }
                }
                else if( reader.name() == "flag" ) {
                    if( location != NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: flag element wasn't expected here");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    qDebug("read flag: %s\n", name_s.toString().toStdString().c_str());
                    if( name_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: flag has no name");
                    }
                    else {
                        quest->addFlag(name_s.toString().toStdString());
                    }
                }
                else if( reader.name() == "location" ) {
                    if( location != NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: location element wasn't expected here");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    qDebug("read location: %s\n", name_s.toString().toStdString().c_str());
                    if( quest->findLocation(name_s.toString().toStdString()) != NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: duplicate location name");
                    }
                    QStringRef type_s = reader.attributes().value("type");
                    QStringRef geo_type_s = reader.attributes().value("geo_type");
                    QStringRef lighting_min_s = reader.attributes().value("lighting_min");
                    QStringRef display_name_s = reader.attributes().value("display_name");
                    QStringRef rest_summon_location_s = reader.attributes().value("rest_summon_location");
                    location = new Location(name_s.toString().toStdString());
                    if( type_s.length() > 0 ) {
                        if( type_s.toString() == "indoors" ) {
                            location->setType(Location::TYPE_INDOORS);
                        }
                        else if( type_s.toString() == "outdoors" ) {
                            location->setType(Location::TYPE_OUTDOORS);
                        }
                        else {
                            LOG("error at line %d\n", reader.lineNumber());
                            LOG("unknown type: %s\n", type_s.toString().toStdString().c_str());
                            throw string("unexpected quest xml: location has unknown type");
                        }
                    }
                    if( geo_type_s.length() > 0 ) {
                        if( geo_type_s.toString() == "dungeon" ) {
                            location->setGeoType(Location::GEOTYPE_DUNGEON);
                        }
                        else if( geo_type_s.toString() == "outdoors" ) {
                            location->setGeoType(Location::GEOTYPE_OUTDOORS);
                        }
                        else {
                            LOG("error at line %d\n", reader.lineNumber());
                            LOG("unknown geo_type: %s\n", geo_type_s.toString().toStdString().c_str());
                            throw string("unexpected quest xml: location has unknown geo_type");
                        }
                    }
                    if( lighting_min_s.length() > 0 ) {
                        int lighting_min = parseInt(lighting_min_s.toString());
                        if( lighting_min < 0 || lighting_min > 255 ) {
                            LOG("invalid lighting_min: %f\n", lighting_min);
                            throw string("unexpected quest xml: location has invalid lighting_min");
                        }
                        location->setLightingMin(static_cast<unsigned char>(lighting_min));
                    }
                    bool display_name = parseBool(display_name_s.toString(), true);
                    location->setDisplayName(display_name);
                    location->setRestSummonLocation(rest_summon_location_s.toString().toStdString());
                    quest->addLocation(location);
                }
                else if( reader.name() == "floor" ) {
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    if( image_name_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: floor element has no image_name attribute");
                    }
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: floor element outside of location");
                    }
                    location->setFloorImageName(image_name_s.toString().toStdString());
                }
                else if( reader.name() == "wall" ) {
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: wall element outside of location");
                    }
                    if( image_name_s.length() > 0 ) {
                        location->setWallImageName(image_name_s.toString().toStdString());
                        QStringRef image_x_scale_s = reader.attributes().value("image_x_scale");
                        if( image_x_scale_s.length() > 0 ) {
                            float image_x_scale = parseFloat(image_x_scale_s.toString());
                            location->setWallXScale(image_x_scale);
                        }
                    }
                }
                else if( reader.name() == "dropwall" ) {
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: dropwall element outside of location");
                    }
                    if( image_name_s.length() > 0 ) {
                        location->setDropWallImageName(image_name_s.toString().toStdString());
                    }
                }
                else if( reader.name() == "background" ) {
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    if( image_name_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: background element has no image_name attribute");
                    }
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: background element outside of location");
                    }
                    location->setBackgroundImageName(image_name_s.toString().toStdString());
                }
                else if( reader.name() == "wandering_monster" ) {
                    QStringRef template_s = reader.attributes().value("template");
                    if( template_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: wandering_monster element has no template attribute");
                    }
                    QStringRef time_s = reader.attributes().value("time");
                    QStringRef rest_chance_s = reader.attributes().value("rest_chance");
                    int time = parseInt(time_s.toString());
                    int rest_chance = parseInt(rest_chance_s.toString());
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: wandering_monster element outside of location");
                    }
                    location->setWanderingMonster(template_s.toString().toStdString(), time, rest_chance);
                }
                else if( reader.name() == "floorregion" ) {
                    FloorRegion *floor_region = loadFloorRegion(reader);
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: floorregion end element outside of location");
                    }
                    location->addFloorRegion(floor_region);
                }
                else if( reader.name() == "tilemap" ) {
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: tilemap element outside of location");
                    }
                    QStringRef x_s = reader.attributes().value("x");
                    float x = parseFloat(x_s.toString());
                    QStringRef y_s = reader.attributes().value("y");
                    float y = parseFloat(y_s.toString());
                    QStringRef imagemap_s = reader.attributes().value("imagemap");
                    QString imagemap = imagemap_s.toString();
                    QStringRef tile_width_s = reader.attributes().value("tile_width");
                    int tile_width = parseInt(tile_width_s.toString());
                    QStringRef tile_height_s = reader.attributes().value("tile_height");
                    int tile_height = parseInt(tile_height_s.toString());
                    QString map = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    if( map.length() > 0 ) {
                        qDebug("tilemap:");
                        int index = 0;
                        if( map.at(index) == '\n' )
                            index++;
                        vector<string> map_lines;
                        for(;;) {
                            int next_index =  map.indexOf('\n', index);
                            if( next_index == -1 || next_index == index )
                                break;
                            QString line = map.mid(index, next_index - index); // doesn't copy the newline
                            map_lines.push_back(line.toStdString());
                            qDebug("    %s", line.toStdString().c_str());
                            index = next_index+1;
                        }
                        Tilemap *tilemap = new Tilemap(x, y, imagemap.toStdString(), tile_width, tile_height, map_lines);
                        location->addTilemap(tilemap);
                    }
                }
                /*else if( reader.name() == "boundary" ) {
                    questXMLType = QUEST_XML_TYPE_BOUNDARY;
                    boundary = Polygon2D(); // reset
                }
                else if( reader.name() == "point" ) {
                    if( questXMLType != QUEST_XML_TYPE_BOUNDARY ) {
                        throw string("unexpected quest xml");
                    }
                    QStringRef point_x_s = reader.attributes().value("x");
                    float point_x = parseFloat(point_x_s.toString());
                    QStringRef point_y_s = reader.attributes().value("y");
                    float point_y = parseFloat(point_y_s.toString());
                    qDebug("found boundary point: %f, %f\n", point_x, point_y);
                    boundary.addPoint(Vector2D(point_x, point_y));
                }*/
                else if( reader.name() == "player_start" ) {
                    if( done_player_start ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: duplicate player_start element");
                    }
                    QStringRef pos_x_s = reader.attributes().value("x");
                    float pos_x = parseFloat(pos_x_s.toString());
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_y = parseFloat(pos_y_s.toString());
                    if( player == NULL ) {
                        throw string("encountered player_start element, but player not yet defined");
                    }
                    qDebug("player starts at %f, %f", pos_x, pos_y);
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: player_start element outside of location");
                    }
                    this->c_location = location;
                    this->c_location->addCharacter(player, pos_x, pos_y);
                    done_player_start = true;
                }
                else if( reader.name() == "quest_objective" ) {
                    QStringRef type_s = reader.attributes().value("type");
                    QStringRef arg1_s = reader.attributes().value("arg1");
                    QStringRef gold_s = reader.attributes().value("gold");
                    int gold = parseInt(gold_s.toString());
                    QuestObjective *quest_objective = new QuestObjective(type_s.toString().toStdString(), arg1_s.toString().toStdString(), gold);
                    this->quest->setQuestObjective(quest_objective);
                }
                else if( reader.name() == "quest_info" ) {
                    QStringRef complete_s = reader.attributes().value("complete");
                    bool complete = parseBool(complete_s.toString(), true);
                    this->quest->setCompleted(complete);
                }
                else if( reader.name() == "npc" || reader.name() == "player" ) {

                    bool is_player = false;
                    Vector2D pos;
                    Character *npc = this->loadNPC(&is_player, &pos, reader);
                    if( is_player ) {
                        if( !is_savegame ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unexpected quest xml: player element not expected in non-save games");
                        }
                        this->player = npc;
                        this->loadPlayerAnimation();
                    }

                    // if an NPC doesn't have a default position defined in the file, we set it to the position
                    if( !npc->hasDefaultPosition() ) {
                        npc->setDefaultPosition(pos.x, pos.y);
                    }
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: npc/player element outside of location");
                    }
                    location->addCharacter(npc, pos.x, pos.y);
                }
                else if( reader.name() == "item" || reader.name() == "weapon" || reader.name() == "shield" || reader.name() == "armour" || reader.name() == "ring" || reader.name() == "ammo" || reader.name() == "currency" || reader.name() == "gold" ) {
                    Vector2D pos;
                    Item *item = this->loadItem(&pos, reader, NULL, NULL, false);

                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: item element outside of location");
                    }
                    location->addItem(item, pos.x, pos.y);
                }
                else if( reader.name() == "scenery" ) {
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: scenery element outside of location");
                    }
                    QStringRef pos_x_s = reader.attributes().value("x");
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_x = parseFloat(pos_x_s.toString());
                    float pos_y = parseFloat(pos_y_s.toString());
                    Scenery *scenery = loadScenery(reader);
                    location->addScenery(scenery, pos_x, pos_y);
                }
                else if( reader.name() == "trap" ) {
                    QStringRef pos_x_s = reader.attributes().value("x");
                    float pos_x = parseFloat(pos_x_s.toString());
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_y = parseFloat(pos_y_s.toString());
                    QStringRef size_w_s = reader.attributes().value("w");
                    float size_w = parseFloat(size_w_s.toString());
                    QStringRef size_h_s = reader.attributes().value("h");
                    float size_h = parseFloat(size_h_s.toString());
                    Trap *trap = loadTrap(reader);
                    trap->setSize(size_w, size_h);
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: trap element outside of location");
                    }
                    location->addTrap(trap, pos_x, pos_y);
                }
                else if( reader.name() == "start_bonus" ) {
                    if( is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: start_bonus element not expected in save games");
                    }
                    loadStartBonus(reader, cheat_mode);
                }
                else if( reader.name() == "random_scenery" ) {
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: random_scenery element outside of location");
                    }
                    if( random_scenery.size() > 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: random_scenery already defined");
                    }
                    random_scenery = loadRandomScenery(reader);
                }
            }
            else if( reader.isEndElement() ) {
                if( reader.name() == "location" ) {
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: location end element wasn't expected here");
                    }
                    // n.b., we can't make use of some location methods here, e.g., boundaries haven't been created yet
                    // (and if we did create them here, we'd have to ensure we update that information if we ever say add random NPCs or blocking scenery
                    float location_width = 0.0f, location_height = 0.0f;
                    location->calculateSize(&location_width, &location_height);
                    for(vector<RandomScenery>::const_iterator iter = random_scenery.begin(); iter != random_scenery.end(); ++iter) {
                        RandomScenery random_scenery = *iter;
                        const Scenery *scenery = random_scenery.getScenery();
                        if( scenery->isBlocking() ) {
                            throw string("unexpected quest xml: random_scenery shouldn't be blocking");
                        }
                        float density = random_scenery.getDensity();
                        int count = (int)(density * location_width * location_height);
                        count += rollDice(1, 3, -1);
                        if( count > 0 ) {
                            for(int i=0;i<count;i++) {
                                const float precision = 100.0f;
                                float pos_x = rollDice(1, (int)(location_width*precision), -1) / precision;
                                float pos_y = rollDice(1, (int)(location_height*precision), -1) / precision;
                                FloorRegion *floor_region = location->findFloorRegionInside(Vector2D(pos_x, pos_y), scenery->getWidth(), scenery->getHeight());
                                if( floor_region != NULL ) {
                                    Scenery *new_scenery = scenery->clone();
                                    location->addScenery(new_scenery, pos_x, pos_y);
                                }
                            }
                        }
                    }

                    for(vector<RandomScenery>::const_iterator iter = random_scenery.begin(); iter != random_scenery.end(); ++iter) {
                        RandomScenery random_scenery = *iter;
                        delete random_scenery.getScenery();
                    }
                    random_scenery.clear();
                    location = NULL;
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error at line %d\n", reader.lineNumber());
            LOG("error reading quest xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading quest xml file");
        }
        else if( !done_player_start ) {
            LOG("quest xml didn't define player_start\n");
            throw string("quest xml didn't define player_start");
        }
    }

    if( this->permadeath && is_savegame ) {
        this->permadeath_has_savefilename = true;
        this->permadeath_savefilename = filename;
        if( this->quickSaveButton != NULL ) {
            delete this->quickSaveButton;
            this->quickSaveButton = NULL;
        }
    }

    gui_overlay->setProgress(50);
    qApp->processEvents();

    int progress_lo = 50, progress_hi = 100;
    this->processLocations(progress_lo, progress_hi);

    gui_overlay->unsetProgress();
    qApp->processEvents();

    this->c_location->setListener(this, NULL); // must do after creating the location and its contents, so it doesn't try to add items to the scene, etc
    this->setupView();

    window->setEnabled(true);
    game_g->setPaused(false, true);
    game_g->restartElapsedTimer();
    is_created = true;

    //qApp->processEvents();

    if( !is_savegame && quest->getInfo().length() > 0 ) {
        string quest_info = quest->getInfo();
        quest_info = convertToHTML(quest_info);
        stringstream str;
        str << "<html><body>";
        str << "<h1>Quest: " << quest->getName() << "</h1>";
        str << "<p>" << quest_info << "</p>";
        str << "</body></html>";
        this->showInfoWindow(str.str());

        this->journal_ss << "<hr/><p><b>Quest Details: " << quest->getName() << "</b></p>";
        this->journal_ss << "<p>" << quest_info << "</p>";
    }

    //game_g->playSound(music_key_ingame_c, true);

    qDebug("View is transformed? %d", view->isTransformed());
    LOG("done\n");
}

void PlayingGamestate::createRandomQuest() {
    createRandomQuest(false, false, DIRECTION4_NORTH);
}

void PlayingGamestate::createRandomQuest(bool force_start, bool passageway_start_type, Direction4 start_direction) {
    LOG("PlayingGamestate::createRandomQuest()\n");
    ASSERT_LOGGER(gameType == GAMETYPE_RANDOM);

    MainWindow *window = game_g->getMainWindow();
    window->setEnabled(false);
    game_g->setPaused(true, true);

    gui_overlay->setProgress(0);
    qApp->processEvents();

    if( this->player != NULL && this->player->getLocation() != NULL ) {
        qDebug("remove player from location\n");
        this->player->getLocation()->removeCharacter(this->player);
        this->player->setListener(NULL, NULL);
    }
    if( this->quest != NULL ) {
        qDebug("delete previous quest...\n");
        delete this->quest;
    }
    // delete any items from previous quests
    view->clear();
    this->target_item = NULL;
    //this->closeAllSubWindows(); // just to be safe - e.g., when moving onto next quest from campaign window

    qDebug("create random quest\n");
    this->quest = new Quest();
    //this->quest->setCompleted(true); // test
    this->c_quest_indx = 0; // ensure reset to 0

    this->quest->setName(tr("Random dungeon").toStdString());

    stringstream str;
    str << "This game type allows you to explore a randomly generated dungeon. Explore the dungeon, kill any monsters you find, and collect the loot!\n\n";
    str << "There is no specific objective, but see how much XP you can obtain, or how much gold you can find.";
    this->quest->setInfo(str.str());

    this->view_transform_3d = true;
    this->view_walls_3d = true;

    string monster_type = rollDice(1, 3, 0) <= 2 ? "goblinoid" : "undead";
    //string monster_type = "goblinoid";
    LOG("monster type: %s\n", monster_type.c_str());

    // load random quest definitions
    map<string, NPCTable *> npc_tables;
    {
        LOG("load random quest definitions\n");
        QFile file(QString(DEPLOYMENT_PATH) + "data/randomquest.xml");
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open quest xml file");
        }
        QXmlStreamReader reader(&file);
        NPCTable *npc_table = NULL;
        NPCTableLevel *npc_table_level = NULL;
        NPCGroup *npc_group = NULL;
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            //qDebug("read %d element: %s", reader.tokenType(), reader.name().toString().toStdString().c_str());
            if( reader.isStartElement() ) {
                if( reader.name() == "npc_table" ) {
                    QStringRef group_s = reader.attributes().value("group");
                    QStringRef type_s = reader.attributes().value("type");
                    qDebug("found npc table, group: %s, type: %s", group_s.toString().toStdString().c_str(), type_s.toString().toStdString().c_str());
                    if( group_s.length() == 0 || group_s.toString().toStdString() == monster_type ) {
                        qDebug("    matches group");
                        if( npc_tables.find( type_s.toString().toStdString() ) != npc_tables.end() ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("more than one table specified for a given group and type");
                        }
                        npc_table = new NPCTable();
                        npc_tables[type_s.toString().toStdString()] = npc_table;
                    }
                }
                else if( reader.name() == "level" ) {
                    if( npc_table != NULL ) {
                        QStringRef value_s = reader.attributes().value("value");
                        int value = parseInt(value_s.toString());
                        qDebug("    found level %d", value);
                        npc_table_level = new NPCTableLevel();
                        npc_table->addLevel(value, npc_table_level);
                    }
                }
                else if( reader.name() == "npc_group" ) {
                    if( npc_table != NULL ) {
                        if( npc_table_level == NULL ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("npc_group tag not inside level");
                        }
                        qDebug("        found npc group");
                        npc_group = new NPCGroup();
                        npc_table_level->addNPCGroup(npc_group);
                    }
                }
                else if( reader.name() == "npc" ) {
                    if( npc_table != NULL ) {
                        if( npc_group == NULL ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("npc tag not inside npc_group");
                        }
                        bool is_player = false;
                        Vector2D pos;
                        Character *npc = this->loadNPC(&is_player, &pos, reader);
                        qDebug("            found NPC: %s", npc->getName().c_str());
                        npc_group->addNPC(npc);
                    }
                }
            }
            else if( reader.isEndElement() ) {
                if( reader.name() == "npc_table" ) {
                    npc_table = NULL;
                }
                else if( reader.name() == "level" ) {
                    npc_table_level = NULL;
                }
                else if( reader.name() == "npc_group" ) {
                    npc_group = NULL;
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error at line %d\n", reader.lineNumber());
            LOG("error reading randomquest xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading randomquest xml file");
        }

    }

    const int progress_lo = 10, progress_mid = 60, progress_hi = 100;

    gui_overlay->setProgress(progress_lo);
    qApp->processEvents();

    const int n_levels_c = 3;
    string previous_level_name;
    Scenery *previous_exit_down = NULL;
    Vector2D first_player_start;
    Location *first_location = NULL;
    for(int level=0;;level++) {
        float progress = ((float)level) / ((float)n_levels_c);
        stringstream progress_message;
        progress_message << "Generating random dungeon level " << (level+1) << "...";
        gui_overlay->setProgress((1.0f - progress) * progress_lo + progress * progress_mid, progress_message.str());
        qApp->processEvents();

        Vector2D player_start;
        Scenery *exit_down = NULL, *exit_up = NULL;
        Location *location = LocationGenerator::generateLocation(&exit_down, &exit_up, this, &player_start, npc_tables, level, n_levels_c, force_start, passageway_start_type, start_direction);
        this->quest->addLocation(location);
        if( level == 0 ) {
            first_player_start = player_start;
            first_location = location;
        }
        if( exit_up != NULL ) {
            if( level == 0 ) {
                // out of dungeon
                exit_up->setExit(true);
            }
            else {
                // exit to previous level
                Vector2D previous_exit_down_pos = previous_exit_down->getPos() + Vector2D(1.0f, 0.0f);
                exit_up->setExitLocation(previous_level_name, previous_exit_down_pos, 0);

                previous_exit_down->setExitLocation(location->getName(), player_start, 0);
            }
        }
        if( exit_down == NULL ) {
            // reached bottom of dungeon
            break;
        }

        previous_level_name = location->getName();
        previous_exit_down = exit_down;

    }

    for(map<string, NPCTable *>::iterator iter = npc_tables.begin(); iter != npc_tables.end(); ++iter) {
        NPCTable *npc_table = iter->second;
        delete npc_table;
    }

    ASSERT_LOGGER(first_location != NULL);
    this->c_location = first_location;
    this->c_location->addCharacter(player, first_player_start.x, first_player_start.y);

    gui_overlay->setProgress(progress_mid);
    qApp->processEvents();

    this->processLocations(progress_mid, progress_hi);

    gui_overlay->unsetProgress();
    qApp->processEvents();

    this->c_location->setListener(this, NULL); // must do after creating the location and its contents, so it doesn't try to add items to the scene, etc
    this->setupView();

    window->setEnabled(true);
    game_g->setPaused(false, true);
    game_g->restartElapsedTimer();
    is_created = true;

    //qApp->processEvents();

    if( quest->getInfo().length() > 0 ) {
        string quest_info = quest->getInfo();
        quest_info = convertToHTML(quest_info);
        stringstream str;
        str << "<html><body>";
        str << "<h1>Quest: " << quest->getName() << "</h1>";
        str << "<p>" << quest_info << "</p>";
        str << "</body></html>";
        this->showInfoWindow(str.str());

        this->writeJournal("<hr/><p><b>" + tr("Quest Details:").toStdString() + " ");
        this->writeJournal(quest->getName());
        this->writeJournal("</b></p><p>");
        this->writeJournal(quest_info);
        this->writeJournal("</p>");
    }

    // test:
    //this->c_location->revealMap(this);

    qDebug("View is transformed? %d", view->isTransformed());
    LOG("done\n");
}

void PlayingGamestate::processLocations(int progress_lo, int progress_hi) {
    int progress_count = 0;
    for(vector<Location *>::iterator iter = quest->locationsBegin(); iter != quest->locationsEnd(); ++iter, progress_count++) {
        Location *loc = *iter;
        qDebug("process location: %s", loc->getName().c_str());

        float progress = ((float)progress_count) / ((float)quest->getNLocations());
        stringstream progress_message;
        progress_message << tr("Processing locations: ").toStdString() << (progress_count+1) << " / " << quest->getNLocations() << "...";
        gui_overlay->setProgress((1.0f - progress) * progress_lo + progress * progress_hi, progress_message.str());
        qApp->processEvents();

        if( loc->getBackgroundImageName().length() == 0 ) {
            throw string("Location doesn't define background image name");
        }
        else if( loc->getFloorImageName().length() == 0 ) {
            throw string("Location doesn't define floor image name");
        }

        loc->createBoundariesForRegions();
        loc->createBoundariesForScenery();
        loc->createBoundariesForFixedNPCs();
        loc->addSceneryToFloorRegions();
        loc->calculateDistanceGraph();
        for(set<Character *>::iterator iter2 = loc->charactersBegin(); iter2 != loc->charactersEnd(); ++iter2) {
            Character *character = *iter2;
            if( character != player && !character->isStaticImage() ) {
                map<string, LazyAnimationLayer *>::const_iterator iter3 = this->animation_layers.find( character->getAnimationName() );
                if( iter3 == this->animation_layers.end() ) {
                    LOG("can't find animation layer %s for %s\n", character->getAnimationName().c_str(), character->getName().c_str());
                    throw string("can't find animation layer");
                }
                LazyAnimationLayer *animation_layer = iter3->second;
                animation_layer->getAnimationLayer(); // force animation to be loaded
            }
        }
    }
    gui_overlay->setProgress(progress_hi);
}

void PlayingGamestate::addGraphicsItem(QGraphicsItem *object, float width, bool undo_3d) {
    float item_scale = width / object->boundingRect().width();
    if( this->view_transform_3d && undo_3d ) {
        // undo the 3D transform
        float centre_x = 0.5f*object->boundingRect().width();
        float centre_y = 0.5f*object->boundingRect().height();
        QTransform transform;
        transform = transform.scale(item_scale, 2.0f*item_scale);
        transform = transform.translate(-centre_x, -centre_y);
        object->setTransform(transform);
    }
    else {
        object->setTransformOriginPoint(-0.5f*object->boundingRect().width()*item_scale, -0.5f*object->boundingRect().height()*item_scale);
        object->setScale(item_scale);
    }
    scene->addItem(object);
}

void PlayingGamestate::locationAddItem(const Location *location, Item *item, bool visible) {
    if( this->c_location == location ) {
        QGraphicsPixmapItem *object = new QGraphicsPixmapItem();
        item->setUserGfxData(object);
        object->setPixmap( this->getItemImage( item->getImageName() ) );
        /*{
            // DEBUG
            QPen pen(Qt::red);
            // the pen width is affected by the transformation, but setting 0 forces it to ignore the transformation
            // note that in Qt 4, QPen defaulted to width 0, but in Qt 5 it defaults to width 1, so we set it explicitly
            pen.setWidth(0);
            scene->addEllipse(item->getX() - 0.5f*item_width, item->getY() - 0.5f*item_width, item_width, item_width, pen);
            //scene->addEllipse(item->getX() - 0.05f, item->getY() - 0.05f, 0.1f, 0.1f, pen);
        }*/
        float icon_width = item->getIconWidth();
        object->setPos(item->getX(), item->getY());
        object->setZValue(z_value_items);
        object->setVisible(visible);
        this->addGraphicsItem(object, icon_width, true);
    }
}

void PlayingGamestate::locationRemoveItem(const Location *location, Item *item) {
    if( this->c_location == location ) {
        QGraphicsItem *object = static_cast<QGraphicsPixmapItem *>(item->getUserGfxData());
        item->setUserGfxData(NULL);
        scene->removeItem(object);
        delete object;
    }
}

void PlayingGamestate::locationAddScenery(const Location *location, Scenery *scenery) {
    //qDebug("PlayingGamestate::locationAddScenery(%d, %d): %s", location, scenery, scenery->getName().c_str());
    if( this->c_location == location ) {
        int n_stripes = ( scenery->isBoundaryIso() && fabs(scenery->getBoundaryIsoRatio() - 0.5f) > E_TOL_LINEAR ) ? 4 : 1;
        scenery->clearUserGfxData();
        float stripe_width = 1.0f / (float)n_stripes;
        AnimatedObject *top_object = NULL;
        float scenery_scale_w = 0.0f, scenery_scale_h = 0.0f;
        QTransform transform;
        for(int stripe=0;stripe<n_stripes;stripe++) {
            AnimatedObject *object = new AnimatedObject();
            //scenery->setUserGfxData(object);
            scenery->addUserGfxData(object);
            if( n_stripes > 1 ) {
                object->setClip(stripe*stripe_width, 0.0f, stripe_width, 1.0f);
            }
            this->locationUpdateScenery(scenery);
            object->setOpacity(scenery->getOpacity());
            object->setVisible(false); // default to false, visibility is checked afterwards
            scene->addItem(object);
            object->setPos(scenery->getX(), scenery->getY());
            float z_value = object->pos().y();
            if( n_stripes > 1 ) {
                float boundary_iso_ratio = scenery->getBoundaryIsoRatio();
                float y0 = (0.5f-boundary_iso_ratio) * scenery->getHeight();
                float y1 = -y0;
                float alpha = ((float)stripe + 0.5f)/((float)n_stripes);
                float offset_z = (1.0f-alpha)*y0 + alpha*y1;
                z_value += offset_z;
                //qDebug("%d : %f -> %f : alpha %f offset %f", stripe, y0, y1, alpha, offset_z);
            }
            if( scenery->getDrawType() == Scenery::DRAWTYPE_FLOATING ) {
                z_value += 1000.0f;
            }
            else if( scenery->getDrawType() == Scenery::DRAWTYPE_BACKGROUND ) {
                //qDebug("background: %s", scenery->getName().c_str());
                z_value = z_value_scenery_background;
            }
            object->setZValue(z_value);
            //qDebug("    z value: %f", z_value);
            if( top_object == NULL || z_value > top_object->zValue() + E_TOL_LINEAR ) {
                top_object = object;
            }
            if( stripe == 0 ) {
                scenery_scale_w = scenery->getWidth() / object->boundingRect().width();
                scenery_scale_h = scenery->getVisualHeight() / object->boundingRect().height();
                float centre_x = 0.5f*object->boundingRect().width();
                float centre_y = 0.5f*object->boundingRect().height();
                centre_y += 0.5f * (scenery->getVisualHeight() - scenery->getHeight()) / scenery_scale_h;
                if( scenery->getVisualHeight() - scenery->getHeight() < 0.0f ) {
                    qDebug(">>> %s at %f, %f", scenery->getName().c_str(), scenery->getX(), scenery->getY());
                    ASSERT_LOGGER( scenery->getVisualHeight() - scenery->getHeight() >= 0.0f );
                }
                transform = transform.scale(scenery_scale_w, scenery_scale_h);
                transform = transform.translate(-centre_x, -centre_y);
            }
            object->setTransform(transform);
        }

        if( scenery->hasSmoke() ) {
            SmokeParticleSystem *ps = new SmokeParticleSystem(smoke_pixmap);
            ps->setBirthRate(25.0f);
            Vector2D smoke_pos = scenery->getSmokePos();
            ps->setPos(smoke_pos.x*top_object->boundingRect().width(), smoke_pos.y*top_object->boundingRect().height());
            ps->setZValue(top_object->pos().y() + 2000.0f);
            ps->setParentItem(top_object);
            QTransform transform2;
            qDebug("### scenery scale %f , %f", scenery_scale_w, scenery_scale_h);
            qDebug("### object %f x %f", top_object->boundingRect().width(), top_object->boundingRect().height());
            const float mult_y = this->view_transform_3d ? 2.0f : 1.0f;
            transform2 = transform2.scale(1.0f/(64.0f*scenery_scale_w), mult_y/(64.0f*scenery_scale_h));
            ps->setTransform(transform2);
        }
    }
}

void PlayingGamestate::locationRemoveScenery(const Location *location, Scenery *scenery) {
    if( this->c_location == location ) {
        for(size_t i=0;i<scenery->getNUserGfxData();i++) {
            QGraphicsItem *object = static_cast<QGraphicsPixmapItem *>(scenery->getUserGfxData(i));
            scene->removeItem(object);
            delete object;
        }
        scenery->clearUserGfxData();

        if( scenery->blocksVisibility() ) {
            this->updateVisibility(player->getPos());
#ifdef _DEBUG
            this->refreshDebugItems();
#endif
        }
    }
}

void PlayingGamestate::locationUpdateScenery(Scenery *scenery) {
    //qDebug("### update for: %s", scenery->getName().c_str());
    for(size_t i=0;i<scenery->getNUserGfxData();i++) {
        AnimatedObject *object = static_cast<AnimatedObject *>(scenery->getUserGfxData(i));
        object->clearAnimationLayers();
        //qDebug("update scenery: %s", scenery->getName().c_str());
        object->addAnimationLayer( this->scenery_animation_layers[scenery->getImageName()]->getAnimationLayer() );
        if( scenery->isOpened() ) {
            object->setAnimationSet("opened", true);
        }
        else {
            object->setAnimationSet("", true);
        }
    }
}

void PlayingGamestate::locationAddCharacter(const Location *, Character *character) {
    AnimatedObject *object = new AnimatedObject();
    object->setBounce( character->isBounce() );
    this->characterUpdateGraphics(character, object);
    this->characterTurn(character, object);
    scene->addItem(object);
    object->setPos(character->getX(), character->getY());
    object->setZValue( object->pos().y() );
    if( character != player ) {
        // default to invisible for NPCs, until set by testFogOfWar()
        object->setVisible(false);
    }
    int character_size = std::max(object->getWidth(), object->getHeight());
    float ratio_w = object->getWidth()/(float)character_size;
    float ratio_h = object->getHeight()/(float)character_size;
    const int off_x = 64 - 32;
    const int off_y = 94 - 40;
    if( this->view_transform_3d ) {
        //const float desired_size = 0.75f;
        const float desired_size = 1.5f * character->getImageSize();
        float character_scale = desired_size / (float)character_size;
        qDebug("character %s size %d scale %f", character->getName().c_str(), character_size, character_scale);
        {
            Vector2D scale_centre(-desired_size*ratio_w*off_x/64.0f, -2.0f*desired_size*ratio_h*off_y/64.0f);
            QTransform transform;
            transform.translate(scale_centre.x, scale_centre.y);
            transform.scale(character_scale, 2.0f*character_scale);
            transform.translate(-scale_centre.x, -scale_centre.y);
            object->setTransform(transform);
        }
    }
    else {
        //const float desired_size = 1.0f;
        const float desired_size = 2.0f * character->getImageSize();
        float character_scale = desired_size / (float)character_size;
        qDebug("character %s size %d scale %f", character->getName().c_str(), character_size, character_scale);
        object->setTransformOriginPoint(-desired_size*ratio_w*off_x/64.0f, -desired_size*ratio_h*off_y/64.0f);
        object->setScale(character_scale);
    }

    character->setListener(this, object);
    //item->setAnimationSet("attack"); // test
}

void PlayingGamestate::clickedStats() {
    LOG("clickedStats()\n");
    this->closeSubWindow();

    new StatsWindow(this);
    game_g->setPaused(true, true);
}

void PlayingGamestate::clickedItems() {
    LOG("clickedItems()\n");
    this->closeSubWindow();

    new ItemsWindow(this);
    game_g->setPaused(true, true);
}

void PlayingGamestate::clickedJournal() {
    LOG("clickedJournal()\n");
    this->closeSubWindow();
    this->playSound("turn_page");
    stringstream str;
    str << "<html><body>";
    /*for(int i=0;i<100;i++) {
        str << "i = " << i << "<br/>";
    }*/
    str << "<h1>Journal</h1>";
    str << this->journal_ss.str();
    str << "<hr/><p>" << this->getJournalDate() << "</p>";
    str << "</body></html>";
    this->showInfoWindow(str.str(), true);
}

void PlayingGamestate::clickedPause() {
    LOG("clickedPause()\n");
    game_g->togglePaused();
    if( game_g->isPaused() ) {
        this->displayPausedMessage();
    }
}

void PlayingGamestate::clickedOptions() {
    LOG("clickedOptions()\n");
    this->closeSubWindow();

    game_g->setPaused(true, true);

    QWidget *subwindow = new CloseSubWindowWidget(this);
    this->addWidget(subwindow, false);

    QVBoxLayout *layout = new QVBoxLayout();
    subwindow->setLayout(layout);

    QPushButton *quitButton = new QPushButton(tr("Quit game"));
    game_g->initButton(quitButton);
    quitButton->setShortcut(QKeySequence(Qt::Key_Q));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
    quitButton->setToolTip(tr("Quit current game, and return to the main menu (Q)"));
#endif
    quitButton->setFont(game_g->getFontBig());
    quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(quitButton);
    connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));

    QPushButton *saveButton = new QPushButton(tr("Save game"));
    game_g->initButton(saveButton);
    saveButton->setShortcut(QKeySequence(Qt::Key_S));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
    saveButton->setToolTip(tr("Save the current game (S)"));
#endif
    saveButton->setFont(game_g->getFontBig());
    saveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(saveButton);
    connect(saveButton, SIGNAL(clicked()), this, SLOT(clickedSave()));

#ifndef Q_OS_ANDROID
    // Because Qt's fullscreen is a bit unusual, it's useful to have a built in option to minimise to desktop
    // This is especially important for Windows 8 tablets, where there is no way to minimise a fullscrreen Qt application
    QPushButton *minimiseButton = new QPushButton(tr("Minimise to desktop"));
    game_g->initButton(minimiseButton);
    minimiseButton->setFont(game_g->getFontBig());
    minimiseButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(minimiseButton);
    connect(minimiseButton, SIGNAL(clicked()), game_g->getMainWindow(), SLOT(showMinimized()));
#endif

    QPushButton *closeButton = new QPushButton(tr("Back to game"));
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Escape));
    closeButton->setFont(game_g->getFontBig());
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeSubWindow()));

    //subwindow->showFullScreen();
    //game_g->getScreen()->getMainWindow()->hide();
}

int PlayingGamestate::getRestTime() const {
    int health_restore_percent = 100 - this->player->getHealthPercent();
    int time = (int)(health_restore_percent*10.0f/100.0f + 0.5f);
    if( time == 0 ) {
        time = 1;
    }
    return time;
}

void PlayingGamestate::clickedRest() {
    LOG("clickedRest()\n");
    if( c_location->hasEnemies(this) ) {
        this->showInfoDialog(tr("You cannot rest here - enemies are nearby.").toStdString());
        return;
    }
    if( this->askQuestionDialog(tr("Rest until fully healed?").toStdString()) ) {
        bool rest_ok = true;
        if( c_location->getRestSummonLocation().length() > 0 ) {
            Location *summon_location = this->quest->findLocation(c_location->getRestSummonLocation());
            if( summon_location == NULL ) {
                LOG("can't find summon_location!: %s\n", c_location->getRestSummonLocation().c_str());
            }
            else if( summon_location == c_location ) {
                LOG("already in summon_location!: %s\n", c_location->getRestSummonLocation().c_str());
            }
            else if( summon_location->getNCharacters() == 0 ) {
                LOG("no NPCs in summon_location!: %s\n", c_location->getRestSummonLocation().c_str());
            }
            else {
                rest_ok = false;
                c_location->setRestSummonLocation("");

                set<Character *> npcs;
                while( summon_location->getNCharacters() > 0 ) {
                    Character *npc = *summon_location->charactersBegin();
                    summon_location->removeCharacter(npc);
                    npcs.insert(npc);
                }
                for(set<Character *>::iterator iter = npcs.begin(); iter != npcs.end(); ++iter) {
                    Character *npc = *iter;
                    c_location->addCharacter(npc, npc->getX(), npc->getY());
                }
                this->addTextEffect(tr("Your rest is disturbed...").toStdString(), player->getPos(), 2000);
            }
        }
        if( c_location->getWanderingMonsterTemplate().length() > 0 && c_location->getWanderingMonsterRestChance(player) > 0 ) {
            int chance = c_location->getWanderingMonsterRestChance(player);
            if( rand() % 100 < chance ) {
                Vector2D free_pvec;
                Character *enemy = this->createCharacter(c_location->getWanderingMonsterTemplate(), c_location->getWanderingMonsterTemplate());
                if( c_location->findFreeWayPoint(&free_pvec, this->player->getPos(), true, enemy->canFly()) ) {
                    rest_ok = false;
                    this->addTextEffect(tr("You are disturbed by a\nwandering monster!").toStdString(), player->getPos(), 2000);
                    enemy->setDefaultPosition(free_pvec.x, free_pvec.y);
                    c_location->addCharacter(enemy, free_pvec.x, free_pvec.y);
                }
                else {
                    delete enemy;
                }
            }
        }
        if( rest_ok ) {
            game_g->stopSound(music_key_combat_c);
            music_mode = MUSICMODE_SILENCE;
            int time = this->getRestTime();
            this->player->restoreHealth();
            this->player->expireProfileEffects();
            // also set all NPCs to no longer be fleeing
            for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
                Character *character = *iter;
                if( character->isFleeing() ) {
                    character->setFleeing(false);
                    character->setStateIdle();
                }
            }
            stringstream str;
            str << "Rested for " << time << " hour";
            if( time > 1 )
                str << "s";
            this->addTextEffect(str.str(), player->getPos(), 2000);
            time_hours += time;
        }
    }
}

bool PlayingGamestate::canSaveHere() {
    // check for c_location != NULL just in case (as this is called e.g. from ::activate())
    return c_location != NULL && !c_location->hasEnemies(this);
}

void PlayingGamestate::clickedSave() {
    LOG("PlayingGamestate::clickedSave()\n");
    if( !this->canSaveHere() ) {
        this->showInfoDialog(tr("You cannot save here - enemies are nearby.").toStdString());
        this->closeSubWindow();
        return;
    }

    if( this->isPermadeath() ) {
        if( this->hasPermadeathSavefilename() ) {
            QString filename = this->getPermadeathSavefilename();
            LOG("permadeath save mode: save as existing file: %s\n", filename.toUtf8().data());
            if( this->saveGame(filename, true) ) {
                this->addTextEffect(tr("The game has been successfully saved").toStdString(), 5000);
            }
            else {
                game_g->showErrorDialog(tr("Failed to save game!").toStdString());
            }
            this->closeSubWindow();
        }
        else {
            SaveGameWindow *saveGameWindow = new SaveGameWindow(this);
            saveGameWindow->requestNewSaveGame();
        }
    }
    else {
        new SaveGameWindow(this);
    }
}

void PlayingGamestate::clickedQuit() {
    LOG("clickedQuit()\n");
    this->closeSubWindow();
    this->quitGame();
}

void PlayingGamestate::showInfoWindow(const string &html) {
    this->showInfoWindow(html, false);
}

void PlayingGamestate::showInfoWindow(const string &html, bool scroll_to_end) {
    // n.b., different to showInfoDialog, as this doesn't block and wait for an answer
    qDebug("showInfoWindow()\n");

    game_g->setPaused(true, true);

    QWidget *subwindow = new CloseSubWindowWidget(this);

    QVBoxLayout *layout = new QVBoxLayout();
    subwindow->setLayout(layout);

    QTextEdit *label = new QTextEdit();
    game_g->setTextEdit(label);
    label->setHtml(html.c_str());
    layout->addWidget(label);

    QPushButton *closeButton = new QPushButton(tr("Continue"));
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeSubWindow()));

    this->addWidget(subwindow, false); // should be last, so resizing is already correct - needed for things like scrolling to bottom to work

    if( scroll_to_end ) {
        label->verticalScrollBar()->setValue( label->verticalScrollBar()->maximum() );
    }
}

void PlayingGamestate::closeSubWindow() {
    LOG("PlayingGamestate::closeSubWindow\n");
    int n_stacked_widgets = this->widget_stack.size();
    if( n_stacked_widgets > 1 ) {
        QWidget *subwindow = this->widget_stack.at(n_stacked_widgets-1);
        /*if( this->main_stacked_widget != NULL ) {
            this->main_stacked_widget->removeWidget(subwindow);
        }*/
        subwindow->hide(); // ensure window is hidden immediately - e.g., needed when a quest is completed and we start a new quest
        subwindow->deleteLater();
        this->widget_stack.erase( this->widget_stack.begin() + n_stacked_widgets-1 );
        if( n_stacked_widgets == 2 ) {
            game_g->getMainWindow()->activateWindow(); // needed for Symbian at least
            game_g->setPaused(false, true);
            this->view->setEnabled(true);
            this->view->grabKeyboard();
            this->view->resetKeyboard();
        }
        else {
            QWidget *subwindow2 = this->widget_stack.at(this->widget_stack.size()-1);
            subwindow2->activateWindow(); // needed for Symbian at least
        }
    }
}

void PlayingGamestate::closeAllSubWindows() {
    LOG("PlayingGamestate::closeAllSubWindows\n");
#if defined(_WIN32)
    game_g->clearOSKButton();
#endif
    while( this->widget_stack.size() > 1 ) {
        LOG("stack size %d\n", this->widget_stack.size());
        QWidget *subwindow = this->widget_stack.at(this->widget_stack.size()-1);
        LOG("delete later: %d\n", subwindow);
        /*if( this->main_stacked_widget != NULL ) {
            this->main_stacked_widget->removeWidget(subwindow);
        }*/
        this->widget_stack.erase(this->widget_stack.begin()+this->widget_stack.size()-1);
        subwindow->hide();
        subwindow->deleteLater();
        LOG("deleted\n");
    }
    LOG("done deleting windows\n");
    game_g->setPaused(false, true);
    LOG("view: %d\n", this->view);
    LOG("about to set enabled\n");
    this->view->setEnabled(true);
    LOG("about to grab keyboard\n");
    this->view->grabKeyboard();
    LOG("about to reset keyboard\n");
    this->view->resetKeyboard();
    LOG("PlayingGamestate::closeAllSubWindows done\n");
}

void PlayingGamestate::quitGame() {
    qDebug("PlayingGamestate::quitGame()");
    //qApp->quit();
    qDebug("ask player to quit");
    if( this->askQuestionDialog(tr("Are you sure you wish to quit?").toStdString()) ) {
        qDebug("player requests quit");
        GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
        game_g->pushMessage(game_message);
    }
}

void PlayingGamestate::testFogOfWar() {
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        if( character != this->player ) {
            bool is_visible = c_location->visibilityTest(player->getPos(), character->getPos());
            character->setVisible(is_visible);
            AnimatedObject *object = static_cast<AnimatedObject *>(character->getListenerData());
            object->setVisible(is_visible);
        }
    }
}

void PlayingGamestate::update() {
    //qDebug("PlayingGamestate::update()");
    // update target item
    if( this->player != NULL ) {
        if( this->player->getTargetNPC() != NULL && this->player->getTargetNPC()->isHostile() && this->player->getTargetNPC()->isVisible() ) {
            Vector2D target_pos = this->player->getTargetNPC()->getPos();
            if( this->target_item == NULL ) {
                //this->target_item = this->addPixmapGraphic(this->target_pixmap, target_pos, 0.5f, false, true);
                AnimatedObject *object = new AnimatedObject();
                this->target_item = object;
                object->addAnimationLayer( target_animation_layer );
                object->setZValue(z_value_gui);
                object->setPos(target_pos.x, target_pos.y);
                this->addGraphicsItem(object, this->player->getTargetNPC()->getImageSize(), false);
            }
            else {
                this->target_item->setPos(target_pos.x, target_pos.y);
            }
        }
        else {
            if( this->target_item != NULL ) {
                delete this->target_item;
                this->target_item = NULL;
            }
        }
    }
    else {
        if( this->target_item != NULL ) {
            delete this->target_item;
            this->target_item = NULL;
        }
    }

    if( this->player == NULL ) {
        return;
    }
//#define TIMING_INFO

    const int elapsed_ms = game_g->getGameTimeTotalMS();

    //qDebug("PlayingGamestate::update() have player");
    bool do_complex_update = false;
    int complex_time_ms = 0; // time since last "complex" update

#if defined(Q_OS_SYMBIAN)
    // use longer updates on Symbian, due to older devices on average having lower performance
    const int complex_update_time_ms = 200;
#else
    const int complex_update_time_ms = 100;
#endif
    //qDebug("time: compare: %d vs %d", elapsed_ms, time_last_complex_update_ms);
    if( elapsed_ms - this->time_last_complex_update_ms > complex_update_time_ms ) {
        if( time_last_complex_update_ms == 0 ) {
            complex_time_ms = game_g->getGameTimeFrameMS();
        }
        else {
            complex_time_ms = elapsed_ms - time_last_complex_update_ms;
        }
        this->time_last_complex_update_ms = elapsed_ms;
        do_complex_update = true;

        // we need to run the update() loop at a fast rate (preferably 60FPS) so that the movement and animation is updated as smoothly as possible on fast systems
        // but we don't need to run other things every frame (and doing so risks the update taking too long on slower systems, leading to a spiral of death)
    }

    if( do_complex_update ) {
#ifdef TIMING_INFO
        QElapsedTimer timer_complex;
        timer_complex.start();
#endif

        {
            int next_level_xp = this->player->getXPForNextLevel();
            //qDebug("compare: %d vs %d", this->player->getXP(), next_level_xp);
            if( this->player->getXP() >= next_level_xp ) {
                // we only advance one level at any given increase
                this->player->advanceLevel(this);
            }
        }

        // we can get away with not updating this every call
        this->testFogOfWar();

        if( this->need_visibility_update ) {
            // important for performance not to be called every frame (especially on older devices like Nokia 5800, with large locations)
            this->updateVisibility(player->getPos());
        }

        for(set<Scenery *>::iterator iter = c_location->scenerysBegin(); iter != c_location->scenerysEnd(); ++iter) {
            Scenery *scenery = *iter;
            if( scenery->getActionType().length() > 0 ) {
                if( elapsed_ms >= scenery->getActionLastTime() + scenery->getActionDelay() ) {
                    bool has_effect = false;
                    if( scenery->getActionType() == "harm_player" ) {
                        if( !player->isDead() && scenery->isOn(player) ) {
                            LOG("scenery %s harms player\n", scenery->getName().c_str());
                            has_effect = true;
                            player->decreaseHealth(this, scenery->getActionValue(), false, false);
                        }
                    }
                    else {
                        LOG("unknown action type: %s\n", scenery->getActionType().c_str());
                        ASSERT_LOGGER(false);
                    }

                    if( has_effect ) {
                        scenery->setActionLastTime(elapsed_ms);
                    }
                }
            }
        }

        // terror
        for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
            Character *character = *iter;
            if( character != player && character->isVisible() && character->getCausesTerror() && !character->hasDoneTerror() ) {
                int roll = rollDice(2, 6, character->getTerrorEffect());
                int bravery = player->getProfileIntProperty(profile_key_B_c);
                qDebug("Terror? Roll %d vs %d", roll, bravery);
                if( roll > bravery )
                {
                    player->paralyse(5000);
                    this->addTextEffect(tr("You are too terrified to move!").toStdString(), player->getPos(), 5000);
                }
                character->setDoneTerror(true);
            }
        }

        // spawning wandering monsters
        if( c_location->getWanderingMonsterTemplate().length() > 0 && c_location->getWanderingMonsterTimeMS() > 0 ) {
            int prob = poisson(c_location->getWanderingMonsterTimeMS(), complex_time_ms);
            int roll = rand();
            //qDebug("prob: %d vs %d (update frame time %d, rate %d)", roll, prob, complex_time_ms, c_location->getWanderingMonsterTimeMS());
            if( roll < prob ) {
                Vector2D free_pvec;
                Character *enemy = this->createCharacter(c_location->getWanderingMonsterTemplate(), c_location->getWanderingMonsterTemplate());
                if( c_location->findFreeWayPoint(&free_pvec, this->player->getPos(), false, enemy->canFly()) ) {
                    qDebug("spawn wandering monster at %f, %f", free_pvec.x, free_pvec.y);
                    enemy->setDefaultPosition(free_pvec.x, free_pvec.y);
                    c_location->addCharacter(enemy, free_pvec.x, free_pvec.y);
                }
                else {
                    delete enemy;

                }
            }
        }

        // music
        if( has_ingame_music ) {
            if( music_mode != MUSICMODE_COMBAT ) {
                bool enemy_visible = false;
                for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd() && !enemy_visible; ++iter) {
                    Character *character = *iter;
                    if( character != player && character->isVisible() && character->isHostile() ) {
                        enemy_visible = true;
                    }
                }
                if( enemy_visible ) {
                    game_g->playSound(music_key_combat_c, true);
                    music_mode = MUSICMODE_COMBAT;
                    this->time_combat_ended = -1;
                }
            }
            else {
                if( c_location->hasEnemies(this) ) {
                    this->time_combat_ended = -1;
                }
                else {
                    if( this->time_combat_ended == -1 ) {
                        this->time_combat_ended = elapsed_ms;
                    }
                    else {
                        if( elapsed_ms - time_combat_ended > 5000 ) {
                            game_g->fadeSound(music_key_combat_c);
                            music_mode = MUSICMODE_SILENCE;
                        }
                    }
                }
            }
        }

        //qDebug("complex update done");
#ifdef TIMING_INFO
        qDebug("complex update took %d", timer_complex.elapsed());
#endif
    }

    {
#ifdef TIMING_INFO
        QElapsedTimer timer_kinput;
        timer_kinput.start();
#endif
        //qDebug("update due to keyboard input");
        // update due to keyboard input
        // note that this doesn't actually move the player directly, but sets a target direction to move to

        // n.b., we need to call setDirection() on the player, so that the direction still gets set even if the player can't move in that direction (e.g., blocked by NPC or scenery) - needed to we can still do Action on that NPC or scenery!
        bool moved = false;
        Vector2D dest = player->getPos();
        const float step = 0.25f; // should be <= npc_radius_c
        ASSERT_LOGGER( step <= npc_radius_c );
        if( this->view->keyDown(MainGraphicsView::KEY_L) ) {
            moved = true;
            dest.x -= step;
        }
        else if( this->view->keyDown(MainGraphicsView::KEY_R) ) {
            moved = true;
            dest.x += step;
        }
        if( this->view->keyDown(MainGraphicsView::KEY_U) ) {
            moved = true;
            dest.y -= step;
        }
        else if( this->view->keyDown(MainGraphicsView::KEY_D) ) {
            moved = true;
            dest.y += step;
        }
        if( this->view->keyDown(MainGraphicsView::KEY_LU) ) {
            moved = true;
            dest.x -= step;
            dest.y -= step;
        }
        else if( this->view->keyDown(MainGraphicsView::KEY_RU) ) {
            moved = true;
            dest.x += step;
            dest.y -= step;
        }
        else if( this->view->keyDown(MainGraphicsView::KEY_LD) ) {
            moved = true;
            dest.x -= step;
            dest.y += step;
        }
        else if( this->view->keyDown(MainGraphicsView::KEY_RD) ) {
            moved = true;
            dest.x += step;
            dest.y += step;
        }
        if( moved && player->isDoingAction() ) {
            // moving will cancel the current action - only do this if changing direction
            Vector2D old_dir = player->getDirection();
            Vector2D new_dir = dest - player->getPos();
            new_dir.normalise();
            if( new_dir % old_dir > E_TOL_LINEAR ) {
                // don't cancel action
                moved = false;
                this->is_keyboard_moving = false;
                qDebug("cancel keyboard move, to avoid cancelling action");
            }
        }
        if( moved ) {
            //qDebug("keyboard move");
            this->is_keyboard_moving = true;
            this->player->setDirection(dest - player->getPos());
            this->view->centreOnPlayer();
            // check if we need to update the target
            bool need_update = true;
            if( this->player->hasPath() ) {
                Vector2D current_dest = this->player->getDestination();
                Vector2D current_dir = current_dest - this->player->getPos();
                if( current_dir.magnitude() > 0.5f*step ) {
                    Vector2D new_dir = dest - player->getPos();
                    current_dir.normalise();
                    new_dir.normalise();
                    if( current_dir.isEqual(new_dir, E_TOL_LINEAR) ) {
                        need_update = false;
                    }
                }
            }
            if( need_update ) {
                //qDebug("update");
                this->requestPlayerMove(dest, NULL);
            }
            else {
                //qDebug("no update");
            }
        }
        else if( this->is_keyboard_moving ) {
            qDebug("stopping keyboard movement");
            this->is_keyboard_moving = false;
            this->player->setStateIdle(); // needed, as for keyboard movement this may not have been done by the Character::update()
        }
        //qDebug("keyboard input done");
#ifdef TIMING_INFO
        qDebug("keyboard input took %d", timer_kinput.elapsed());
#endif
    }

#ifdef TIMING_INFO
    QElapsedTimer timer_advance;
    timer_advance.start();
#endif
    //qDebug("advance scene");
    scene->advance();
    //qDebug("advance scene done");
#ifdef TIMING_INFO
    qDebug("scene advance took %d", timer_advance.elapsed());
#endif


    vector<CharacterAction *> delete_character_actions;
    for(vector<CharacterAction *>::iterator iter = this->character_actions.begin(); iter != this->character_actions.end(); ++iter) {
        CharacterAction *character_action = *iter;
        if( character_action->isExpired() ) {
            delete_character_actions.push_back(character_action);
        }
        else {
            character_action->update();
        }
    }
    for(vector<CharacterAction *>::iterator iter = delete_character_actions.begin(); iter != delete_character_actions.end(); ++iter) {
        CharacterAction *character_action = *iter;
        for(vector<CharacterAction *>::iterator iter2 = this->character_actions.begin(); iter2 != this->character_actions.end(); ++iter2) {
            CharacterAction *character_action2 = *iter2;
            if( character_action2 == character_action ) {
                this->character_actions.erase(iter2);
                break;
            }
        }
        character_action->implement(this);
        delete character_action;
    }

#ifdef TIMING_INFO
    QElapsedTimer timer_cupdate;
    timer_cupdate.start();
#endif
    // Character::update() also handles movement, so need to do that with every update() call
    // though we could split out the AI etc to a separate function, as that doesn't need to be done with every update() call
    vector<Character *> delete_characters;
    //LOG("update characters\n");
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        //LOG("update: %s\n", character->getName().c_str());
        if( character->update(this) ) {
            LOG("character is about to die: %s\n", character->getName().c_str());
            delete_characters.push_back(character);
        }
    }
#ifdef TIMING_INFO
        qDebug("character update took %d", timer_cupdate.elapsed());
#endif
    //LOG("done update characters\n");
    // handle deleted characters
#ifdef TIMING_INFO
        QElapsedTimer timer_dcharacters;
        timer_dcharacters.start();
#endif
    for(vector<Character *>::iterator iter = delete_characters.begin(); iter != delete_characters.end(); ++iter) {
        Character *character = *iter;
        LOG("character has died: %s\n", character->getName().c_str());
        c_location->removeCharacter(character);
        //LOG("done removing character from location\n");

        for(set<Character *>::iterator iter3 = c_location->charactersBegin(); iter3 != c_location->charactersEnd(); ++iter3) {
            Character *ch = *iter3;
            if( ch->getTargetNPC() == character ) {
                ch->setTargetNPC(NULL);
            }
        }
        for(vector<CharacterAction *>::iterator iter = this->character_actions.begin(); iter != this->character_actions.end(); ++iter) {
            CharacterAction *character_action = *iter;
            character_action->notifyDead(character);
        }

        if( character == this->player ) {
            //LOG("player has died\n");
            if( this->permadeath && this->permadeath_has_savefilename ) {
                QString full_path = this->permadeath_savefilename;
                if( QFile::remove(full_path) ) {
                    LOG("permadeath mode: removed save filename: %s\n", full_path.toUtf8().data());
                }
                else {
                    LOG("permadeath mode: failed to remove save filename!: %s\n", full_path.toUtf8().data());
                }
            }
            stringstream death_message;
            int r = rand() % 4;
            if( r == 0 ) {
                death_message << "<p><b>Game over</b></p><p>You have died! Your noble quest has come to an end. Your corpse rots away, left for future brave adventurers to encounter.</p>";
            }
            else if( r == 1 ) {
                death_message << "<p><b>Game over</b></p><p>You have died! Your adventure has met an untimely end. Better luck next time!</p>";
            }
            else if( r == 2 ) {
                death_message << "<p><b>Game over</b></p><p>Ooh, nasty!</p>";
            }
            else {
                death_message << "<p><b>Game over</b></p><p>You are dead! Your time on this mortal plane is over, and your adventure ends here.</p>";
            }
            death_message << "<p><b>Achieved Level:</b> " << player->getLevel() << "<br/><b>Achieved XP:</b> " << player->getXP() << "</p>";
            game_g->playSound(music_key_game_over_c, false);
            music_mode = MUSICMODE_SILENCE;
            this->showInfoDialog(death_message.str(), string(DEPLOYMENT_PATH) + "gfx/scenes/death.jpg");

            this->player = NULL;
            GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
            game_g->pushMessage(game_message);
        }
        else {
            //LOG("npc has died\n");
            if( music_mode == MUSICMODE_COMBAT ) {
                if( !c_location->hasEnemies(this) ) {
                    // immediately stop music after killing the last nearby enemy
                    game_g->fadeSound(music_key_combat_c);
                    music_mode = MUSICMODE_SILENCE;
                }
            }
        }
        //LOG("about to delete: %s\n", character->getName().c_str());
        delete character; // also removes character from the QGraphicsScene, via the listeners
        //LOG("done delete character\n");
    }
    if( this->player != NULL && delete_characters.size() > 0 ) {
        this->checkQuestComplete();
    }
#ifdef TIMING_INFO
        qDebug("deleted characters took %d", timer_dcharacters.elapsed());
#endif
    //qDebug("PlayingGamestate::update() exit");

}

void PlayingGamestate::updateInput() {
    if( this->player == NULL ) {
        return;
    }

    // scroll
    bool scrolled = false;
    int real_time_ms = game_g->getInputTimeFrameMS();
    float speed = (4.0f * real_time_ms)/1000.0f;
    if( !touchscreen_c ) {
        // scroll due to mouse at edge of screen
        int m_x = QCursor::pos().x();
        int m_y = QCursor::pos().y();
        // use screenGeometry() instead of availableGeometry(), as we don't care about the taskbar etc (we want to detect if mouse is at the edge of the screen, not below the taskbar - which will be hidden in fullscreen mode anyway)
        int screen_width = QApplication::desktop()->screenGeometry().width();
        int screen_height = QApplication::desktop()->screenGeometry().height();
        //qDebug("mouse at %d, %d", m_x, m_y);
        //qDebug("screen size %d x %d", screen_width, screen_height);
        QPointF centre = this->view->mapToScene( this->view->rect() ).boundingRect().center();
        if( m_x <= 0 ) {
            centre.setX( centre.x() - speed );
            this->view->centerOn(centre);
            scrolled = true;
        }
        else if( m_x >= screen_width-1 ) {
            centre.setX( centre.x() + speed );
            this->view->centerOn(centre);
            scrolled = true;
        }
        if( m_y <= 0 ) {
            centre.setY( centre.y() - speed );
            this->view->centerOn(centre);
            scrolled = true;
        }
        else if( m_y >= screen_height-1 ) {
            centre.setY( centre.y() + speed );
            this->view->centerOn(centre);
            scrolled = true;
        }
    }
    if( !scrolled && !touchscreen_c && (qApp->mouseButtons() & Qt::LeftButton)==0 ) {
        // scroll due to player near the edge
        // disabled for touchscreens, as makes drag-scrolling harder
        QPoint player_pos = this->view->mapFromScene(this->player->getX(), this->player->getY());
        if( player_pos.x() >= 0 && player_pos.x() < this->view->width() && player_pos.y() > 0 && player_pos.y() < this->view->height() ) {
            const int wid = 64;
            QPointF centre = this->view->mapToScene( this->view->rect() ).boundingRect().center();
            if( player_pos.x() < wid ) {
                centre.setX( centre.x() - speed );
                this->view->centerOn(centre);
                scrolled = true;
            }
            else if( player_pos.x() >= this->view->width() - wid ) {
                centre.setX( centre.x() + speed );
                this->view->centerOn(centre);
                scrolled = true;
            }
            if( player_pos.y() < wid ) {
                centre.setY( centre.y() - speed );
                this->view->centerOn(centre);
                scrolled = true;
            }
            else if( player_pos.y() >= this->view->height() - wid ) {
                centre.setY( centre.y() + speed );
                this->view->centerOn(centre);
                scrolled = true;
            }
        }
    }

    this->view->updateInput();
}

void PlayingGamestate::render() {
    // n.b., won't render immediately, but schedules for repainting from Qt's main event loop
    //qDebug("render");
    this->view->viewport()->update();
}

void PlayingGamestate::displayPausedMessage() {
    // note, we don't display a pause message every time the game is paused - e.g., when it's due to opening a subwindow - but only when it might not be clear to the user that the game is paused
    string paused_message = touchscreen_c ?
                "Game paused\nTouch screen to continue" :
                "Game paused\nClick or press a key to continue";
    this->addTextEffect(paused_message, 0); // 0 time, so the message disappears as soon as the game is unpaused
}

void PlayingGamestate::activate(bool active, bool newly_paused) {
    if( !active && this->canSaveHere() ) {
        LOG("save game due to going inactive\n");
        this->autoSave();
    }
    //this->addTextEffect(active ? "activated" : "deactivated", 10000);
    if( newly_paused ) {
        this->displayPausedMessage();
    }
}

void PlayingGamestate::processTouchEvent(QTouchEvent *touchEvent) {
    this->view->processTouchEvent(touchEvent);
}

void PlayingGamestate::checkQuestComplete() {
    qDebug("PlayingGamestate::checkQuestComplete()");
    if( !this->quest->isCompleted() && this->quest->testIfComplete(this) ) {
        //game_g->showInfoDialog("Quest complete", "You have completed the quest! Now return to the dungeon exit.");
        this->showInfoDialog(tr("Quest complete!\n\nYou have completed the quest, you should now return to the dungeon exit.").toStdString());
    }
    //qDebug("PlayingGamestate::checkQuestComplete() end");
}

void PlayingGamestate::characterUpdateGraphics(const Character *character, void *user_data) {
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    if( object == NULL ) {
        throw string("character has NULL graphics object");
    }
    object->clearAnimationLayers();
    if( character == player ) {
        LazyAnimationLayer *lazy_animation_layer = this->animation_layers["player"];
        if( lazy_animation_layer == NULL ) {
            throw string("can't find lazy_animation_layer for player");
        }
        AnimationLayer *animation_layer = lazy_animation_layer->getAnimationLayer();
        object->addAnimationLayer( animation_layer );
        if( character->getCurrentWeapon() != NULL && character->getCurrentWeapon()->getAnimationName().length() > 0 ) {
            object->addAnimationLayer( this->animation_layers[ character->getCurrentWeapon()->getAnimationName() ]->getAnimationLayer() );
        }
        if( character->getCurrentShield() != NULL && character->getCurrentShield()->getAnimationName().length() > 0 ) {
            object->addAnimationLayer( this->animation_layers[ character->getCurrentShield()->getAnimationName() ]->getAnimationLayer() );
        }
    }
    else {
        LazyAnimationLayer *lazy_animation_layer = this->animation_layers[character->getAnimationName()];
        if( lazy_animation_layer == NULL ) {
            throw string("can't find lazy_animation_layer for: " + character->getAnimationName());
        }
        AnimationLayer *animation_layer = lazy_animation_layer->getAnimationLayer();
        object->addAnimationLayer( animation_layer );
    }
}

void PlayingGamestate::characterTurn(const Character *character, void *user_data) {
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    Vector2D dir = character->getDirection();
    Direction direction = directionFromVecDir(dir);
    object->setDimension(direction);
}

void PlayingGamestate::characterMoved(Character *character, void *user_data) {
    //QGraphicsItem *item = static_cast<QGraphicsItem *>(user_data);
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    QPointF old_pos = object->pos();
    QPointF new_pos(character->getX(), character->getY());
    if( new_pos != old_pos ) {
        object->setPos(new_pos);
        object->setZValue( object->pos().y() );
        /*Direction direction = DIRECTION_W;
        if( new_pos.x() > old_pos.x() ) {
            direction = DIRECTION_E;
        }*/
        QPointF dir = new_pos - old_pos;
        Vector2D vdir(dir.x(), dir.y());
        if( !( character == this->player && this->is_keyboard_moving ) ) {
            // if using keyboard movement, then the direction is already set (and the UI works better if the direction comes from that requested by the player - e.g., consider walking against a diagonal scenery boundary)
            character->setDirection(vdir);
        }

        if( character == this->player ) {
            // handle popup text
            Vector2D old_pos_v(old_pos.x(), old_pos.y());
            Vector2D new_pos_v = character->getPos();
            for(set<Scenery *>::const_iterator iter = this->c_location->scenerysBegin(); iter != this->c_location->scenerysEnd(); ++iter) {
                const Scenery *scenery = *iter;
                if( scenery->getPopupText().length() > 0 ) {
                    Vector2D scenery_pos = scenery->getPos();
                    float old_dist = (scenery_pos - old_pos_v).magnitude();
                    float new_dist = (scenery_pos - new_pos_v).magnitude();
                    const float influence_radius_c = 3.0f;
                    if( new_dist <= influence_radius_c && old_dist > influence_radius_c ) {
                        qDebug("popup text: dists %f, %f ; at %f, %f", old_dist, new_dist, scenery->getX(), scenery->getY());
                        this->addTextEffect(scenery->getPopupText(), scenery->getPos(), 2000);
                    }
                }
            }
        }
    }
    if( character == this->player ) {
        // handle traps
        vector<Trap *> delete_traps;
        for(set<Trap *>::iterator iter = this->c_location->trapsBegin(); iter != this->c_location->trapsEnd(); ++iter) {
            Trap *trap = *iter;
            if( trap->isSetOff(character) ) {
                trap->setOff(this, character);
                delete_traps.push_back(trap);
                if( character->isDead() ) {
                    break;
                }
            }
        }
        for(vector<Trap *>::iterator iter = delete_traps.begin(); iter != delete_traps.end(); ++iter) {
            Trap *trap = *iter;
            c_location->removeTrap(trap);
        }

        if( !character->isDead() ) {
            this->need_visibility_update = true;
        }
    }

}

void PlayingGamestate::characterSetAnimation(const Character *, void *user_data, const string &name, bool force_restart) {
    //LOG("PlayingGamestate::characterSetAnimation(%s, %d, %s, %d)\n", character->getName().c_str(), user_data, name.c_str(), force_restart);
    //LOG("    at %d\n", game_g->getScreen()->getGameTimeTotalMS());
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    object->setAnimationSet(name, force_restart);
}

void PlayingGamestate::characterDeath(Character *character, void *user_data) {
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    character->setListener(NULL, NULL);
    delete object;
}

/*void PlayingGamestate::mouseClick(int m_x, int m_y) {
    qDebug("PlayingGamestate::mouseClick(%d, %d)", m_x, m_y);
}*/

void PlayingGamestate::clickedOnNPC(Character *character) {
    // should be called when clicking, even if character is NULL
    if( character != NULL ) {
        float dist_from_player = (player->getPos() - character->getPos()).magnitude();
        if( dist_from_player <= talk_range_c ) {
            if( character->canTalk() ) {
                stringstream message;
                message << "<b>";
                message << character->getName();
                message << "</b>: ";
                if( character->isInteractionCompleted() ) {
                    message << character->getTalkOpeningInteractionComplete();
                }
                else if( !character->hasTalked() ) {
                    message << character->getTalkOpeningInitial();
                }
                else {
                    message << character->getTalkOpeningLater();
                }
                message << "<br/>";
                character->setHasTalked(true);
                for(;;) {
                    vector<string> buttons;
                    //vector<string> answers;
                    vector<TalkItem *> talk_items;
                    if( !character->isHostile() ) {
                        // being hostile means the character has become hostile whilst talking, so now the only thing left is to say Goodbye!
                        int count=0;
                        for(vector<TalkItem>::iterator iter = character->talkItemsBegin(); iter != character->talkItemsEnd(); ++iter, count++) {
                            TalkItem *talk_item = &*iter;
                            if( talk_item->while_not_done && character->isInteractionCompleted() ) {
                                continue;
                            }
                            if( talk_item->objective && !character->canCompleteInteraction(this) ) {
                                continue;
                            }
                            buttons.push_back(talk_item->question);
                            //answers.push_back(talk_item->answer);
                            talk_items.push_back(talk_item);
                        }
                    }
                    buttons.push_back(tr("Goodbye").toStdString());
                    InfoDialog *dialog = new InfoDialog(message.str(), "", NULL, buttons, false, true, true);
                    this->addWidget(dialog, false);
                    dialog->scrollToBottom();
                    int result = dialog->exec();
                    this->closeSubWindow();
                    ASSERT_LOGGER(result >= 0);
                    ASSERT_LOGGER(result < buttons.size());
                    if( result == buttons.size()-1 ) {
                        break;
                    }
                    else {
                        TalkItem *talk_item = talk_items.at(result);
                        message << "<b>";
                        message << player->getName();
                        message << "</b>: ";
                        message << buttons.at(result);
                        message << "<br/>";

                        message << "<b>";
                        message << character->getName();
                        message << "</b>: ";
                        //message << answers.at(result);
                        message << talk_item->answer;
                        message << "<br/>";
                        if( talk_item->journal.length() > 0 ) {
                            this->writeJournal("<hr/><p>");
                            this->writeJournalDate();
                            this->writeJournal(talk_item->journal);
                            this->writeJournal("</p>");
                            talk_item->journal = ""; // stops the journal text being written repeatedly
                        }
                        if( talk_item->objective ) {
                            character->completeInteraction(this);
                        }
                        if( talk_item->action.length() > 0 ) {
                            if( talk_item->action == "SET_HOSTILE") {
                                character->setHostile(true);
                            }
                            else {
                                LOG("character %s, talk item %s has unknown action: %s\n", character->getName().c_str(), talk_item->question.c_str(), talk_item->action.c_str());
                                ASSERT_LOGGER(false);
                            }
                        }
                    }
                }
            }
            else if( character->getShop().length() > 0 ) {
                const Shop *shop = NULL;
                for(vector<Shop *>::const_iterator iter = this->shopsBegin(); iter != this->shopsEnd() && shop==NULL; ++iter) {
                    const Shop *this_shop = *iter;
                    if( this_shop->getName() == character->getShop() ) {
                        shop = this_shop;
                    }
                }
                ASSERT_LOGGER(shop != NULL);
                if( shop != NULL ) {
                    LOG("visit shop: %s\n", shop->getName().c_str());
                    new TradeWindow(this, shop->getItems(), shop->getCosts());
                }
            }
        }
    }
    player->setTargetNPC(character); // n.b., if no NPC selected, we therefore set to NULL
}

bool PlayingGamestate::handleClickForItems(Vector2D dest) {
    qDebug("PlayingGamestate::handleClickForItems");
    bool done = false;
    //float min_dist = 0.0f;
    const float click_tol_items_c = 0.25f;
    // search for clicking on an item
    vector<Item *> pickup_items;
    for(set<Item *>::iterator iter = c_location->itemsBegin(); iter != c_location->itemsEnd(); ++iter) {
        Item *item = *iter;
        float icon_width = item->getIconWidth();
        if( icon_width < default_icon_width_c )
            icon_width = default_icon_width_c; // this avoids the problem where it's hard to pick up a small item - also avoids problem with multiple items dropped at same location, of different size (e.g., Red Gem problem with Goblin in 1st quest)
        float dist_from_click = (dest - item->getPos()).magnitude();
        float dist_from_player = (player->getPos() - item->getPos()).magnitude();
        if( dist_from_click <= sqrt(0.5f) * icon_width + click_tol_items_c && dist_from_player <= npc_radius_c + sqrt(0.5f)*icon_width ) {
            done = true;
            //min_dist = dist_from_click;
            pickup_items.push_back(item);
        }
    }
    if( pickup_items.size() == 1 ) {
        Item *picked_item = pickup_items.at(0);
        this->addTextEffect(picked_item->getName(), player->getPos(), 2000);
        if( picked_item->getType() == ITEMTYPE_CURRENCY ) {
            this->playSound("coin");
        }
        player->pickupItem(picked_item);
        this->checkQuestComplete();
    }
    else if( pickup_items.size() > 1 ) {
        new ItemsPickerWindow(this, pickup_items);
        game_g->setPaused(true, true);
    }
    return done;
}

bool PlayingGamestate::interactWithScenery(bool *move, void **ignore, Scenery *scenery) {
    LOG("PlayingGamestate::interactWithScenery: %s\n", scenery->getName().c_str());
    bool done = false;

    if( scenery->getTrap() != NULL ) {
        scenery->getTrap()->setOff(this, player);
        scenery->setTrap(NULL);
        if( player->isDead() ) {
            done = true;
            return done;
        }
    }

    bool is_locked = false;
    if( scenery->isLocked() ) {
        // can we unlock it?
        is_locked = true;
        string unlock_item_name = scenery->getUnlockItemName();
        Item *item = NULL;
        if( unlock_item_name.length() > 0 ) {
            qDebug("search for %s", unlock_item_name.c_str());
            for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd() && is_locked; ++iter) {
                item = *iter;
                //LOG("    compare to: %s\n", item->getKey().c_str());
                if( item->getType() == ITEMTYPE_WEAPON && player->getCurrentWeapon() != item )
                    continue;
                else if( item->getType() == ITEMTYPE_ARMOUR && player->getCurrentArmour() != item )
                    continue;
                else if( item->getType() == ITEMTYPE_SHIELD && player->getCurrentShield() != item )
                    continue;
                else if( item->getType() == ITEMTYPE_RING && player->getCurrentRing() != item )
                    continue;
                if( item->getKey() == unlock_item_name ) {
                    is_locked = false;
                }
            }
        }
        if( is_locked ) {
            done = true;
            if( !scenery->isLockedSilent() ) {
                this->playSound("lock");
            }
            if( scenery->getLockedText().length() == 0 ) {
                stringstream str;
                str << "The " << scenery->getName() << " is locked!";
                this->addTextEffect(str.str(), player->getPos(), 2000);
            }
            else if( scenery->getLockedText() != "none" ) {
                this->addTextEffect(scenery->getLockedText(), player->getPos(), 2000);
            }
        }
        else {
            ASSERT_LOGGER(item != NULL);
            stringstream str;
            if( scenery->getUnlockText().length() == 0 ) {
                str << "You unlock the " << scenery->getName() << ".";
            }
            else {
                str << scenery->getUnlockText();
            }
            this->addTextEffect(str.str(), player->getPos(), 2000);
            qDebug("isKeyAlwaysNeeded? %d", scenery->isKeyAlwaysNeeded());
            if( !scenery->isKeyAlwaysNeeded() ) {
                scenery->setLocked(false);
            }
            qDebug("isLockedUsedUp? %d", scenery->isLockedUsedUp());
            if( scenery->isLockedUsedUp() ) {
                player->takeItem(item);
                delete item;
                item = NULL;
            }
            player->addXP(this, scenery->getUnlockXP());
        }
    }

    if( !is_locked ) {
        if( scenery->getNItems() > 0 ) {
            done = true;
            bool all_gold = true;
            for(set<Item *>::iterator iter = scenery->itemsBegin(); iter != scenery->itemsEnd(); ++iter) {
                Item *item = *iter;
                if( item->getType() != ITEMTYPE_CURRENCY ) {
                    all_gold = false;
                }
                c_location->addItem(item, player->getX(), player->getY());
            }
            scenery->eraseAllItems();
            this->addTextEffect(all_gold ? "Found some gold!" : "Found some items!", player->getPos(), 2000);
        }

        if( scenery->canBeOpened() && !scenery->isOpened() ) {
            done = true;
            this->playSound("container");
            scenery->setOpened(true);
        }

        if( scenery->isDoor() ) {
            done = true;
            qDebug("clicked on a door");
            // open door
            if( scenery->getName() == "Door" ) {
                this->playSound("door");
            }
            this->addTextEffect(tr("Opening door...").toStdString(), scenery->getPos(), 1000);
            qApp->processEvents(); // so that the text effect gets displayed, whilst recalculating the location's distance graph
            c_location->removeScenery(scenery);
            delete scenery;
            scenery = NULL;
            *ignore = NULL;
        }
        else if( scenery->isExit() ) {
            done = true;
            LOG("clicked on an exit\n");
            // exit
            if( scenery->getName() == "Door" ) {
                this->playSound("door");
            }
            this->closeSubWindow(); // just in case
#if !defined(Q_OS_SYMBIAN) // autosave disabled due to being slow on Nokia 5800 at least
            this->autoSave();
#endif
            if( this->quest->getQuestObjective() != NULL && this->quest->getQuestObjective()->getType() == "find_exit" && this->quest->getQuestObjective()->getArg1() == scenery->getName() ) {
                this->quest->setCompleted(true);
            }

            this->quest->testIfComplete(this); // recall, in case quest no longer completed (e.g., player has dropped an item that is needed)
            if( this->getQuest()->isCompleted() ) {
                this->getQuest()->getQuestObjective()->completeQuest(this);
                /*int gold = this->getQuest()->getQuestObjective()->getGold();
                this->player->addGold(gold);*/
                string completed_text = this->getQuest()->getCompletedText();
                completed_text = convertToHTML(completed_text);
                this->showInfoDialog(completed_text);
                this->writeJournal("<hr/><p>");
                this->writeJournalDate();
                this->writeJournal(completed_text);
                this->writeJournal("</p>");
            }
            game_g->stopSound(music_key_combat_c);
            music_mode = MUSICMODE_SILENCE;
            new CampaignWindow(this);
            game_g->setPaused(true, true);
            this->time_hours += 48 + this->getRestTime();
            this->player->restoreHealth();
            this->player->expireProfileEffects();
        }
        else if( scenery->getExitLocation().length() > 0 ) {
            done = true;
            if( scenery->getName() == "Door" ) {
                this->playSound("door");
            }
            this->time_hours += scenery->getExitTravelTime();
            LOG("clicked on an exit location: %s travel time %d\n", scenery->getExitLocation().c_str(), scenery->getExitTravelTime());
            Location *new_location = quest->findLocation(scenery->getExitLocation());
            ASSERT_LOGGER(new_location != NULL);
            if( new_location != NULL ) {
#if !defined(Q_OS_SYMBIAN) // autosave disabled due to being slow on Nokia 5800 at least
                this->autoSave();
#endif
                game_g->stopSound(music_key_combat_c);
                music_mode = MUSICMODE_SILENCE;
                this->moveToLocation(new_location, scenery->getExitLocationPos());
                *move = false;
                int travel_time_hours = scenery->getExitTravelTime();
                if( travel_time_hours > 0 ) {
                    int travel_time_days = travel_time_hours / 24;
                    stringstream str;
                    if( travel_time_days > 0 ) {
                        str << "Journey took " << travel_time_days << " day";
                        if( travel_time_days > 1 )
                            str << "s";
                    }
                    else {
                        str << "Journey took " << travel_time_hours << " hour";
                        if( travel_time_hours > 1 )
                            str << "s";
                    }
                    this->addTextEffect(str.str(), player->getPos(), 2000);
                }
                LOG("moved okay\n");
            }
        }
        else if( scenery->getInteractType().length() > 0 ) {
            done = true;
            LOG("interact_type: %s\n", scenery->getInteractType().c_str());
            string dialog_text;
            vector<string> options = scenery->getInteractionText(this, &dialog_text);
            if( options.size() == 0 ) {
                // auto-interact
                scenery->interact(this, 0);
            }
            else {
                //if( this->askQuestionDialog(dialog_text) ) {
                //InfoDialog *dialog = InfoDialog::createInfoDialogYesNo(dialog_text);
                InfoDialog *dialog = new InfoDialog(dialog_text, "", NULL, options, false, false, true);
                this->addWidget(dialog, false);
                int result = dialog->exec();
                LOG("scenery iteraction dialog returns %d\n", result);
                this->closeSubWindow();
                if( result != options.size()-1 ) {
                    scenery->interact(this, result);
                }
            }
        }
        else if( scenery->getDescription().length() > 0 ) {
            done = true;
            string description = scenery->getDescription();
            description = convertToHTML(description);
            this->showInfoDialog(description, string(DEPLOYMENT_PATH) + scenery->getBigImageName());
        }
    }
    return done;
}

bool PlayingGamestate::clickedOnScenerys(bool *move, void **ignore, const vector<Scenery *> &clicked_scenerys) {
    bool done = false;
    for(vector<Scenery *>::const_iterator iter = clicked_scenerys.begin(); iter != clicked_scenerys.end() && !done; ++iter) {
        Scenery *scenery = *iter;
        qDebug("clicked on scenery: %s", scenery->getName().c_str());

        if( scenery->getRequiresFlag().length() > 0 && !this->quest->hasFlag( scenery->getRequiresFlag() ) ) {
            qDebug("but doesn't have required flag: %s", scenery->getRequiresFlag().c_str());
            continue;
        }

        bool confirm_ok = true;
        if( scenery->getConfirmText().length() > 0 ) {
            confirm_ok = this->askQuestionDialog(scenery->getConfirmText());
            if( !confirm_ok ) {
                done = true; // important so that we don't repeatedly ask (e.g., needed for the keyboard input actionCommand() which calls this function at multiple positions)
            }
        }

        if( confirm_ok ) {
            done = this->interactWithScenery(move, ignore, scenery);
        }
    }
    return done;
}

bool PlayingGamestate::handleClickForScenerys(bool *move, void **ignore, Vector2D dest, bool is_click) {
    //qDebug("PlayingGamestate::handleClickForScenerys(): %f, %f", dest.x, dest.y);
    // search for clicking on a scenery
    const float click_tol_scenery_c = touchscreen_c ? 0.2f : 0.0f;
    vector<Scenery *> clicked_scenerys;
    for(set<Scenery *>::iterator iter = c_location->scenerysBegin(); iter != c_location->scenerysEnd(); ++iter) {
        Scenery *scenery = *iter;
        /*Vector2D scenery_pos;
        float scenery_width = 0.0f, scenery_height = 0.0f;
        scenery->getBox(&scenery_pos, &scenery_width, &scenery_height, is_click);
        float dist_from_click = distFromBox2D(scenery_pos, scenery_width, scenery_height, dest);*/
        float dist_from_click = scenery->distFromPoint(dest, is_click);
        //qDebug("dist_from_click for scenery %s : %f : pos %f, %f size %f X %f", scenery->getName().c_str(), dist_from_click, scenery_pos.x, scenery_pos.y, scenery_width, scenery_height);
        if( dist_from_click <= npc_radius_c && scenery->isBlocking() ) {
            // clicked on or near this scenery
            *ignore = scenery;
        }
        if( dist_from_click <= click_tol_scenery_c ) {
            // clicked on this scenery
            //LOG("    scenery pos: %f, %f : width %f height %f\n", scenery_pos.x, scenery_pos.y, scenery_width, scenery_height);
            //float player_dist = distFromBox2D(scenery_pos, scenery_width, scenery_height, player->getPos());
            float player_dist = scenery->distFromPoint(player->getPos(), is_click);
            //qDebug("    player_dist from %s: %f", scenery->getName().c_str(), player_dist);
            if( player_dist <= npc_radius_c + 0.5f ) {
                clicked_scenerys.push_back(scenery);
            }
        }
    }

    bool done = clickedOnScenerys(move, ignore, clicked_scenerys);
    //qDebug("clickedOnScenerys returns: %d", done);
    return done;

}

void PlayingGamestate::actionCommand(bool pickup_only) {
    qDebug("PlayingGamestate::actionCommand()");

    if( player != NULL && !player->isDead() && !player->isParalysed() ) {
        bool done = false;
        Vector2D forward_dest1 = player->getPos() + player->getDirection() * npc_radius_c * 1.1f;
        Vector2D forward_dest4 = player->getPos() + player->getDirection() * (npc_radius_c * 1.0f + 0.1f - E_TOL_LINEAR); // useful for doors, which have thickness 0.1
        Vector2D forward_dest2 = player->getPos() + player->getDirection() * npc_radius_c * 2.0f;
        Vector2D forward_dest3 = player->getPos() + player->getDirection() * npc_radius_c * 3.0f;

        // search for NPC
        if( !pickup_only )
        {
            float min_dist = 0.0f;
            Character *target_npc = NULL;
            for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
                Character *character = *iter;
                if( character == player )
                    continue;
                if( !character->isVisible() )
                    continue;
                float dist_from_click = (forward_dest2 - character->getPos()).magnitude();
                if( dist_from_click <= npc_radius_c ) {
                    if( target_npc == NULL || dist_from_click < min_dist ) {
                        // clicked on this character
                        done = true;
                        target_npc = character;
                        min_dist = dist_from_click;
                    }
                }
            }
            if( target_npc == NULL && player->getCurrentWeapon() != NULL && player->getCurrentWeapon()->isRangedOrThrown() ) {
                // for ranged weapons, pick closest visible enemy to player in the direction the player is facing
                for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
                    Character *character = *iter;
                    if( character == player )
                        continue;
                    if( !character->isVisible() )
                        continue;
                    if( !character->isHostile() )
                        continue;
                    Vector2D diff = character->getPos() - player->getPos();
                    float dist_from_player = diff.magnitude();
                    Vector2D player_dir = player->getDirection();
                    if( player_dir % diff >= -E_TOL_LINEAR ) {
                        if( target_npc == NULL || dist_from_player < min_dist ) {
                            done = true;
                            target_npc = character;
                            min_dist = dist_from_player;
                        }
                    }
                }
            }
            this->clickedOnNPC(target_npc); // call even if target_npc is NULL
        }

        if( !done && !pickup_only ) {
            bool move = false;
            void *ignore_scenery = NULL;
            done = handleClickForScenerys(&move, &ignore_scenery, forward_dest1, false);
            if( !done ) {
                done = handleClickForScenerys(&move, &ignore_scenery, forward_dest2, false);
            }
            if( !done /*&& player->getDirection().y < 0.0f*/ ) {
                // needed for scenery on walls, as well as improving for "isometric" objects
                done = handleClickForScenerys(&move, &ignore_scenery, forward_dest3, false);
            }
            if( !done ) {
                done = handleClickForScenerys(&move, &ignore_scenery, forward_dest4, false);
            }
            if( !done ) {
                // needed for when on top of non-blocking sceneries
                done = handleClickForScenerys(&move, &ignore_scenery, player->getPos(), false);
            }
        }

        if( !done ) {
            // We do items last, unlike with mouse click, so that the player can still do other things even if standing over items.
            // If the player wants to force picking up items, they can do that via a separate shortcut
            done = handleClickForItems( player->getPos() );
        }

    }
}

void PlayingGamestate::cycleTargetNPC() {
    qDebug("PlayingGamestate::cycleTargetNPC()");
    if( targetButton != NULL ) {
        targetButton->clearFocus(); // workaround for Android still showing selection
    }
    if( this->player != NULL ) {
        Character *current_target = this->player->getTargetNPC();
        this->player->setTargetNPC(NULL);
        bool was_last = false;
        Character *first_npc = NULL;
        for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
            Character *character = *iter;
            if( character == player )
                continue;
            if( !character->isHostile() )
                continue;
            if( !character->isVisible() )
                continue;
            if( first_npc == NULL ) {
                first_npc = character;
            }
            if( current_target == NULL ) {
                this->player->setTargetNPC(character);
                break;
            }
            else if( current_target == character ) {
                was_last = true;
            }
            else if( was_last ) {
                this->player->setTargetNPC(character);
                break;
            }
        }
        if( this->player->getTargetNPC() == NULL && first_npc != NULL ) {
            // reset to first NPC
            this->player->setTargetNPC(first_npc);
        }
        if( this->player->getTargetNPC() != NULL && this->player->getTargetNPC() != current_target ) {
            const Character *npc = this->player->getTargetNPC();
            this->requestPlayerMove(npc->getPos(), npc);
        }
    }

}

void PlayingGamestate::clickedMainView(float scene_x, float scene_y) {
    if( !is_created ) {
        // on Android with Qt 5 at least, this can be called while still loading/creating
        return;
    }
    if( game_g->isPaused() ) {
        game_g->setPaused(false, false);
    }

    if( player != NULL && !player->isDead() && !player->isParalysed() ) {
        //player->setPos(scene_x, scene_y);

        Vector2D dest(scene_x, scene_y);

        bool done = false;
        bool move = true;

        // do items first - so that player can always pick up items dropped near friendly (or otherwise non-moving) NPCs, as well as behind scenery!
        if( !done ) {
            done = handleClickForItems(dest);
        }

        void *ignore = NULL; // item to ignore for collision detection - so we can move towards an item that blocks
        // search for clicking on an NPC
        if( !done )
        {
            //qDebug("look for clicking on NPCs");
            float min_dist = 0.0f;
            Character *target_npc = NULL;
            for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
                Character *character = *iter;
                if( character == player )
                    continue;
                if( !character->isVisible() )
                    continue;
                /*
                const float click_tol_npc_c = 0.0f;
                float dist_from_click = (dest - character->getPos()).magnitude();
                if( dist_from_click <= npc_radius_c + click_tol_npc_c ) {
                    if( target_npc == NULL || dist_from_click < min_dist ) {*/
                /*if( target_npc == NULL || dist_from_click < min_dist ) {
                    AnimatedObject *object = static_cast<AnimatedObject *>(character->getListenerData());
                    if( object->contains(object->mapFromScene(QPointF(scene_x, scene_y)) ) ) {*/

                /* For collision detection and so on, we treat the characters as being a circle of radius npc_radius_c, centred on the character position.
                 * But for clicking on an NPC, we want to allow the user to be able to click on the visible graphical area (which may be taller, for example).
                 */
                AnimatedObject *object = static_cast<AnimatedObject *>(character->getListenerData());
                if( object->contains(object->mapFromScene(QPointF(scene_x, scene_y)) ) ) {
                    // clicked on this character
                    float this_dist = (character->getPos() - dest).magnitude();
                    //qDebug("  clicked on npc, dist: %f", this_dist);
                    if( !done || this_dist < min_dist )
                    {
                        done = true;
                        target_npc = character;
                        ignore = target_npc;
                        min_dist = this_dist;
                    }
                }
            }
            this->clickedOnNPC(target_npc); // call even if target_npc is NULL
        }

        if( !done ) {
            done = handleClickForScenerys(&move, &ignore, dest, true);
        }

        if( move && !player->isDead() ) {
            //qDebug("ignore: %d", ignore);
            this->requestPlayerMove(dest, ignore);
        }
    }
    //qDebug("done PlayingGamestate::clickedMainView()");
}

void PlayingGamestate::requestPlayerMove(Vector2D dest, const void *ignore) {
    // nudge position due to boundaries
    dest = this->c_location->nudgeToFreeSpace(player->getPos(), dest, npc_radius_c);
    if( dest != player->getPos() ) {
        player->setDestination(dest.x, dest.y, ignore);
        if( player->carryingTooMuch() ) {
            this->addTextEffect(tr("You are carrying too much weight to move!").toStdString(), player->getPos(), 2000);
        }
        else if( player->tooWeakForArmour() ) {
            this->addTextEffect(tr("The armour you are wearing is\ntoo heavy for you to move!").toStdString(), player->getPos(), 2000);
        }
    }
}

void PlayingGamestate::hitEnemy(Character *source, Character *target, bool is_ranged, bool weapon_no_effect_magical, bool weapon_no_effect_holy, int weapon_damage) {
    qDebug("PlayingGamestate::hitEnemy()");
    // source may be NULL, if attacker is no longer alive (for ranged attacks)
    if( weapon_no_effect_magical ) {
        if( source == this->getPlayer() ) {
            this->addTextEffect(tr("Weapon has no effect!").toStdString(), source->getPos(), 2000, 255, 0, 0);
        }
    }
    else if( weapon_no_effect_holy ) {
        if( source == this->getPlayer() ) {
            this->addTextEffect(tr("Holy weapon has no effect!").toStdString(), source->getPos(), 2000, 255, 0, 0);
        }
    }
    else if( weapon_damage > 0 ) {
        if( target->decreaseHealth(this, weapon_damage, true, true) ) {
            target->addPainTextEffect(this);
            if( target->isDead() && source == this->getPlayer() ) {
                source->addXP(this, target->getXPWorth());
            }
        }
        //qDebug("    damage %d remaining %d", weapon_damage, target->getHealth());
    }

    if( !target->isDead() && !is_ranged ) {
        source->handleSpecialHitEffects(this, target);
    }
    qDebug("PlayingGamestate::hitEnemy() exit");
}

void PlayingGamestate::updateVisibilityForFloorRegion(FloorRegion *floor_region) {
    //qDebug("updateVisibilityForFloorRegion");
    ASSERT_LOGGER( floor_region->isVisible() );
    if( floor_region->isVisible() ) {
        //qDebug("floor_region %d", floor_region);
        QGraphicsItem *gfx_item = static_cast<QGraphicsPolygonItem *>(floor_region->getUserGfxData());
        //qDebug("gfx_item %d", gfx_item);
        ASSERT_LOGGER(gfx_item != NULL);
        gfx_item->setVisible( true );
        //qDebug("do sceneries");
        for(set<Scenery *>::iterator iter = floor_region->scenerysBegin(); iter != floor_region->scenerysEnd(); ++iter) {
            Scenery *scenery = *iter;
            //qDebug("scenery %d", scenery);
            //qDebug("    %s at %f, %f", scenery->getName().c_str(), scenery->getX(), scenery->getY());
            //qDebug("    at %f, %f", scenery->getX(), scenery->getY());
            for(size_t i=0;i<scenery->getNUserGfxData();i++) {
                QGraphicsItem *gfx_item2 = static_cast<QGraphicsPixmapItem *>(scenery->getUserGfxData(i));
                //qDebug("gfx_item2 %d", gfx_item2);
                gfx_item2->setVisible( true );
            }
        }
        //qDebug("do items");
        for(set<Item *>::iterator iter = floor_region->itemsBegin(); iter != floor_region->itemsEnd(); ++iter) {
            Item *item = *iter;
            //qDebug("item %d", item);
            QGraphicsItem *gfx_item2 = static_cast<QGraphicsPixmapItem *>(item->getUserGfxData());
            //qDebug("gfx_item2 %d", gfx_item2);
            if( gfx_item2 != NULL ) {
                gfx_item2->setVisible( true );
            }
        }
    }
    //qDebug("updateVisibilityForFloorRegion done");
}

void PlayingGamestate::updateVisibility(Vector2D pos) {
    this->need_visibility_update = false;
    vector<FloorRegion *> update_regions = this->c_location->updateVisibility(pos);
    for(vector<FloorRegion *>::iterator iter = update_regions.begin(); iter != update_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        updateVisibilityForFloorRegion(floor_region);
    }
}

void PlayingGamestate::saveItemProfileBonusInt(QTextStream &stream, const Item *item, const string &key) const {
    int value = item->getRawProfileBonusIntProperty(key);
    if( value != 0 ) {
        //fprintf(file, " bonus_%s=\"%d\"", key.c_str(), value);
        stream << " bonus_" << key.c_str() << "=\"" << value << "\"";
    }
}

void PlayingGamestate::saveItemProfileBonusFloat(QTextStream &stream, const Item *item, const string &key) const {
    float value = item->getRawProfileBonusFloatProperty(key);
    if( value != 0 ) {
        //fprintf(file, " bonus_%s=\"%f\"", key.c_str(), value);
        stream << " bonus_" << key.c_str() << "=\"" << value << "\"";
    }
}

void PlayingGamestate::saveItem(QTextStream &stream, const Item *item) const {
    return saveItem(stream, item, NULL);
}

void PlayingGamestate::saveItem(QTextStream &stream, const Item *item, const Character *character) const {
    switch( item->getType() ) {
    case ITEMTYPE_GENERAL:
        //fprintf(file, "<item");
        stream << "<item";
        break;
    case ITEMTYPE_WEAPON:
        //fprintf(file, "<weapon");
        stream << "<weapon";
        break;
    case ITEMTYPE_SHIELD:
        //fprintf(file, "<shield");
        stream << "<shield";
        break;
    case ITEMTYPE_ARMOUR:
        //fprintf(file, "<armour");
        stream << "<armour";
        break;
    case ITEMTYPE_RING:
        //fprintf(file, "<ring");
        stream << "<ring";
        break;
    case ITEMTYPE_AMMO:
        //fprintf(file, "<ammo");
        stream << "<ammo";
        break;
    case ITEMTYPE_CURRENCY:
        //fprintf(file, "<currency");
        stream << "<currency";
        break;
    }
    //fprintf(file, " name=\"%s\"", item->getKey().c_str()); // n.b., we use the key, not the name, as the latter may be overloaded to give more descriptive names
    stream << " name=\"" << item->getKey().c_str() << "\""; // n.b., we use the key, not the name, as the latter may be overloaded to give more descriptive names
    //fprintf(file, " image_name=\"%s\"", item->getImageName().c_str());
    stream << " image_name=\"" << item->getImageName().c_str() << "\"";
    //fprintf(file, " x=\"%f\" y=\"%f\"", item->getX(), item->getY());
    stream << " x=\"" << item->getX() << "\" y=\"" << item->getY() << "\"";
    //fprintf(file, " icon_width=\"%f\"", item->getIconWidth());
    stream << " icon_width=\"" << item->getIconWidth() << "\"";
    //fprintf(file, " weight=\"%d\"", item->getWeight());
    stream << " weight=\"" << item->getWeight() << "\"";
    if( item->getUse().length() > 0 ) {
        //fprintf(file, " use=\"%s\"", item->getUse().c_str());
        stream << " use=\"" << item->getUse().c_str() << "\"";
        //fprintf(file, " use_verb=\"%s\"", item->getUseVerb().c_str());
        stream << " use_verb=\"" << item->getUseVerb().c_str() << "\"";
    }
    if( item->getArg1() != 0 ) {
        //fprintf(file, " arg1=\"%d\"", item->getArg1());
        stream << " arg1=\"" << item->getArg1() << "\"";
    }
    if( item->getArg2() != 0 ) {
        //fprintf(file, " arg2=\"%d\"", item->getArg2());
        stream << " arg2=\"" << item->getArg2() << "\"";
    }
    if( item->getArg1s().length() > 0 ) {
        //fprintf(file, " arg1_s=\"%s\"", item->getArg1s().c_str());
        stream << " arg1_s=\"" << item->getArg1s().c_str() << "\"";
    }
    //fprintf(file, " rating=\"%d\"", item->getRating()); // n.b., should always explicitly save, due to hack with default rating being 1, not 0
    stream << " rating=\"" << item->getRating() << "\""; // n.b., should always explicitly save, due to hack with default rating being 1, not 0
    if( item->isMagical() ) {
        //fprintf(file, " magical=\"true\"");
        stream << " magical=\"true\"";
    }
    if( item->getBaseTemplate().length() > 0 ) {
        //fprintf(file, " base_template=\"%s\"", item->getBaseTemplate().c_str());
        stream << " base_template=\"" << item->getBaseTemplate().c_str() << "\"";
    }
    if( item->getWorthBonus() > 0 ) {
        //fprintf(file, " worth_bonus=\"%d\"", item->getWorthBonus());
        stream << " worth_bonus=\"" << item->getWorthBonus() << "\"";
    }
    this->saveItemProfileBonusInt(stream, item, profile_key_FP_c);
    this->saveItemProfileBonusInt(stream, item, profile_key_BS_c);
    this->saveItemProfileBonusInt(stream, item, profile_key_S_c);
    this->saveItemProfileBonusInt(stream, item, profile_key_A_c);
    this->saveItemProfileBonusInt(stream, item, profile_key_M_c);
    this->saveItemProfileBonusInt(stream, item, profile_key_D_c);
    this->saveItemProfileBonusInt(stream, item, profile_key_B_c);
    this->saveItemProfileBonusFloat(stream, item, profile_key_Sp_c);
    // now do subclasses
    switch( item->getType() ) {
    case ITEMTYPE_GENERAL:
        break;
    case ITEMTYPE_WEAPON:
    {
        const Weapon *weapon = static_cast<const Weapon *>(item);
        //fprintf(file, " animation_name=\"%s\"", weapon->getAnimationName().c_str());
        stream << " animation_name=\"" << weapon->getAnimationName().c_str() << "\"";
        if( weapon->isTwoHanded() ) {
            //fprintf(file, " two_handed=\"true\"");
            stream << " two_handed=\"true\"";
        }
        if( weapon->getWeaponType() == Weapon::WEAPONTYPE_RANGED ) {
            //fprintf(file, " ranged=\"true\"");
            stream << " ranged=\"true\"";
        }
        else if( weapon->getWeaponType() == Weapon::WEAPONTYPE_THROWN ) {
            //fprintf(file, " thrown=\"true\"");
            stream << " thrown=\"true\"";
        }
        int damageX = 0, damageY = 0, damageZ = 0;
        weapon->getDamage(&damageX, &damageY, &damageZ);
        //fprintf(file, " damageX=\"%d\"", damageX);
        //fprintf(file, " damageY=\"%d\"", damageY);
        //fprintf(file, " damageZ=\"%d\"", damageZ);
        stream << " damageX=\"" << damageX << "\"";
        stream << " damageY=\"" << damageY << "\"";
        stream << " damageZ=\"" << damageZ << "\"";
        if( weapon->getMinStrength() > 0 ) {
            //fprintf(file, " min_strength=\"%d\"", weapon->getMinStrength());
            stream << " min_strength=\"" << weapon->getMinStrength() << "\"";
        }
        if( weapon->isUnholyOnly() ) {
            stream << " unholy_only=\"true\"";
        }
        if( weapon->getUnholyBonus() != 0 ) {
            stream << " unholy_bonus=\"" << weapon->getUnholyBonus() << "\"";
        }
        if( weapon->getWeaponClass().length() > 0 ) {
            stream << " weapon_class=\"" << weapon->getWeaponClass().c_str() << "\"";
        }
        if( weapon->getRequiresAmmo() ) {
            stream << " ammo=\"" << weapon->getAmmoKey().c_str() << "\"";
        }
        if( character != NULL && weapon == character->getCurrentWeapon() ) {
            stream << " current_weapon=\"true\"";
        }
        break;
    }
    case ITEMTYPE_SHIELD:
    {
        const Shield *shield = static_cast<const Shield *>(item);
        //fprintf(file, " animation_name=\"%s\"", shield->getAnimationName().c_str());
        stream << " animation_name=\"" << shield->getAnimationName().c_str() << "\"";
        if( character != NULL && shield == character->getCurrentShield() ) {
            //fprintf(file, " current_shield=\"true\"");
            stream << " current_shield=\"true\"";
        }
        break;
    }
    case ITEMTYPE_ARMOUR:
    {
        const Armour *armour = static_cast<const Armour *>(item);
        if( armour->getMinStrength() > 0 ) {
            //fprintf(file, " min_strength=\"%d\"", armour->getMinStrength());
            stream << " min_strength=\"" << armour->getMinStrength() << "\"";
        }
        if( character != NULL && armour == character->getCurrentArmour() ) {
            //fprintf(file, " current_armour=\"true\"");
            stream << " current_armour=\"true\"";
        }
        break;
    }
    case ITEMTYPE_RING:
    {
        const Ring *ring = static_cast<const Ring *>(item);
        if( character != NULL && ring == character->getCurrentRing() ) {
            //fprintf(file, " current_ring=\"true\"");
            stream << " current_ring=\"true\"";
        }
        break;
    }
    case ITEMTYPE_AMMO:
    {
        const Ammo *ammo = static_cast<const Ammo *>(item);
        stream << " ammo_type=\"" << ammo->getAmmoType().c_str() << "\"";
        //fprintf(file, " projectile_image_name=\"%s\"", ammo->getProjectileImageName().c_str());
        stream << " projectile_image_name=\"" << ammo->getProjectileImageName().c_str() << "\"";
        //fprintf(file, " amount=\"%d\"", ammo->getAmount());
        stream << " amount=\"" << ammo->getAmount() << "\"";
        if( character != NULL && ammo == character->getCurrentAmmo() ) {
            stream << " current_ammo=\"true\"";
        }
        break;
    }
    case ITEMTYPE_CURRENCY:
    {
        const Currency *currency = static_cast<const Currency *>(item);
        //fprintf(file, " value=\"%d\"", currency->getValue());
        stream << " value=\"" << currency->getValue() << "\"";
        break;

    }
    }
    //fprintf(file, ">");
    stream << ">";
    //fprintf(file, "%s", item->getDescription().c_str());
    stream << item->getDescription().c_str();
    switch( item->getType() ) {
    case ITEMTYPE_GENERAL:
        //fprintf(file, "</item>");
        stream << "</item>";
        break;
    case ITEMTYPE_WEAPON:
        //fprintf(file, "</weapon>");
        stream << "</weapon>";
        break;
    case ITEMTYPE_SHIELD:
        //fprintf(file, "</shield>");
        stream << "</shield>";
        break;
    case ITEMTYPE_ARMOUR:
        //fprintf(file, "</armour>");
        stream << "</armour>";
        break;
    case ITEMTYPE_RING:
        //fprintf(file, "</ring>");
        stream << "</ring>";
        break;
    case ITEMTYPE_AMMO:
        //fprintf(file, "</ammo>");
        stream << "</ammo>";
        break;
    case ITEMTYPE_CURRENCY:
        //fprintf(file, "</currency>");
        stream << "</currency>";
        break;
    }
    //fprintf(file, "\n");
    stream << "\n";
}

void PlayingGamestate::saveTrap(QTextStream &stream, const Trap *trap) const {
    //fprintf(file, "<trap");
    //fprintf(file, " type=\"%s\"", trap->getType().c_str());
    //fprintf(file, " x=\"%f\" y=\"%f\"", trap->getX(), trap->getY());
    //fprintf(file, " w=\"%f\" h=\"%f\"", trap->getWidth(), trap->getHeight());
    //fprintf(file, " difficulty=\"%d\"", trap->getDifficulty());
    //fprintf(file, " rating=\"%d\"", trap->getRating());
    //fprintf(file, " />\n");
    stream << "<trap type=\"" << trap->getType().c_str() << "\" x=\"" << trap->getX() << "\" y=\"" << trap->getY() << "\"";
    stream << " w=\"" << trap->getWidth() << "\" h=\"" << trap->getHeight() << "\" difficulty=\"" << trap->getDifficulty() << "\"";
    stream << " rating=\"" << trap->getRating() << "\"";
    stream << "/>\n";
}

bool PlayingGamestate::saveGame(const QString &filename, bool already_fullpath) {
    LOG("PlayingGamestate::saveGame(%s)\n", filename.toUtf8().data());
    QString full_path;
    if( already_fullpath )
        full_path = filename;
    else
        full_path = game_g->getApplicationFilename(savegame_folder + filename);
    LOG("full path: %s\n", full_path.toUtf8().data());

    /*FILE *file = fopen(full_path.toUtf8().data(), "wt");
    if( file == NULL ) {
        LOG("failed to create file\n");
        return false;
    }*/
    QFile file(full_path);
    if( !file.open(QIODevice::WriteOnly | QIODevice::Text) ) {
        LOG("failed to create file\n");
        return false;
    }
    QTextStream stream(&file);

    game_g->getMainWindow()->setCursor(Qt::WaitCursor);

    const int savegame_version = 2;

    LOG("c_quest_indx: %d\n", c_quest_indx);

    //fprintf(file, "<?xml version=\"1.0\" ?>\n");
    //fprintf(file, "<savegame major=\"%d\" minor=\"%d\" savegame_version=\"%d\">\n", versionMajor, versionMinor, savegame_version);
    //fprintf(file, "\n");
    //fprintf(file, "<game difficulty=\"%s\" permadeath=\"%s\"/>", Game::getDifficultyString(difficulty).c_str(), permadeath ? "true" : "false");
    //fprintf(file, "<current_quest name=\"%s\"/>\n", this->quest_list.at(this->c_quest_indx).getFilename().c_str());
    //fprintf(file, "\n");
    stream << "<?xml version=\"1.0\" ?>\n";
    stream << "<savegame major=\"" << versionMajor << "\" minor=\"" << versionMinor << "\" savegame_version=\"" << savegame_version << "\">\n";
    stream << "\n";
    stream << "<game";
    stream << " difficulty=\"" << Game::getDifficultyString(difficulty).c_str() << "\"";
    stream << " permadeath=\"" << (permadeath ? "true" : "false") << "\"";
    stream << " gametype=\"";
    if( gameType == GAMETYPE_RANDOM )
        stream << "gametype_random";
    else
        stream << "gametype_campaign";
    stream << "\"";
    stream << "/>\n";
    if( gameType == GAMETYPE_CAMPAIGN ) {
        stream << "<current_quest name=\"" << this->quest_list.at(this->c_quest_indx).getFilename().c_str() << "\"/>\n";
    }
    stream << "\n";

    qDebug("save flags");
    for(set<string>::const_iterator iter_flags = quest->flagsBegin(); iter_flags != quest->flagsEnd(); ++iter_flags) {
        const string flag = *iter_flags;
        stream << "<flag name=\"" << flag.c_str() << "\"/>\n";
    }

    qDebug("save locations");
    for(vector<Location *>::const_iterator iter_loc = quest->locationsBegin(); iter_loc != quest->locationsEnd(); ++iter_loc) {
        const Location *location = *iter_loc;

        string type_str;
        switch( location->getType() ) {
        case Location::TYPE_INDOORS:
            type_str = "indoors";
            break;
        case Location::TYPE_OUTDOORS:
            type_str = "outdoors";
            break;
        default:
            ASSERT_LOGGER(false);
            break;
        }
        string geo_type_str;
        switch( location->getGeoType() ) {
        case Location::GEOTYPE_DUNGEON:
            geo_type_str = "dungeon";
            break;
        case Location::GEOTYPE_OUTDOORS:
            geo_type_str = "outdoors";
            break;
        default:
            ASSERT_LOGGER(false);
            break;
        }
        //fprintf(file, "<location name=\"%s\" type=\"%s\" lighting_min=\"%d\">\n\n", location->getName().c_str(), type_str.c_str(), location->getLightingMin());
        stream << "<location name=\"" << location->getName().c_str() << "\" type=\"" << type_str.c_str() << "\" geo_type=\"" << geo_type_str.c_str() << "\" lighting_min=\"" << static_cast<int>(location->getLightingMin()) << "\"";
        if( location->isDisplayName() ) {
            stream << " display_name=\"true\"";
        }
        if( location->getRestSummonLocation().length() > 0 ) {
            stream << " rest_summon_location=\"" << location->getRestSummonLocation().c_str() << "\"";
        }
        stream << ">\n\n";

        qDebug("save location images");
        //fprintf(file, "<background image_name=\"%s\"/>\n", location->getBackgroundImageName().c_str());
        stream << "<background image_name=\"" << location->getBackgroundImageName().c_str() << "\"/>\n";
        //fprintf(file, "<floor image_name=\"%s\"/>\n", location->getFloorImageName().c_str());
        stream << "<floor image_name=\"" << location->getFloorImageName().c_str() << "\"/>\n";
        if( location->getWallImageName().length() > 0 ) {
            //fprintf(file, "<wall image_name=\"%s\" image_x_scale=\"%f\"/>\n", location->getWallImageName().c_str(), location->getWallXScale());
            stream << "<wall image_name=\"" << location->getWallImageName().c_str() << "\" image_x_scale=\"" << location->getWallXScale() << "\"/>\n";
        }
        if( location->getDropWallImageName().length() > 0 ) {
            //fprintf(file, "<dropwall image_name=\"%s\"/>\n", location->getDropWallImageName().c_str());
            stream << "<dropwall image_name=\"" << location->getDropWallImageName().c_str() << "\"/>\n";
        }
        if( location->getWanderingMonsterTemplate().length() > 0 ) {
            //fprintf(file, "<wandering_monster template=\"%s\" time=\"%d\" rest_chance=\"%d\"/>\n", location->getWanderingMonsterTemplate().c_str(), location->getWanderingMonsterTimeMS(), location->getWanderingMonsterRestChance());
            stream << "<wandering_monster template=\"" << location->getWanderingMonsterTemplate().c_str() << "\" time=\"" << location->getWanderingMonsterTimeMS() << "\" rest_chance=\"" << location->getBaseWanderingMonsterRestChance() << "\"/>\n";
        }
        //fprintf(file, "\n");
        stream << "\n";

        for(size_t i=0;i<location->getNFloorRegions();i++) {
            const FloorRegion *floor_region = location->getFloorRegion(i);
            stream << "<floorregion shape=\"polygon\"";
            if( floor_region->isVisible() ) {
                stream << " visible=\"true\"";
            }
            if( floor_region->getFloorImageName().length() > 0 ) {
                stream << " image_name=\"" << floor_region->getFloorImageName().c_str() << "\"";
            }
            stream << ">\n";
            for(size_t j=0;j<floor_region->getNPoints();j++) {
                Vector2D point = floor_region->getPoint(j);
                stream << "    <floorregion_point x=\"" << point.x << "\" y=\"" << point.y << "\"/>\n";
            }
            stream << "</floorregion>\n";
        }
        //fprintf(file, "\n");
        stream << "\n";

        for(size_t i=0;i<c_location->getNTilemaps();i++) {
            const Tilemap *tilemap = c_location->getTilemap(i);
            //fprintf(file, "<tilemap imagemap=\"%s\" tile_width=\"%d\" tile_height=\"%d\" x=\"%f\" y=\"%f\">\n", tilemap->getImagemap().c_str(), tilemap->getTileWidth(), tilemap->getTileHeight(), tilemap->getX(), tilemap->getY());
            stream << "<tilemap imagemap=\"" << tilemap->getImagemap().c_str() << "\" tile_width=\"" << tilemap->getTileWidth() << "\" tile_height=\"" << tilemap->getTileHeight() << "\" x=\"" << tilemap->getX() << "\" y=\"" << tilemap->getY() << "\">\n";
            for(int y=0;y<tilemap->getHeighti();y++) {
                for(int x=0;x<tilemap->getWidthi();x++) {
                    char ch = tilemap->getTileAt(x, y);
                    //fprintf(file, "%c", ch);
                    stream << ch;
                }
                //fprintf(file, "\n");
                stream << "\n";
            }
            //fprintf(file, "</tilemap>\n");
            stream << "</tilemap>\n";
        }
        //fprintf(file, "\n");
        stream << "\n";

        qDebug("save player and npcs");
        for(set<Character *>::const_iterator iter = location->charactersBegin(); iter != location->charactersEnd(); ++iter) {
            const Character *character = *iter;
            if( player == character ) {
                //fprintf(file, "<player");
                stream << "<player";
            }
            else {
                //fprintf(file, "<npc");
                //fprintf(file, " is_hostile=\"%s\"", character->isHostile() ? "true": "false");
                //fprintf(file, " animation_name=\"%s\"", character->getAnimationName().c_str());
                stream << "<npc is_hostile=\"" << (character->isHostile() ? "true": "false") << "\" animation_name=\"" << character->getAnimationName().c_str() << "\"";
            }
            if( character->isStaticImage() ) {
                //fprintf(file, " static_image=\"%s\"", character->isStaticImage() ? "true": "false");
                stream << " static_image=\"" << (character->isStaticImage() ? "true": "false") << "\"";
            }
            if( character->getType().length() > 0 ) {
                stream << " type=\"" << character->getType().c_str() << "\"";
            }
            if( character->getPortrait().length() > 0 ) {
                //fprintf(file, " portrait=\"%s\"", character->getPortrait().c_str());
                stream << " portrait=\"" << character->getPortrait().c_str() << "\"";
            }
            if( character->getAnimationFolder().length() > 0 ) {
                stream << " animation_folder=\"" << character->getAnimationFolder().c_str() << "\"";
            }
            if( character->isBounce() ) {
                stream << " bounce=\"true\"";
            }
            stream << " image_size=\"" << character->getImageSize() << "\"";
            if( character->getWeaponResistClass().length() > 0 ) {
                stream << " weapon_resist_class=\"" << character->getWeaponResistClass().c_str() << "\"";
                stream << " weapon_resist_percentage=\"" << character->getWeaponResistPercentage() << "\"";
            }
            if( character->getRegeneration() != 0 ) {
                stream << " regeneration=\"" << character->getRegeneration() << "\"";
            }
            if( character->getDeathExplodes() ) {
                stream << " death_explodes=\"true\"";
                stream << " death_explodes_damage=\"" << character->getDeathExplodesDamage() << "\"";
            }
            //fprintf(file, " name=\"%s\"", character->getName().c_str());
            stream << " name=\"" << character->getName().c_str() << "\"";
            if( character->isDead() ) {
                //fprintf(file, " is_dead=\"true\"");
                stream << " is_dead=\"true\"";
            }
            //fprintf(file, " x=\"%f\" y=\"%f\"", character->getX(), character->getY());
            stream << " x=\"" << character->getX() << "\" y=\"" << character->getY() << "\"";
            if( character->hasDefaultPosition() ) {
                //fprintf(file, " default_x=\"%f\" default_y=\"%f\"", character->getDefaultX(), character->getDefaultY());
                stream << " default_x=\"" << character->getDefaultX() << "\" default_y=\"" << character->getDefaultY() << "\"";
            }
            //fprintf(file, " health=\"%d\"", character->getHealth());
            stream << " health=\"" << character->getHealth() << "\"";
            //fprintf(file, " max_health=\"%d\"", character->getMaxHealth());
            stream << " max_health=\"" << character->getMaxHealth() << "\"";
            for(map<string, int>::const_iterator iter = character->getBaseProfile()->intPropertiesBegin(); iter != character->getBaseProfile()->intPropertiesEnd(); ++iter) {
                string key = iter->first;
                int value = iter->second;
                //fprintf(file, " %s=\"%d\"", key.c_str(), value);
                stream << " " << key.c_str() << "=\"" << value << "\"";
            }
            for(map<string, float>::const_iterator iter = character->getBaseProfile()->floatPropertiesBegin(); iter != character->getBaseProfile()->floatPropertiesEnd(); ++iter) {
                string key = iter->first;
                float value = iter->second;
                //fprintf(file, " %s=\"%f\"", key.c_str(), value);
                stream << " " << key.c_str() << "=\"" << value << "\"";
            }
            if( character == this->getPlayer() ) {
                // only care about initial stats for player for now
                stream << " initial_level=\"" << character->getInitialLevel() << "\"";
                for(map<string, int>::const_iterator iter = character->getInitialBaseProfile()->intPropertiesBegin(); iter != character->getInitialBaseProfile()->intPropertiesEnd(); ++iter) {
                    string key = iter->first;
                    int value = iter->second;
                    stream << " initial_" << key.c_str() << "=\"" << value << "\"";
                }
                for(map<string, float>::const_iterator iter = character->getInitialBaseProfile()->floatPropertiesBegin(); iter != character->getInitialBaseProfile()->floatPropertiesEnd(); ++iter) {
                    string key = iter->first;
                    float value = iter->second;
                    stream << " initial_" << key.c_str() << "=\"" << value << "\"";
                }
            }
            //int natural_damageX = 0, natural_damageY = 0, natural_damageZ = 0;
            //character->getNaturalDamage(&natural_damageX, &natural_damageY, &natural_damageZ);
            int natural_damageX = character->getNaturalDamageX();
            int natural_damageY = character->getNaturalDamageY();
            int natural_damageZ = character->getNaturalDamageZ();

            //fprintf(file, " natural_damageX=\"%d\"", natural_damageX);
            //fprintf(file, " natural_damageY=\"%d\"", natural_damageY);
            //fprintf(file, " natural_damageZ=\"%d\"", natural_damageZ);
            stream << " natural_damageX=\"" << natural_damageX << "\"";
            stream << " natural_damageY=\"" << natural_damageY << "\"";
            stream << " natural_damageZ=\"" << natural_damageZ << "\"";

            if( character->canFly() ) {
                //fprintf(file, " can_fly=\"true\"");
                stream << " can_fly=\"true\"";
            }
            if( character->isParalysed() ) {
                //fprintf(file, " is_paralysed=\"true\"");
                stream << " is_paralysed=\"true\"";
                //fprintf(file, " paralysed_time=\"%d\"", character->getParalysedUntil() - game_g->getScreen()->getGameTimeTotalMS());
                stream << " paralysed_time=\"" << (character->getParalysedUntil() - game_g->getGameTimeTotalMS()) << "\"";
            }
            if( character->isDiseased() ) {
                //fprintf(file, " is_diseased=\"true\"");
                stream << " is_diseased=\"true\"";
            }
            //fprintf(file, " level=\"%d\"", character->getLevel());
            stream << " level=\"" << character->getLevel() << "\"";
            //fprintf(file, " xp=\"%d\"", character->getXP());
            stream << " xp=\"" << character->getXP() << "\"";
            //fprintf(file, " xp_worth=\"%d\"", character->getXPWorth());
            stream << " xp_worth=\"" << character->getXPWorth() << "\"";
            if( character->getCausesTerror() ) {
                //fprintf(file, " causes_terror=\"true\"");
                stream << " causes_terror=\"true\"";
                //fprintf(file, " terror_effect=\"%d\"", character->getTerrorEffect());
                stream << " terror_effect=\"" << character->getTerrorEffect() << "\"";
            }
            if( character->hasDoneTerror() ) {
                //fprintf(file, " done_terror=\"%s\"", character->hasDoneTerror() ? "true" : "false");
                stream << " done_terror=\"true\"";
            }
            if( character->isFleeing() ) {
                stream << " is_fleeing=\"true\"";
            }
            if( character->getCausesDisease() > 0 ) {
                //fprintf(file, " causes_disease=\"%d\"", character->getCausesDisease());
                stream << " causes_disease=\"" << character->getCausesDisease() << "\"";
            }
            if( character->getCausesParalysis() > 0 ) {
                //fprintf(file, " causes_paralysis=\"%d\"", character->getCausesParalysis());
                stream << " causes_paralysis=\"" << character->getCausesParalysis() << "\"";
            }
            if( character->requiresMagical() ) {
                //fprintf(file, " requires_magical=\"true\"");
                stream << " requires_magical=\"true\"";
            }
            if( character->isUnholy() ) {
                //fprintf(file, " unholy=\"true\"");
                stream << " unholy=\"true\"";
            }
            //fprintf(file, " gold=\"%d\"", character->getGold());
            stream << " gold=\"" << character->getGold() << "\"";
            if( character->canTalk() ) {
                //fprintf(file, " can_talk=\"true\"");
                stream << " can_talk=\"true\"";
            }
            if( character->hasTalked() ) {
                //fprintf(file, " has_talked=\"true\"");
                stream << " has_talked=\"true\"";
            }
            if( character->getInteractionType().length() > 0 ) {
                //fprintf(file, " interaction_type=\"%s\"", character->getInteractionType().c_str());
                stream << " interaction_type=\"" << character->getInteractionType().c_str() << "\"";
            }
            if( character->getInteractionData().length() > 0 ) {
                //fprintf(file, " interaction_data=\"%s\"", character->getInteractionData().c_str());
                stream << " interaction_data=\"" << character->getInteractionData().c_str() << "\"";
            }
            if( character->getInteractionJournal().length() > 0 ) {
                stream << " interaction_journal=\"" << character->getInteractionJournal().c_str() << "\"";
            }
            if( character->getInteractionSetFlag().length() > 0 ) {
                stream << " interaction_set_flag=\"" << character->getInteractionSetFlag().c_str() << "\"";
            }
            if( character->getInteractionXP() != 0 ) {
                //fprintf(file, " interaction_xp=\"%d\"", character->getInteractionXP());
                stream << " interaction_xp=\"" << character->getInteractionXP() << "\"";
            }
            if( character->getInteractionRewardItem().length() > 0 ) {
                //fprintf(file, " interaction_reward_item=\"%s\"", character->getInteractionRewardItem().c_str());
                stream << " interaction_reward_item=\"" << character->getInteractionRewardItem().c_str() << "\"";
            }
            if( character->getInteractionRewardGold() > 0 ) {
                stream << " interaction_reward_gold=\"" << character->getInteractionRewardGold() << "\"";
            }
            if( character->isInteractionCompleted() ) {
                //fprintf(file, " interaction_completed=\"true\"");
                stream << " interaction_completed=\"true\"";
            }
            if( character->getShop().length() > 0 ) {
                //fprintf(file, " shop=\"%s\"", character->getShop().c_str());
                stream << " shop=\"" << character->getShop().c_str() << "\"";
            }
            if( character->getObjectiveId().length() > 0 ) {
                //fprintf(file, " objective_id=\"%s\"", character->getObjectiveId().c_str());

                stream << " objective_id=\"" << character->getObjectiveId().c_str() << "\"";
            }
            //fprintf(file, ">\n");
            stream << ">\n";

            for(set<string>::const_iterator iter2 = character->skillsBegin(); iter2 != character->skillsEnd(); ++iter2) {
                const string skill = *iter2;
                stream << "<skill name=\"" << skill.c_str() << "\"/>";
            }

            if( character->getTalkOpeningInitial().length() > 0 ) {
                //fprintf(file, "<opening_initial>%s</opening_initial>", character->getTalkOpeningInitial().c_str());
                stream << "<opening_initial>" << character->getTalkOpeningInitial().c_str() << "</opening_initial>";
            }
            if( character->getTalkOpeningLater().length() > 0 ) {
                //fprintf(file, "<opening_later>%s</opening_later>", character->getTalkOpeningLater().c_str());
                stream << "<opening_later>" << character->getTalkOpeningLater().c_str() << "</opening_later>";
            }
            if( character->getTalkOpeningInteractionComplete().length() > 0 ) {
                //fprintf(file, "<opening_interaction_complete>%s</opening_interaction_complete>", character->getTalkOpeningInteractionComplete().c_str());
                stream << "<opening_interaction_complete>" << character->getTalkOpeningInteractionComplete().c_str() << "</opening_interaction_complete>";
            }
            for(vector<TalkItem>::const_iterator iter2 = character->talkItemsBegin(); iter2 != character->talkItemsEnd(); ++iter2) {
                const TalkItem *talk_item = &*iter2;
                //fprintf(file, "<talk");
                //fprintf(file, " question=\"%s\"", talk_item->question.c_str());
                //fprintf(file, " action=\"%s\"", talk_item->action.c_str());
                //fprintf(file, " while_not_done=\"%s\"", talk_item->while_not_done ? "true": "false");
                //fprintf(file, " objective=\"%s\"", talk_item->objective ? "true": "false");
                //fprintf(file, ">\n");
                //fprintf(file, "%s\n", talk_item->answer.c_str());
                //fprintf(file, "</talk>\n");
                stream << "<talk question=\"" << talk_item->question.c_str() << "\"";
                if( talk_item->action.length() > 0 ) {
                    stream << " action=\"" << talk_item->action.c_str() << "\"";
                }
                if( talk_item->journal.length() > 0 ) {
                    stream << " journal=\"" << talk_item->journal.c_str() << "\"";
                }
                stream << " while_not_done=\"" << (talk_item->while_not_done ? "true": "false") << "\"";
                stream << " objective=\"" << (talk_item->objective ? "true": "false") << "\"";
                stream << ">\n";
                stream << talk_item->answer.c_str() << "\n";
                stream << "</talk>\n";
            }
            for(map<string, int>::const_iterator iter2 = character->spellsBegin(); iter2 != character->spellsEnd(); ++iter2) {
                string spell_name = iter2->first;
                int spell_count = iter2->second;
                //fprintf(file, "<spell name=\"%s\" count=\"%d\"/>\n", spell_name.c_str(), spell_count);
                stream << "<spell name=\"" << spell_name.c_str() << "\" count=\"" << spell_count << "\"/>\n";
            }
            for(set<Item *>::const_iterator iter2 = character->itemsBegin(); iter2 != character->itemsEnd(); ++iter2) {
                const Item *item = *iter2;
                this->saveItem(stream, item, character);
            }
            if( player == character ) {
                //fprintf(file, "</player>\n");
                stream << "</player>\n";
            }
            else {
                //fprintf(file, "</npc>\n");
                stream << "</npc>\n";
            }
        }
        //fprintf(file, "\n");
        stream << "\n";

        if( location == c_location ) {
            qDebug("save player additional info");
            //fprintf(file, "<player_start x=\"%f\" y=\"%f\"/>\n", player->getX(), player->getY());
            //fprintf(file, "\n");
            stream << "<player_start x=\"" << player->getX() << "\" y=\"" << player->getY() << "\"/>\n\n";
        }

        qDebug("save scenery");
        for(set<Scenery *>::const_iterator iter = location->scenerysBegin(); iter != location->scenerysEnd(); ++iter) {
            const Scenery *scenery = *iter;
            //fprintf(file, "<scenery");
            stream << "<scenery";
            //fprintf(file, " name=\"%s\"", scenery->getName().c_str());
            stream << " name=\"" << scenery->getName().c_str() << "\"";
            //fprintf(file, " image_name=\"%s\"", scenery->getImageName().c_str());
            stream << " image_name=\"" << scenery->getImageName().c_str() << "\"";
            if( scenery->getBigImageName().length() > 0 ) {
                //fprintf(file, " big_image_name=\"%s\"", scenery->getBigImageName().c_str());
                stream << " big_image_name=\"" << scenery->getBigImageName().c_str() << "\"";
            }
            stream << " x=\"" << scenery->getX() << "\" y=\"" << scenery->getY() << "\"";
            stream << " w=\"" << scenery->getWidth() << "\" h=\"" << scenery->getHeight() << "\" visual_h=\"" << scenery->getVisualHeight() << "\"";
            if( scenery->isBoundaryIso() ) {
                stream << " boundary_iso=\"true\" boundary_iso_ratio=\"" << scenery->getBoundaryIsoRatio() << "\"";
            }
            stream << " opacity=\"" << scenery->getOpacity() << "\"";
            switch( scenery->getDrawType() ) {
            case Scenery::DRAWTYPE_NORMAL:
                break;
            case Scenery::DRAWTYPE_FLOATING:
                //fprintf(file, " draw_type=\"floating\"");
                stream << " draw_type=\"floating\"";
                break;
            case Scenery::DRAWTYPE_BACKGROUND:
                //fprintf(file, " draw_type=\"background\"");
                stream << " draw_type=\"background\"";
                break;
            default:
                ASSERT_LOGGER(false);
                break;
            }
            //fprintf(file, " action_last_time=\"%d\"", scenery->getActionLastTime());
            //fprintf(file, " action_delay=\"%d\"", scenery->getActionDelay());
            //fprintf(file, " action_type=\"%s\"", scenery->getActionType().c_str());
            //fprintf(file, " action_value=\"%d\"", scenery->getActionValue());
            //fprintf(file, " interact_type=\"%s\"", scenery->getInteractType().c_str());
            //fprintf(file, " interact_state=\"%d\"", scenery->getInteractState());
            // not saved:
            /*if( scenery->getActionLastTime() != 0 )
                stream << " action_last_time=\"" << scenery->getActionLastTime() << "\"";*/
            if( scenery->getActionDelay() != 0 )
                stream << " action_delay=\"" << scenery->getActionDelay() << "\"";
            if( scenery->getActionType().length() > 0 )
                stream << " action_type=\"" << scenery->getActionType().c_str() << "\"";
            if( scenery->getActionValue() != 0 )
                stream << " action_value=\"" << scenery->getActionValue() << "\"";
            if( scenery->getInteractType().length() > 0 )
                stream << " interact_type=\"" << scenery->getInteractType().c_str() << "\"";
            if( scenery->getInteractState() != 0 )
                stream << " interact_state=\"" << scenery->getInteractState() << "\"";
            if( scenery->getRequiresFlag().length() > 0 ) {
                stream << " requires_flag=\"" << scenery->getRequiresFlag().c_str() << "\"";
            }
            if( scenery->isBlocking() ) {
                //fprintf(file, " blocking=\"true\"");
                stream << " blocking=\"true\"";
            }
            if( scenery->blocksVisibility() ) {
                //fprintf(file, " block_visibility=\"true\"");
                stream << " block_visibility=\"true\"";
            }
            if( scenery->hasSmoke() ) {
                //fprintf(file, " has_smoke=\"true\"");
                //fprintf(file, " smoke_x=\"%f\"", scenery->getSmokePos().x);
                //fprintf(file, " smoke_y=\"%f\"", scenery->getSmokePos().y);
                stream << " has_smoke=\"true\" smoke_x=\"" << scenery->getSmokePos().x << "\" smoke_y=\"" << scenery->getSmokePos().y << "\"";
            }
            if( scenery->isOpened() ) {
                //fprintf(file, " is_opened=\"true\"");
                stream << " is_opened=\"true\"";
            }
            if( scenery->isExit() ) {
                //fprintf(file, " exit=\"true\"");
                stream << " exit=\"true\"";
            }
            if( scenery->isDoor() ) {
                //fprintf(file, " door=\"true\"");
                stream << " door=\"true\"";
            }
            if( scenery->getExitLocation().length() > 0 ) {
                stream << " exit_location=\"" << scenery->getExitLocation().c_str() << "\" exit_location_x=\"" << scenery->getExitLocationPos().x << "\" exit_location_y=\"" << scenery->getExitLocationPos().y << "\"";
                if( scenery->getExitTravelTime() > 0 ) {
                    stream << " exit_travel_time=\"" << scenery->getExitTravelTime() << "\"";
                }
            }
            if( scenery->isLocked() ) {
                //fprintf(file, " locked=\"true\"");
                stream << " locked=\"true\"";
            }
            if( scenery->isLockedSilent() ) {
                //fprintf(file, " locked_silent=\"true\"");
                stream << " locked_silent=\"true\"";
            }
            if( scenery->getLockedText().length() > 0 ) {
                //fprintf(file, " locked_text=\"%s\"", scenery->getLockedText().c_str());
                stream << " locked_text=\"" << scenery->getLockedText().c_str() << "\"";
            }
            if( scenery->isLockedUsedUp() ) {
                //fprintf(file, " locked_used_up=\"true\"");
                stream << " locked_used_up=\"true\"";
            }
            if( scenery->isKeyAlwaysNeeded() ) {
                //fprintf(file, " key_always_needed=\"true\"");
                stream << " key_always_needed=\"true\"";
            }
            if( scenery->getUnlockItemName().length() > 0 ) {
                //fprintf(file, " unlocked_by_template=\"%s\"", scenery->getUnlockItemName().c_str());
                stream << " unlocked_by_template=\"" << scenery->getUnlockItemName().c_str() << "\"";
            }
            if( scenery->getUnlockText().length() > 0 ) {
                //fprintf(file, " unlock_text=\"%s\"", scenery->getUnlockText().c_str());
                stream << " unlock_text=\"" << scenery->getUnlockText().c_str() << "\"";
            }
            if( scenery->getUnlockXP() > 0 ) {
                //fprintf(file, " unlock_xp=\"%d\"", scenery->getUnlockXP());
                stream << " unlock_xp=\"" << scenery->getUnlockXP() << "\"";
            }
            if( scenery->getConfirmText().length() > 0 ) {
                stream << " confirm_text=\"" << scenery->getConfirmText().c_str() << "\"";
            }
            //fprintf(file, ">");
            stream << ">";
            for(set<Item *>::const_iterator iter2 = scenery->itemsBegin(); iter2 != scenery->itemsEnd(); ++iter2) {
                const Item *item = *iter2;
                this->saveItem(stream, item);
            }
            if( scenery->getTrap() != NULL ) {
                this->saveTrap(stream, scenery->getTrap());
            }
            if( scenery->getDescription().length() > 0 ) {
                //fprintf(file, "%s", scenery->getDescription().c_str());
                stream << scenery->getDescription().c_str();
            }
            //fprintf(file, "</scenery>\n");
            stream << "</scenery>\n";
        }
        //fprintf(file, "\n");
        stream << "\n";

        qDebug("save items");
        for(set<Item *>::const_iterator iter = location->itemsBegin(); iter != location->itemsEnd(); ++iter) {
            const Item *item = *iter;
            this->saveItem(stream, item);
        }
        //fprintf(file, "\n");
        stream << "\n";

        qDebug("save traps");
        for(set<Trap *>::const_iterator iter = location->trapsBegin(); iter != location->trapsEnd(); ++iter) {
            const Trap *trap = *iter;
            this->saveTrap(stream, trap);
        }
        //fprintf(file, "\n");
        stream << "\n";


        //fprintf(file, "</location>\n\n");
        stream << "</location>\n\n";
    }

    const QuestObjective *quest_objective = this->getQuest()->getQuestObjective();
    if( quest_objective != NULL ) {
        qDebug("save quest objective");
        //fprintf(file, "<quest_objective");
        //fprintf(file, " type=\"%s\"", quest_objective->getType().c_str());
        //fprintf(file, " arg1=\"%s\"", quest_objective->getArg1().c_str());
        //fprintf(file, " gold=\"%d\"", quest_objective->getGold());
        //fprintf(file, " />");
        stream << "<quest_objective type=\"" << quest_objective->getType().c_str() << "\"";
        stream << " arg1=\"" << quest_objective->getArg1().c_str() << "\"";
        stream << " gold=\"" << quest_objective->getGold() << "\"";
        stream << " />";
    }
    //fprintf(file, "\n");
    stream << "\n";

    qDebug("save quest completed text");
    //fprintf(file, "<completed_text>%s</completed_text>\n", this->getQuest()->getCompletedText().c_str());
    //fprintf(file, "\n");
    stream << "<completed_text>" << this->getQuest()->getCompletedText().c_str() << "</completed_text>\n\n";

    qDebug("save current quest info");
    //fprintf(file, "<quest_info complete=\"%s\"/>\n", this->getQuest()->isCompleted() ? "true" : "false");
    //fprintf(file, "\n");
    stream << "<quest_info complete=\"" << (this->getQuest()->isCompleted() ? "true" : "false") << "\"/>\n\n";

    qDebug("save journal");
    //fprintf(file, "<journal>\n");
    stream << "<journal>\n";
    QByteArray encoded = QUrl::toPercentEncoding(this->journal_ss.str().c_str(), QByteArray(), "<>");
    //fprintf(file, "%s", encoded.data());
    stream << encoded;
    //fprintf(file, "</journal>\n");
    //fprintf(file, "\n");
    stream << "</journal>\n\n";

    stream << "<time_hours value=\"" << this->time_hours << "\"/>";

    //fprintf(file, "</savegame>\n");
    stream << "</savegame>\n";

    //fclose(file);

    if( this->permadeath ) {
        this->permadeath_has_savefilename = true;
        this->permadeath_savefilename = full_path;
    }

    game_g->getMainWindow()->unsetCursor();
    return true;
}

void PlayingGamestate::addWidget(QWidget *widget, bool fullscreen_hint) {
    LOG("PlayingGamestate::addWidget() fullscreen_hint? %d\n", fullscreen_hint);
    ASSERT_LOGGER( widget->isWindow() );
    this->widget_stack.push_back(widget);
    /*if( mobile_c ) {
        this->main_stacked_widget->addWidget(widget);
        this->main_stacked_widget->setCurrentWidget(widget);
    }
    else*/ {
        MainWindow *window = game_g->getMainWindow();
        //widget->setParent(window);
        widget->setParent(window->centralWidget());
        widget->setWindowModality(Qt::ApplicationModal);
        //widget->setWindowFlags(Qt::Dialog);
        //widget->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        if( smallscreen_c ) {
            // always fullscreen
            widget->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
#ifdef Q_OS_ANDROID
            // this works whether the MainWindow is maximized or fullscreen:
            widget->showMaximized();
#else
            widget->showFullScreen();
#endif
        }
        else if( game_g->isFullscreen() ) {
            widget->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
            if( fullscreen_hint ) {
                widget->showFullScreen();
                widget->resize(QApplication::desktop()->width(), QApplication::desktop()->height()); // workaround for Ubuntu bug where windows sometimes aren't fullscreen?! No harm in having it for all platforms
            }
            else {
                game_g->resizeTopLevelWidget(widget);
                widget->show();
            }
        }
        else {
            // always windowed
            widget->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
            game_g->resizeTopLevelWidget(widget);
            widget->show();
        }
    }
    this->view->setEnabled(false);
    this->view->releaseKeyboard();
}

void PlayingGamestate::addTextEffect(const string &text, int duration_ms) {
    QPointF pos = this->view->mapToScene(0.5f*view->width(), 0.5f*view->height());
    this->addTextEffect(text, Vector2D(pos.x(), pos.y()), duration_ms);
}

void PlayingGamestate::addTextEffect(const string &text, Vector2D pos, int duration_ms) {
    this->addTextEffect(text, pos, duration_ms, 255, 255, 255);
}

void PlayingGamestate::addTextEffect(const string &text, Vector2D pos, int duration_ms, int r, int g, int b) {
    LOG("PlayingGamestate::addTextEffect(%s, (%f, %f), %d, %d, %d, %d)\n", text.c_str(), pos.x, pos.y, duration_ms, r, g, b);
    QColor qt_color(r, g, b);
    TextEffect *text_effect = new TextEffect(this->view, text.c_str(), duration_ms, qt_color);
    float font_scale = 1.2f/view->getScale();
    float text_effect_width = font_scale*text_effect->boundingRect().width();
    Vector2D text_effect_pos = pos;
    text_effect_pos.x -= 0.5*text_effect_width;
    text_effect_pos.y -= 0.5f;
    QPointF view_topleft = view->mapToScene(0, 0);
    QPointF view_topright = view->mapToScene(view->width()-1, 0);
    /*qDebug("view width: %d", view->width());
    qDebug("left: %f right: %f", view_topleft.x(), view_topright.x());
    qDebug("text_effect_pos.x: %f text_effect_width: %f", text_effect_pos.x, text_effect_width);*/
    if( text_effect_pos.x < view_topleft.x() ) {
        text_effect_pos.x = std::min(pos.x, (float)view_topleft.x());
    }
    else if( text_effect_pos.x + text_effect_width > view_topright.x() ) {
        text_effect_pos.x = std::max(pos.x, (float)view_topright.x()) - text_effect_width;
    }
    text_effect->setPos( text_effect_pos.x, text_effect_pos.y );
    if( this->view_transform_3d ) {
        QTransform transform;
        transform.scale(font_scale, 2.0f*font_scale);
        text_effect->setTransform(transform);
    }
    else {
        text_effect->setScale(font_scale);
    }
    //text_effect->setZValue(text_effect->pos().y() + 1.0f);
    text_effect->setZValue(text_effect->pos().y() + 1000.0f);
    view->addTextEffect(text_effect);
}

void PlayingGamestate::playSound(const string &sound_effect) {
/*#ifndef Q_OS_ANDROID
    qDebug("play sound: %s\n", sound_effect.c_str());
    if( game_g->isSoundEnabled() ) {
        Sound *sound = this->sound_effects[sound_effect];
        ASSERT_LOGGER(sound != NULL);
        if( sound != NULL ) {
            if( sound->state() == Phonon::PlayingState ) {
                qDebug("    already playing");
            }
            else {
                //sound->stop();


                sound->seek(0);
                sound->play();
            }
        }
    }
#endif*/
    game_g->playSound(sound_effect);
}

void PlayingGamestate::advanceQuest() {
    this->c_quest_indx++;
    ASSERT_LOGGER(this->c_quest_indx < this->quest_list.size());
}

QGraphicsItem *PlayingGamestate::addPixmapGraphic(const QPixmap &pixmap, Vector2D pos, float width, bool undo_3d, bool on_ground) {
    qDebug("PlayingGamestate::addPixmapGraphic(%f, %f, %f)", pos.x, pos.y, width);
    QGraphicsPixmapItem *object = new QGraphicsPixmapItem();
    object->setPixmap(pixmap);
    object->setPos(pos.x, pos.y);
    if( on_ground ) {
        object->setZValue(z_value_gui);
    }
    else {
        object->setZValue(pos.y + 1000.0f);
    }
    this->addGraphicsItem(object, width, undo_3d);
    return object;
}

QGraphicsItem *PlayingGamestate::addSpellGraphic(Vector2D pos) {
    qDebug("PlayingGamestate::addSpellGraphic(%f, %f)", pos.x, pos.y);
    //return this->addPixmapGraphic(this->fireball_pixmap, pos, 0.25f, true, false);
    SmokeParticleSystem *ps = new SmokeParticleSystem(fireball_pixmap);
    ps->setType(SmokeParticleSystem::TYPE_RADIAL);
    ps->setBirthRate(50.0f);
    ps->setPos(pos.x, pos.y);
    ps->setZValue(pos.y + 2000.0f);
    float item_scale = 1.0f / fireball_pixmap.width();
    ps->setScale(item_scale);
    scene->addItem(ps);
    return ps;
}

QGraphicsItem *PlayingGamestate::addExplosionGraphic(Vector2D pos) {
    qDebug("PlayingGamestate::addExplosionGraphic(%f, %f)", pos.x, pos.y);
    SmokeParticleSystem *ps = new SmokeParticleSystem(fireball_pixmap);
    ps->setType(SmokeParticleSystem::TYPE_RADIAL);
    ps->setBirthRate(200.0f);
    ps->setLifeExp(500);
    ps->setSystemLifeExp(100);
    ps->setSize(1.0f, 0.0f);
    ps->setPos(pos.x, pos.y);
    ps->setZValue(pos.y + 2000.0f);
    float item_scale = 1.0f / fireball_pixmap.width();
    ps->setScale(item_scale);
    scene->addItem(ps);
    return ps;
}

void PlayingGamestate::addStandardItem(Item *item) {
    this->standard_items[item->getKey()] = item;
}

Item *PlayingGamestate::cloneStandardItem(const string &name) const {
    const Item *item = this->getStandardItem(name);
    return item->clone();
}

const Item *PlayingGamestate::getStandardItem(const string &name) const {
    map<string, Item *>::const_iterator iter = this->standard_items.find(name);
    if( iter == this->standard_items.end() ) {
        LOG("can't find standard item which doesn't exist: %s\n", name.c_str());
        throw string("Unknown standard item");
    }
    const Item *item = iter->second;
    return item;
}

const Spell *PlayingGamestate::findSpell(const string &name) const {
    map<string, Spell *>::const_iterator iter = this->spells.find(name);
    if( iter == this->spells.end() ) {
        LOG("can't find spell: %s\n", name.c_str());
        throw string("Unknown spell");
    }
    const Spell *spell = iter->second;
    return spell;
}

Character *PlayingGamestate::createCharacter(const string &name, const string &template_name) const {
    map<string, CharacterTemplate *>::const_iterator iter = this->character_templates.find(template_name);
    if( iter == this->character_templates.end() ) {
        LOG("can't find character_templates: %s\n", template_name.c_str());
        throw string("Unknown character_template");
    }
    const CharacterTemplate *character_template = iter->second;
    Character *character = new Character(name, true, *character_template);
    return character;
}

Currency *PlayingGamestate::cloneGoldItem(int value) const {
    Currency *item = static_cast<Currency *>(this->cloneStandardItem("Gold"));
    item->setValue(value);
    return item;
}

const Shop *PlayingGamestate::getRandomShop(bool is_random_npc) const {
    for(;;) {
        int r = rand() % shops.size();
        Shop *shop = shops.at(r);
        if( is_random_npc && !shop->isAllowRandomNPC() ) {
            continue;
        }
        return shops.at(r);
    }
}

AnimationLayer *PlayingGamestate::getProjectileAnimationLayer(const string &name) {
    map<string, AnimationLayer *>::iterator image_iter = this->projectile_animation_layers.find(name);
    if( image_iter == this->projectile_animation_layers.end() ) {
        LOG("failed to find image for projectile: %s\n", name.c_str());
        LOG("    image name: %s\n", name.c_str());
        throw string("Failed to find projectile's image");
    }
    return image_iter->second;
}

QPixmap &PlayingGamestate::getItemImage(const string &name) {
    map<string, QPixmap>::iterator image_iter = this->item_images.find(name);
    if( image_iter == this->item_images.end() ) {
        LOG("failed to find image for item: %s\n", name.c_str());
        LOG("    image name: %s\n", name.c_str());
        throw string("Failed to find item's image");
    }
    return image_iter->second;
}

QString PlayingGamestate::getItemString(const Item *item, bool want_weight, bool newlines) const {
    QString item_str = item->getName().c_str();
    if( this->getPlayer()->getCurrentWeapon() == item ) {
        item_str += " [Armed]";
    }
    else if( this->getPlayer()->getCurrentAmmo() == item ) {
        item_str += " [Armed]";
    }
    else if( this->getPlayer()->getCurrentShield() == item ) {
        item_str += " [Armed]";
    }
    else if( this->getPlayer()->getCurrentArmour() == item ) {
        item_str += " [Worn]";
    }
    else if( this->getPlayer()->getCurrentRing() == item ) {
        item_str += " [Worn]";
    }
    if( want_weight && item->getWeight() > 0 ) {
        if( newlines )
            item_str += "\n";
        else
            item_str += " ";
        item_str += "(Weight " + QString::number(item->getWeight()) + ")";
    }
    return item_str;
}

void PlayingGamestate::showInfoDialog(const string &message) {
    this->showInfoDialog(message, "");
}

void PlayingGamestate::showInfoDialog(const string &message, const string &picture) {
    LOG("PlayingGamestate::showInfoDialog(%s)\n", message.c_str());
    InfoDialog *dialog = InfoDialog::createInfoDialogOkay(message, picture);
    this->addWidget(dialog, false);
    dialog->exec();
    this->closeSubWindow();
}

void PlayingGamestate::showInfoDialog(const string &message, const QPixmap *pixmap) {
    LOG("PlayingGamestate::showInfoDialog(%s)\n", message.c_str());
    InfoDialog *dialog = InfoDialog::createInfoDialogOkay(message, pixmap);
    this->addWidget(dialog, false);
    dialog->exec();
    this->closeSubWindow();
}

bool PlayingGamestate::askQuestionDialog(const string &message) {
    LOG("PlayingGamestate::askQuestionDialog(%s)\n", message.c_str());
    InfoDialog *dialog = InfoDialog::createInfoDialogYesNo(message);
    this->addWidget(dialog, false);
    int result = dialog->exec();
    LOG("askQuestionDialog returns %d\n", result);
    this->closeSubWindow();
    return result == 0;
}

int PlayingGamestate::getImageMemorySize() const {
    LOG("PlayingGamestate::getImageMemorySize()\n");
    int memory_size = 0;

    int animation_layers_memory_size = 0;
    for(map<string, LazyAnimationLayer *>::const_iterator iter = this->animation_layers.begin(); iter != this->animation_layers.end(); ++iter) {
        const LazyAnimationLayer *lazy_animation_layer = (*iter).second;
        animation_layers_memory_size += lazy_animation_layer->getMemorySize();
    }
    memory_size += animation_layers_memory_size;
    LOG("NPCs: %d\n", animation_layers_memory_size);

    int scenery_animation_layers_memory_size = 0;
    for(map<string, LazyAnimationLayer *>::const_iterator iter = this->scenery_animation_layers.begin(); iter != this->scenery_animation_layers.end(); ++iter) {
        const LazyAnimationLayer *lazy_animation_layer = (*iter).second;
        scenery_animation_layers_memory_size += lazy_animation_layer->getMemorySize();
    }
    memory_size += scenery_animation_layers_memory_size;
    LOG("Scenery: %d\n", scenery_animation_layers_memory_size);

    int projectile_animation_layers_memory_size = 0;
    for(map<string, AnimationLayer *>::const_iterator iter = this->projectile_animation_layers.begin(); iter != this->projectile_animation_layers.end(); ++iter) {
        const AnimationLayer *animation_layer = (*iter).second;
        projectile_animation_layers_memory_size += animation_layer->getMemorySize();
    }
    memory_size += projectile_animation_layers_memory_size;
    LOG("Projectiles: %d\n", projectile_animation_layers_memory_size);

    int item_images_memory_size = 0;
    for(map<string, QPixmap>::const_iterator iter = this->item_images.begin(); iter != this->item_images.end(); ++iter) {
        const QPixmap &pixmap = (*iter).second;
        item_images_memory_size += pixmap.width() * pixmap.height() * 4;
    }
    memory_size += item_images_memory_size;
    LOG("Items: %d\n", item_images_memory_size);

    int builtin_images_memory_size = 0;
    for(map<string, QPixmap>::const_iterator iter = this->builtin_images.begin(); iter != this->builtin_images.end(); ++iter) {
        const QPixmap &pixmap = (*iter).second;
        builtin_images_memory_size += pixmap.width() * pixmap.height() * 4;
    }
    memory_size += builtin_images_memory_size;
    LOG("Built-in: %d\n", builtin_images_memory_size);

    LOG("Total: %d\n", memory_size);
    return memory_size;
}
