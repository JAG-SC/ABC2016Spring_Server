﻿
/**

    @file   battle_scene.cpp
    @brief  battle_scene

    @author 新ゝ月かりな(NewNotMoon)
    @date   created on 2016/02/09

*/

#include <c2xa/scene/battle_scene.hpp>
#include <c2xa/config.hpp>
#include <c2xa/exception.hpp>
#include <c2xa/player.hpp>
#include <c2xa/communication/node.hpp>
#include <c2xa/communication/parse.hpp>
#include <c2xa/scene/result_scene.hpp>
#include <AudioEngine.h>

using namespace c2xa;
using namespace c2xa::scene;

battle_scene::battle_scene()
{
}

battle_scene::~battle_scene()
{
    cocos2d::experimental::AudioEngine::stop( bgm_id );
}

bool battle_scene::init( communication_node* com_node_ )
{
    using namespace cocos2d;
    if( !Scene::init() )
    {
        return false;
    }
    state_ = state::ready;

    setName( "battle_scene" );
    scheduleUpdate();

    com_node_->retain();
    com_node_->removeFromParent();
    addChild( com_node_ );
    com_node_->release();

    auto player_1 = player::create( player::number::_1p );
    player_1->setName( "player_1" );
    addChild( player_1, 10 );

    auto player_2 = player::create( player::number::_2p );
    player_2->setName( "player_2" );
    addChild( player_2, 10 );

    auto bg_ = Sprite::create( "img/title_bg.png" );
    bg_->setAnchorPoint( Vec2::ANCHOR_BOTTOM_LEFT );
    addChild( bg_, 1 );

    auto bg_img_ = Sprite::create( "img/battle_bg.png" );
    bg_img_->setAnchorPoint( Vec2::ANCHOR_BOTTOM_LEFT );
    addChild( bg_img_, 4 );

    auto bg_front_img_ = Sprite::create( "img/battle_front.png" );
    bg_front_img_->setAnchorPoint( Vec2::ANCHOR_BOTTOM_LEFT );
    addChild( bg_front_img_, 7 );

    auto particle_smoke_ = ParticleSystemQuad::create( "particle/smoke.plist" );
    particle_smoke_->setPosition( Vec2{ app_width / 2, 700 } );
    particle_smoke_->resetSystem();
    addChild( particle_smoke_, 3 );

    auto grid_fire_ = NodeGrid::create();
    grid_fire_->setContentSize( Size{ 2000, 1080 } );
    auto add_fire = [ grid_fire_ ]( bool high_, Vec2 pos_ )
    {
        auto pf_ = ParticleSystemQuad::create( high_ ? "particle/fire.plist" : "particle/fire_low.plist" );
        pf_->setPosition( pos_ );
        pf_->resetSystem();
        grid_fire_->addChild( pf_, 5 );
    };
    add_fire( true, { 100, 500 } );
    add_fire( true, { app_width - 100, 500 } );
    add_fire( true, { app_width - 400, 400 } );
    add_fire( true, { 400, 300 } );
    add_fire( false, { 700, 450 } );
    add_fire( false, { 1200, 400 } );
    grid_fire_->runAction( RepeatForever::create( Waves::create( 1.f, Size{ 5, 10 }, 5, 0, false, true ) ) );
    addChild( grid_fire_, 5 );

    auto dispatcher = Director::getInstance()->getEventDispatcher();
    auto keyboard_listener_ = EventListenerKeyboard::create();
    keyboard_listener_->onKeyPressed = [ = ]( EventKeyboard::KeyCode key_, Event* event_ )
    {
        if( key_ == EventKeyboard::KeyCode::KEY_D )
        {
            state_ = state::end;
            Director::getInstance()
                ->replaceScene(
                    TransitionFade::create(
                        1.0f,
                        result_scene::create( com_node_ )
                        )
                    );
            dispatcher->removeEventListenersForTarget( this );
        }
    };
    dispatcher->addEventListenerWithSceneGraphPriority( keyboard_listener_, this );

    bgm_id = cocos2d::experimental::AudioEngine::play2d( "snd/battle_bgm.mp3", true, 0.3f, nullptr );

    auto cnts_ = Label::createWithTTF( "READY", "font/Stroke.ttf", 100 );
    cnts_->setPosition( { app_width / 2, app_height / 2 } );
    cnts_->setColor( Color3B{ 255, 255, 99 } );
    cnts_->runAction(
        Sequence::create(
            DelayTime::create( 2.5f ),
            CallFunc::create([ cnts_ ]{ cnts_->setString( "3" ); }),
            DelayTime::create( 1.f ),
            CallFunc::create([ cnts_ ]{ cnts_->setString( "2" ); }),
            DelayTime::create( 1.f ),
            CallFunc::create([ cnts_ ]{ cnts_->setString( "1" ); }),
            DelayTime::create( 1.f ),
            CallFunc::create([ cnts_, this ]{
                cnts_->setString( "fight" );
                state_ = state::fighting;
                cocos2d::experimental::AudioEngine::play2d( "snd/fight.mp3", false, 0.3f, nullptr );
            }),
            DelayTime::create( 0.3f ),
            FadeOut::create( 0.7f ),
            RemoveSelf::create( true ),
            nullptr )
    );
    addChild( cnts_, 50 );

    return true;
}

void battle_scene::update( float )
{
    using namespace cocos2d;

    auto com_node_ = static_cast<communication_node*>( getChildByName( "com_node" ) );
    auto player_1 = static_cast<player*>( getChildByName( "player_1" ) );
    auto player_2 = static_cast<player*>( getChildByName( "player_2" ) );

    // endになったらcom_node_は消えますのでreceiveすると落ちます
    if( state_ != state::end )
    {
        com_node_->receive_1p( [ = ]( auto&& com_, auto&& buffer_ )
        {
            player_1->update_state( buffer_ );
            com_->send_1p();
        } );
        com_node_->receive_2p( [ = ]( auto&& com_, auto&& buffer_ )
        {
            player_2->update_state( buffer_ );
            com_->send_2p();
        } );
    }

    if( state_ == state::fighting )
    {
        player_1->judge();
        player_2->judge();
        auto judge = []( player* player_a, player* player_b )
        {
            auto aa = player_a->get_action();
            auto ab = player_b->get_action();
            if( player_a->check_attacking() && player_b->get_state() == player::state::defenseless )
            {
                if( ab == action::idle || ab == action::messy )
                {
                    player_b->damage( 120 );
                }
                else if( aa == action::thrust )
                {
                    if( ab == action::thrust )
                    {
                        player_a->damage( 100 );
                        player_b->damage( 100 );
                    }
                    else if( ab == action::slash )
                    {
                    }
                    else if( ab == action::guard )
                    {
                        player_b->damage( 100 );
                    }
                }
                else if( aa == action::slash )
                {
                    if( ab == action::thrust )
                    {
                        player_b->damage( 120 );
                    }
                    else if( ab == action::slash )
                    {
                        player_a->damage( 120 );
                        player_b->damage( 120 );
                    }
                    else if( ab == action::guard )
                    {
                        player_a->damage( 120 );
                    }
                }
            }
        };
        judge( player_1, player_2 );
        judge( player_2, player_1 );

        auto result_effect = [ = ]( int winner )
        {
            cocos2d::experimental::AudioEngine::play2d( "snd/gong.mp3", false, 0.3f, nullptr );
            runAction(
                Sequence::create(
                    DelayTime::create( 1.f ),
                    CallFunc::create( [ = ]{
                        if( winner == 0 )
                        {
                            auto draw_ = Sprite::create( "img/draw.png" );
                            draw_->setPosition( { app_width / 2, app_height / 4 } );
                            draw_->setOpacity( 0 );
                            draw_->setScale( 0.7f );
                            draw_->runAction( Spawn::create( EaseBackOut::create( ScaleTo::create( 0.3f, 1 ) ), FadeIn::create( 0.3f ), nullptr ) );
                            addChild( draw_, 20 );
                        }
                        else
                        {
                            auto win_x_ = winner == 1 ? app_width / 5 : app_width * 4 / 5;
                            auto lose_x_ = winner == 2 ? app_width / 5 : app_width * 4 / 5;
                            auto win_ = Sprite::create( "img/win.png" );
                            win_->setPosition( { win_x_, app_height / 4 } );
                            win_->setOpacity( 0 );
                            win_->setScale( 0 );
                            win_->runAction( Spawn::create( EaseBackOut::create( ScaleTo::create( 0.3f, 1 ) ), FadeIn::create( 0.3f ), nullptr ) );
                            addChild( win_, 20 );
                            auto particle_win_ = ParticleSystemQuad::create( "particle/spark_star.plist" );
                            particle_win_->setPosition( Vec2{ win_x_, app_height / 4 } );
                            particle_win_->resetSystem();
                            addChild( particle_win_, 19 );

                            auto lose_ = Sprite::create( "img/lose.png" );
                            lose_->setOpacity( 0 );
                            lose_->setScale( 0.7f );
                            lose_->setPosition( { lose_x_, app_height / 4 - 200 } );
                            lose_->runAction( Spawn::create( MoveTo::create( 0.5f, { lose_x_, app_height / 4 } ), FadeIn::create( 0.5f ), nullptr ) );
                            addChild( lose_, 20 );
                        }
                    } ),
                    DelayTime::create( 0.5f ),
                    CallFunc::create( [ = ]
                    {
                        auto dispatcher = Director::getInstance()->getEventDispatcher();
                        auto keyboard_listener_ = EventListenerKeyboard::create();
                        keyboard_listener_->onKeyPressed = [ = ]( EventKeyboard::KeyCode key_, Event* event_ )
                        {
                            Director::getInstance()
                                ->replaceScene(
                                    TransitionFade::create(
                                        1.0f,
                                        result_scene::create( com_node_ )
                                    )
                                );
                            dispatcher->removeEventListenersForTarget( this );
                        };
                        dispatcher->addEventListenerWithSceneGraphPriority( keyboard_listener_, this );
                    }),
                    nullptr ) );
        };

        if( player_1->is_dead() && player_2->is_dead() )
        {
            // 相打ち
            state_ = state::end;
            result_effect( 0 );
        }
        else if( player_1->is_dead() )
        {
            // 2p勝利
            state_ = state::end;
            result_effect( 2 );
        }
        else if( player_2->is_dead() )
        {
            // 1p勝利
            state_ = state::end;
            result_effect( 1 );
        }
    }

    auto gauge1_text = static_cast<cocos2d::Label*>( getChildByName( "gauge1_text" ) );
    if( gauge1_text == nullptr )
    {
        gauge1_text = Label::createWithTTF( "1p", "font/Stroke.ttf", 32 );
        gauge1_text->setAnchorPoint( Vec2::ANCHOR_BOTTOM_LEFT );
        gauge1_text->setPosition( { 80, app_height - 100 } );
        gauge1_text->setName( "gauge1_text" );
        addChild( gauge1_text, 30 );
    }
    gauge1_text->setString( ( "1p: " + std::to_string( player_1->get_hp() ) ).c_str() );
    auto gauge1 = static_cast<cocos2d::Sprite*>( getChildByName( "gauge1" ) );
    if( gauge1 == nullptr )
    {
        auto gauge_frame = Sprite::create( "img/gauge_frame.png" );
        gauge_frame->setAnchorPoint( Vec2::ANCHOR_TOP_LEFT );
        gauge_frame->setPosition( { 50, app_height - 100 } );

        gauge1 = Sprite::create( "img/gauge.png" );
        gauge1->setAnchorPoint( Vec2::ANCHOR_TOP_LEFT );
        gauge1->setPosition( { 70, app_height - 120 } );
        gauge1->setName( "gauge1" );

        addChild( gauge1, 30 );
        addChild( gauge_frame, 30 );
    }
    gauge1->setScaleX( static_cast<float>( player_1->get_hp() ) / player_max_hp );
    auto gauge2_text = static_cast<cocos2d::Label*>( getChildByName( "gauge2_text" ) );
    if( gauge2_text == nullptr )
    {
        gauge2_text = Label::createWithTTF( "2p", "font/Stroke.ttf", 32 );
        gauge2_text->setAnchorPoint( Vec2::ANCHOR_BOTTOM_RIGHT );
        gauge2_text->setPosition( { app_width - 80, app_height - 100 } );
        gauge2_text->setName( "gauge2_text" );
        addChild( gauge2_text, 30 );
    }
    gauge2_text->setString( ( "2p: " + std::to_string( player_2->get_hp() ) ).c_str() );
    auto gauge2 = static_cast<cocos2d::Sprite*>( getChildByName( "gauge2" ) );
    if( gauge2 == nullptr )
    {
        auto gauge_frame = Sprite::create( "img/gauge_frame.png" );
        gauge_frame->setAnchorPoint( Vec2::ANCHOR_TOP_RIGHT );
        gauge_frame->setPosition( { app_width - 50, app_height - 100 } );

        gauge2 = Sprite::create( "img/gauge.png" );
        gauge2->setAnchorPoint( Vec2::ANCHOR_TOP_RIGHT );
        gauge2->setPosition( { app_width - 70, app_height - 120 } );
        gauge2->setName( "gauge2" );

        addChild( gauge2, 30 );
        addChild( gauge_frame, 30 );
    }
    gauge2->setScaleX( static_cast<float>( player_2->get_hp() ) / player_max_hp );

#ifdef COCOS2D_DEBUG
    auto layer_1 = static_cast<cocos2d::Layer*>( getChildByName( "layer_1" ) );
    if( layer_1 == nullptr )
    {
        layer_1 = Layer::create();
        layer_1->setName( "layer_1" );
        addChild( layer_1, 20 );
    }
    player_1->debug( layer_1 );
    auto layer_2 = static_cast<cocos2d::Layer*>( getChildByName( "layer_2" ) );
    if( layer_2 == nullptr )
    {
        layer_2 = Layer::create();
        layer_2->setName( "layer_2" );
        addChild( layer_2, 20 );
    }
    player_2->debug( layer_2 );
#endif//COCOS2D_DEBUG
}