#include "GameIO.h"
#include "Pic1Board.h"
#include "Pic1Config.h"
#include "GameSM.h"
#include "IngredientSM.h"
#include "HeatSM.h"
#include "StirSM.h"
#include "ScoreSM.h"
#include "PanDisplaySM.h"
#include "PeerLink.h"
#include "PeerUart.h"
#include "ScoreTimerDisplay.h"
#include "EInkAsync.h"
#include "PanCanvas.h"
#include "ES_Framework.h"

/* Debugger-visible pending outputs never imply attached, configured hardware. */
volatile uint32_t GameTreatRequests;
volatile uint16_t GameHeatPermille;
volatile bool GameBurnWarning, GameLinkSynchronized, GameAdapterFault;
volatile PanDisplaySMState_t GamePanState;
static PeerLink peer;
static PeerControl control;
static uint32_t link_generation, sync_started, distance_at;
static uint16_t distance_mm;
static bool distance_valid, fault_posted, expected_sync, stopped, hand_in_range;
static bool peer_initialized;
static uint8_t pan_mask, pan_latest, heat_ingredient;
static bool pan_dirty, pan_rendering;
static EInkFrame pan_frame;

static bool playing(void)
{
    GameSMState_t state = QueryGameSM();
    return state == GAME_COOKING_EARLY || state == GAME_COOKING_FINISHABLE;
}
static uint32_t peer_now(void *unused) { (void)unused; return Pic1Board_Now(); }

void GameIO_Stop(void)
{
    stopped = true;
    control = (PeerControl){0};
    if (peer_initialized) PeerLink_SendControl(&peer, &control);
    Pic1Board_Stop();
    GameHeatPermille = 0; GameBurnWarning = false;
    /* Do not wait for UART. PIC2 independently mutes after heartbeat loss. */
}

static void fault(void)
{
    GameAdapterFault = true;
    GameIO_Stop();
}

static void peer_event(const PeerEvent *event, void *unused)
{
    (void)unused;
    if (event->kind == PEER_STATUS) {
        bool was_synced = GameLinkSynchronized;
        GameLinkSynchronized = event->synchronized;
        distance_valid = false;
        hand_in_range = false;
        if (event->synchronized) expected_sync = false;
        else if (was_synced && !expected_sync &&
                 (playing() || QueryGameSM() == GAME_DISH_INTRO)) fault();
        return;
    }
    if (stopped || !PeerLink_IsSynchronized(&peer) ||
        link_generation != GameSM_InputGeneration()) return;
    if (event->kind == PEER_SENSOR_READING) {
        distance_valid = event->valid;
        distance_mm = event->distance_mm; distance_at = event->time_ms;
        bool present = GameTuning.configured && distance_valid &&
            (uint32_t)(Pic1Board_Now()-distance_at) <= GameTuning.heat_sample_max_age_ms &&
            distance_mm >= GameTuning.distance_min_mm && distance_mm <= GameTuning.distance_max_mm;
        bool activity = present && (!hand_in_range || PIC1_HAND_PRESENCE_ACTIVITY);
        hand_in_range = present;
        if (activity && playing() &&
            !GameSM_PostInput(USER_ACTIVITY,distance_at,link_generation)) fault();
        return;
    }
    if (event->kind != PEER_INPUT) return;
    ES_EventType_t type;
    if (event->input_id == 7u) type = START_PRESSED;
    else if (event->input_id == 5u) type = FINISH_PRESSED;
    else if (event->input_id < INGREDIENT_COUNT) type = USER_ACTIVITY;
    else return;
    if (!GameSM_PostInput(type,event->time_ms,link_generation)) { fault(); return; }
    if (event->input_id < INGREDIENT_COUNT &&
        !IngredientSM_PostSwitch(event->input_id,event->time_ms,link_generation)) fault();
}

static void reset_link(void)
{
    expected_sync = true;
    sync_started = Pic1Board_Now();
    distance_valid = false;
    hand_in_range = false;
    link_generation = GameSM_InputGeneration();
    PeerLink_ResetSession(&peer);
    PeerLink_SendControl(&peer,&control);
}

static bool lifecycle(ES_EventType_t type, GameSMEndReason_t reason)
{
    uint32_t generation = GameSM_InputGeneration(), at = GameSM_LifecycleTime();
    control.audio_enabled = false; control.audio_zone = 0;
    reset_link();
    /* Failure of any partial fan-out is terminal; never retry publication. */
    return IngredientSM_PostLifecycle(type,generation) &&
           (type == GAME_END || ScoreSM_PostLifecycle(type,generation)) &&
           HeatSM_PostLifecycle(type,generation,at) &&
           StirSM_PostLifecycle(type,generation,at) &&
           PanDisplaySM_PostLifecycle(type,generation,at,reason);
}

static bool game_output(GameSMAction_t action, int32_t value)
{
    switch (action) {
    case GAME_ACTION_STATE:
        if (value == GAME_FAULT) fault();
        return true;
    case GAME_ACTION_WELCOME: return ScoreTimerDisplay_SetBanner(WELCOME_TEXT,(int16_t)value);
    case GAME_ACTION_DISH: return ScoreTimerDisplay_SetBanner(DISH_TEXT,(int16_t)value);
    case GAME_ACTION_DASHBOARD:
        return ScoreTimerDisplay_ShowLabels() && ScoreTimerDisplay_SetScore(0) &&
               ScoreTimerDisplay_SetTimerSeconds((uint32_t)value);
    case GAME_ACTION_COUNTDOWN: return ScoreTimerDisplay_SetTimerSeconds((uint32_t)value);
    case GAME_ACTION_FINISH_LED:
        control.lamp = value != 0; PeerLink_SendControl(&peer,&control); return true;
    case GAME_ACTION_RESET: return lifecycle(GAME_RESET,GAME_COMPLETED);
    case GAME_ACTION_START: return lifecycle(GAME_START,GAME_COMPLETED);
    case GAME_ACTION_END: return lifecycle(GAME_END,(GameSMEndReason_t)value);
    case GAME_ACTION_TREAT:
        ++GameTreatRequests;
        /* TODO: calibrated dispenser port. No servo model/travel is approved. */
        return true;
    case GAME_ACTION_END_MESSAGE: case GAME_ACTION_BIG_SCORE:
        return true; /* Final-game end screens exist only in the GameButtons build. */
    }
    return false;
}

static bool ingredient_output(IngredientSMAction_t action, uint8_t ingredient)
{
    if (action != INGREDIENT_ACTION_ADDED) return true;
    heat_ingredient = ingredient;
    uint32_t round = GameSM_InputGeneration(), at = IngredientSM_LastAcceptedAt();
    return ScoreSM_PostIngredient(ingredient,round) &&
           HeatSM_PostIngredient(ingredient,round,at) &&
           StirSM_PostIngredient(ingredient,round,at) &&
           PanDisplaySM_PostIngredient(ingredient,round,at);
}

static bool read_distance(uint16_t *mm, uint32_t *at)
{
    if (!distance_valid || !GameLinkSynchronized) return false;
    *mm = distance_mm; *at = distance_at; return true;
}

static bool heat_output(uint16_t heat, bool burning, bool valid)
{
    GameHeatPermille = valid ? heat : 0; GameBurnWarning = burning;
    /* TODO: ninth MAX7219 heat bar and approved burn sound/warning appearance.
     * Preserve the tested eight-driver status chain without changing its map.
     * Audio zones use HeatSM's target, never teammate bench distance bands. */
    control.audio_enabled = valid && !stopped && playing();
    control.audio_zone = 0;
    if (control.audio_enabled) {
        uint16_t target = 0;
        /* Most recently accepted ingredient determines HeatSM's target. */
        if (heat_ingredient < INGREDIENT_COUNT) target = GameTuning.heat_target[heat_ingredient];
        if (!target) control.audio_enabled = false;
        else control.audio_zone = burning || (uint32_t)heat*2u > (uint32_t)target*3u ? 3u :
                                  heat < target ? 1u : 2u;
    }
    PeerLink_SendControl(&peer,&control);
    return true;
}

static bool stir_motion(bool stirring, uint32_t at, uint32_t round)
{
    return PanDisplaySM_PostMotion(stirring,at,round);
}

static bool pan_output(PanDisplaySMState_t state, uint8_t mask, uint8_t latest)
{
    if (state != GamePanState || mask != pan_mask || latest != pan_latest) pan_dirty = true;
    GamePanState = state; pan_mask = mask; pan_latest = latest;
    return true;
}

static bool service_pan(void)
{
    if (!PIC1_ENABLE_EINK) return true;
    if (!EInkAsync_Service()) return false;
    if (pan_rendering) {
        if (!PanCanvas_Service()) return true;
        pan_rendering = false;
        return EInkAsync_Start(&pan_frame);
    }
    if (!pan_dirty || !EInkAsync_CanStart()) return true;
    pan_rendering = true;
    pan_dirty = false;
    return PanCanvas_Start(&pan_frame,GamePanState,pan_mask);
}

bool GameIO_Init(void)
{
    peer_initialized = false;
    stopped = GameAdapterFault = fault_posted = false;
    GameLinkSynchronized = false; distance_valid = false;
    GameTreatRequests = GameHeatPermille = 0; GameBurnWarning = false;
    GamePanState = PAN_ATTRACT; pan_mask = pan_latest = heat_ingredient = 0; pan_dirty = true;
    pan_rendering = false;
    control = (PeerControl){0};
    if (!Pic1Board_Init() || !ScoreTimerDisplay_Init(&Pic1DisplayConfig)) return false;
    PeerUart_Init();
    PeerLink_Init(&peer,PEER_CONTROLLER,1,peer_now,PeerUart_Send,peer_event,0);
    peer_initialized = true;
    const GameSMPort_t game = {Pic1Board_Now,game_output};
    const IngredientSMPort_t ingredient = {GameSM_InputGeneration,GameSM_AcceptsGameplayInput,ingredient_output};
    const ScoreSMPort_t score = {GameSM_InputGeneration,GameSM_AcceptsScoreRound,ScoreTimerDisplay_SetScore,0,0,false};
    const HeatSMPort_t heat = {Pic1Board_Now,GameSM_InputGeneration,GameSM_AcceptsGameplayInput,
        GameSM_AcceptsScoreRound,read_distance,heat_output,ScoreSM_PostHeat,ScoreSM_PostBurn};
    const StirSMPort_t stir = {Pic1Board_Now,GameSM_InputGeneration,GameSM_AcceptsGameplayInput,
        GameSM_AcceptsScoreRound,Pic1Board_Motor,stir_motion,ScoreSM_PostStir};
    const PanDisplaySMPort_t pan = {Pic1Board_Now,GameSM_InputGeneration,GameSM_AcceptsGameplayInput,pan_output};
    return (!PIC1_ENABLE_EINK || EInkAsync_Init(Pic1Board_EInkPort())) &&
           GameSM_BindPort(&game) && IngredientSM_BindPort(&ingredient) &&
           ScoreSM_BindPort(&score) && ScoreSM_BindTuning(&GameTuning) &&
           HeatSM_BindPort(&heat,&GameTuning) && StirSM_BindPort(&stir,&GameTuning) &&
           PanDisplaySM_BindPort(&pan,&GameTuning);
}

bool GameIO_Joystick(uint32_t angle, bool home, bool moved, uint32_t at)
{
    if (!playing() || stopped || !GameLinkSynchronized) return true;
    uint32_t generation = GameSM_InputGeneration();
    if (moved && !GameSM_PostInput(USER_ACTIVITY,at,generation)) return false;
    return StirSM_PostMotion(angle,home,at,generation);
}

bool GameIO_Service(void)
{
    uint32_t now = Pic1Board_Now();
    PeerUart_Service(&peer);
    PeerLink_Service(&peer);
    if (expected_sync && (playing() || QueryGameSM() == GAME_DISH_INTRO) &&
        (uint32_t)(now-sync_started) >= PEER_SYNC_TIMEOUT_MS) fault();
    if (!stopped && (!Pic1Board_Service() || !service_pan() ||
        (now >= 100u && ScoreTimerDisplay_Service() == SCORE_TIMER_FAULT) ||
        GameSM_HasFault() || IngredientSM_HasFault() || HeatSM_HasFault() ||
        StirSM_HasFault() || ScoreSM_HasFault() || PanDisplaySM_HasFault())) fault();
    if (GameAdapterFault && !fault_posted) {
        fault_posted = PostGameSM((ES_Event_t){ES_ERROR,0});
        return fault_posted;
    }
    return false;
}
