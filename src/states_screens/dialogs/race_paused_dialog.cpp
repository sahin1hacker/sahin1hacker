//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2010-2015 Marianne Gagnon
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "states_screens/dialogs/race_paused_dialog.hpp"

#include <string>

#include "audio/music_manager.hpp"
#include "audio/sfx_manager.hpp"
#include "challenges/story_mode_timer.hpp"
#include "config/user_config.hpp"
#include "guiengine/emoji_keyboard.hpp"
#include "guiengine/engine.hpp"
#include "guiengine/scalable_font.hpp"
#include "guiengine/widgets/CGUIEditBox.hpp"
#include "guiengine/widgets/icon_button_widget.hpp"
#include "guiengine/widgets/ribbon_widget.hpp"
#include "io/file_manager.hpp"
#include "modes/overworld.hpp"
#include "modes/world.hpp"
#include "network/protocols/client_lobby.hpp"
#include "network/network_config.hpp"
#include "network/network_string.hpp"
#include "network/stk_host.hpp"
#include "race/race_manager.hpp"
#include "states_screens/help_screen_1.hpp"
#include "states_screens/main_menu_screen.hpp"
#include "states_screens/race_gui_base.hpp"
#include "states_screens/race_setup_screen.hpp"
#include "states_screens/options/options_screen_general.hpp"
#include "states_screens/state_manager.hpp"
#include "utils/string_utils.hpp"
#include "utils/translation.hpp"

using namespace GUIEngine;
using namespace irr::core;
using namespace irr::gui;

// ----------------------------------------------------------------------------

RacePausedDialog::RacePausedDialog(const float percentWidth,
                                   const float percentHeight) :
    ModalDialog(percentWidth, percentHeight)
{
    m_self_destroy = false;
    m_from_overworld = false;

    if (dynamic_cast<OverWorld*>(World::getWorld()) != NULL)
    {
        loadFromFile("overworld_dialog.stkgui");
        m_from_overworld = true;
    }
    else if (!NetworkConfig::get()->isNetworking())
    {
        loadFromFile("race_paused_dialog.stkgui");
    }
    else
    {
        loadFromFile("online/network_ingame_dialog.stkgui");
    }

    GUIEngine::RibbonWidget* back_btn = getWidget<RibbonWidget>("backbtnribbon");
    back_btn->setFocusForPlayer( PLAYER_ID_GAME_MASTER );

    if (NetworkConfig::get()->isNetworking())
    {
        music_manager->pauseMusic();
        SFXManager::get()->pauseAll();
        m_text_box->clearListeners();
        m_text_box->setTextBoxType(TBT_CAP_SENTENCES);
        // Unicode enter arrow
        getWidget("send")->setText(L"\u21B2");
        // Unicode smile emoji
        getWidget("emoji")->setText(L"\u263A");
        if (UserConfigParams::m_lobby_chat && UserConfigParams::m_race_chat)
        {
            m_text_box->setActive(true);
            getWidget("send")->setVisible(true);
            getWidget("emoji")->setVisible(true);
            m_text_box->addListener(this);
            auto cl = LobbyProtocol::get<ClientLobby>();
            if (cl && !cl->serverEnabledChat())
            {
                m_text_box->setActive(false);
                getWidget("send")->setActive(false);
                getWidget("emoji")->setActive(false);
            }
        }
        else
        {
            m_text_box->setActive(false);
            m_text_box->setText(
                _("Chat is disabled, enable in options menu."));
            getWidget("send")->setVisible(false);
            getWidget("emoji")->setVisible(false);
        }
    }
    else
    {
        World::getWorld()->schedulePause(WorldStatus::IN_GAME_MENU_PHASE);
    }

#ifndef MOBILE_STK
    if (m_text_box && UserConfigParams::m_lobby_chat)
        m_text_box->setFocusForPlayer(PLAYER_ID_GAME_MASTER);
#endif
}   // RacePausedDialog

// ----------------------------------------------------------------------------
RacePausedDialog::~RacePausedDialog()
{
    if (NetworkConfig::get()->isNetworking())
    {
        music_manager->resumeMusic();
        SFXManager::get()->resumeAll();
    }
    else
    {
        World::getWorld()->scheduleUnpause();
    }
}   // ~RacePausedDialog

// ----------------------------------------------------------------------------

void RacePausedDialog::loadedFromFile()
{
    // disable the "restart" button in GPs
    if (race_manager->getMajorMode() == RaceManager::MAJOR_MODE_GRAND_PRIX)
    {
        GUIEngine::RibbonWidget* choice_ribbon =
            getWidget<GUIEngine::RibbonWidget>("choiceribbon");
#ifdef DEBUG
        const bool success = choice_ribbon->deleteChild("restart");
        assert(success);
#else
        choice_ribbon->deleteChild("restart");
#endif
    }
    // Remove "endrace" button for types not (yet?) implemented
    // Also don't show it unless the race has started. Prevents finishing in
    // a time of 0:00:00.
    if ((race_manager->getMinorMode() != RaceManager::MINOR_MODE_NORMAL_RACE  &&
         race_manager->getMinorMode() != RaceManager::MINOR_MODE_TIME_TRIAL ) ||
         World::getWorld()->isStartPhase() ||
         NetworkConfig::get()->isNetworking())
    {
        GUIEngine::RibbonWidget* choice_ribbon =
            getWidget<GUIEngine::RibbonWidget>("choiceribbon");
        choice_ribbon->deleteChild("endrace");
        // No restart in network game
        if (NetworkConfig::get()->isNetworking())
        {
            choice_ribbon->deleteChild("restart");
        }
    }
}

// ----------------------------------------------------------------------------

void RacePausedDialog::onEnterPressedInternal()
{
}   // onEnterPressedInternal

// ----------------------------------------------------------------------------

GUIEngine::EventPropagation
           RacePausedDialog::processEvent(const std::string& eventSource)
{
    GUIEngine::RibbonWidget* choice_ribbon =
            getWidget<GUIEngine::RibbonWidget>("choiceribbon");

    if (eventSource == "send" && m_text_box)
    {
        onEnterPressed(m_text_box->getText());
        return GUIEngine::EVENT_BLOCK;
    }
    else if (eventSource == "emoji" && m_text_box &&
        !ScreenKeyboard::isActive())
    {
        EmojiKeyboard* ek = new EmojiKeyboard(1.0f, 0.40f,
            m_text_box->getIrrlichtElement<CGUIEditBox>());
        ek->init();
        return GUIEngine::EVENT_BLOCK;
    }
    else if (eventSource == "backbtnribbon")
    {
        // unpausing is done in the destructor so nothing more to do here
        ModalDialog::dismiss();
        return GUIEngine::EVENT_BLOCK;
    }
    else if (eventSource == "choiceribbon")
    {
        const std::string& selection =
            choice_ribbon->getSelectionIDString(PLAYER_ID_GAME_MASTER);

        if (selection == "exit")
        {
            ModalDialog::dismiss();
            if (STKHost::existHost())
            {
                STKHost::get()->shutdown();
            }
            race_manager->exitRace();
            race_manager->setAIKartOverride("");

            if (NetworkConfig::get()->isNetworking())
            {
                StateManager::get()->resetAndSetStack(
                    NetworkConfig::get()->getResetScreens().data());
                NetworkConfig::get()->unsetNetworking();
            }
            else
            {
                StateManager::get()->resetAndGoToScreen(MainMenuScreen::getInstance());

                // Pause story mode timer when quitting story mode
                if (m_from_overworld)
                    story_mode_timer->pauseTimer(/*loading screen*/ false);

                if (race_manager->raceWasStartedFromOverworld())
                {
                    OverWorld::enterOverWorld();
                }
            }
            return GUIEngine::EVENT_BLOCK;
        }
        else if (selection == "help")
        {
            dismiss();
            HelpScreen1::getInstance()->push();
            return GUIEngine::EVENT_BLOCK;
        }
        else if (selection == "options")
        {
            dismiss();
            OptionsScreenGeneral::getInstance()->push();
            return GUIEngine::EVENT_BLOCK;
        }
        else if (selection == "restart")
        {
            ModalDialog::dismiss();
            World::getWorld()->scheduleUnpause();
            race_manager->rerunRace();
            return GUIEngine::EVENT_BLOCK;
        }
        else if (selection == "newrace")
        {
            ModalDialog::dismiss();
            if (NetworkConfig::get()->isNetworking())
            {
                // back lobby
                NetworkString back(PROTOCOL_LOBBY_ROOM);
                back.setSynchronous(true);
                back.addUInt8(LobbyProtocol::LE_CLIENT_BACK_LOBBY);
                STKHost::get()->sendToServer(&back, true);
            }
            else
            {
                World::getWorld()->scheduleUnpause();
                race_manager->exitRace();
                Screen* new_stack[] =
                    {
                        MainMenuScreen::getInstance(),
                        RaceSetupScreen::getInstance(),
                        NULL
                    };
                StateManager::get()->resetAndSetStack(new_stack);
            }
            return GUIEngine::EVENT_BLOCK;
        }
        else if (selection == "endrace")
        {
            ModalDialog::dismiss();
            World::getWorld()->getRaceGUI()->removeReferee();
            World::getWorld()->endRaceEarly();
            return GUIEngine::EVENT_BLOCK;
        }
        else if (selection == "selectkart")
        {
            dynamic_cast<OverWorld*>(World::getWorld())->scheduleSelectKart();
            ModalDialog::dismiss();
            return GUIEngine::EVENT_BLOCK;
        }
    }
    return GUIEngine::EVENT_LET;
}   // processEvent

// ----------------------------------------------------------------------------
void RacePausedDialog::beforeAddingWidgets()
{
    GUIEngine::RibbonWidget* choice_ribbon =
        getWidget<GUIEngine::RibbonWidget>("choiceribbon");

    bool showSetupNewRace = race_manager->raceWasStartedFromOverworld();
    int index = choice_ribbon->findItemNamed("newrace");
    if (index != -1)
        choice_ribbon->setItemVisible(index, !showSetupNewRace);

    // Disable in game menu to avoid timer desync if not racing in network
    // game
    if (NetworkConfig::get()->isNetworking() &&
        !(World::getWorld()->getPhase() == WorldStatus::MUSIC_PHASE ||
        World::getWorld()->getPhase() == WorldStatus::RACE_PHASE))
    {
        index = choice_ribbon->findItemNamed("help");
        if (index != -1)
            choice_ribbon->setItemVisible(index, false);
        index = choice_ribbon->findItemNamed("options");
        if (index != -1)
            choice_ribbon->setItemVisible(index, false);
        index = choice_ribbon->findItemNamed("newrace");
        if (index != -1)
            choice_ribbon->setItemVisible(index, false);
    }
    if (NetworkConfig::get()->isNetworking())
        m_text_box = getWidget<TextBoxWidget>("chat");
    else
        m_text_box = NULL;
}   // beforeAddingWidgets

// ----------------------------------------------------------------------------
bool RacePausedDialog::onEnterPressed(const irr::core::stringw& text)
{
    if (auto cl = LobbyProtocol::get<ClientLobby>())
        cl->sendChat(text);
    m_self_destroy = true;
    return true;
}   // onEnterPressed
