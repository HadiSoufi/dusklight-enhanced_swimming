#include "mods/hook.hpp"
#include "mods/service.hpp"

#include "mods/svc/hook.h"
#include "mods/svc/log.h"

#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter_button.h"
#include "d/d_particle.h"
#include "d/d_particle_name.h"
#include "m_Do/m_Do_controller_pad.h"

DEFINE_MOD();

IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(LogService,  svc_log);

static constexpr f32 kZoraForwardSpeed = 13.0f;

static constexpr f32 kSprintFactor    = 1.6f;
static constexpr f32 kSpinDashBoost   = 1.6f;
static constexpr f32 kSpinBurstPeak   = 2.0f;

/* ------------------------------------------------------------------
 *  Swim speed
 *  setSwimMoveAnime mutates mMaxSpeed each frame, so scaling in-place
 *  compounds. Pre-hook removes the old factor; post-hook applies the new.
 * ------------------------------------------------------------------ */
DEFINE_HOOK(&daAlink_c::setSwimMoveAnime, SwimMoveAnime);

static f32 g_appliedFactor = 1.0f;

static int g_boostTimer    = 0;
static int g_boostTimerMax = 1;
static int g_vfxTimer      = 0;

static HookAction on_swim_move_anime_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* self = mods::arg<daAlink_c*>(args, 0);
    if (g_appliedFactor != 1.0f) {
        self->mMaxSpeed /= g_appliedFactor;
        g_appliedFactor = 1.0f;
    }
    return HOOK_CONTINUE;
}

static void on_swim_move_anime_post(ModContext*, void* args, void*, void*) {
    daAlink_c* self = mods::arg<daAlink_c*>(args, 0);
    if (!self->checkZoraWearAbility()) return;

    f32 factor = 1.0f;
    if (mDoCPd_c::getHoldR(PAD_1))
        factor = kSprintFactor;

    if (self->field_0x2f98 == 0 && self->checkSwimDashMode())
        factor *= kZoraForwardSpeed / self->mpHIO->mSwim.m.mForwardMaxSpeed;

    if (g_boostTimer > 0 && self->getZoraSwim())
        factor *= kSpinDashBoost;

    if (factor == 1.0f) return;

    self->mMaxSpeed *= factor;
    g_appliedFactor = factor;
}

/* ------------------------------------------------------------------
 *  Dive remap: A → R
 * ------------------------------------------------------------------ */
DEFINE_HOOK(&daAlink_c::setStickData, SetStickData);

static bool is_swim_proc(u16 procID) {
    return procID >= daAlink_c::PROC_SWIM_UP && procID <= daAlink_c::PROC_SWIM_DIVE;
}

static bool is_zora_swim(const daAlink_c* link) {
    return link->checkZoraWearAbility() &&
           link->mProcID == daAlink_c::PROC_SWIM_MOVE &&
           !link->getZoraSwim();
}

static bool is_underwater(const daAlink_c* link) {
    return link->current.pos.y < link->mWaterY;
}

static bool is_underwater_dive(const daAlink_c* link) {
    return link->mProcID == daAlink_c::PROC_SWIM_DIVE;
}

static void on_set_stick_data_post(ModContext*, void* args, void*, void*) {
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (!link->checkZoraWearAbility() || !is_swim_proc(link->mProcID)) return;

    link->mItemTrigger &= static_cast<u8>(~daAlink_c::BTN_A);
    link->mItemButton  &= static_cast<u8>(~daAlink_c::BTN_A);
    if (mDoCPd_c::getTrigR(PAD_1)) link->mItemTrigger |= daAlink_c::BTN_A;
    if (mDoCPd_c::getHoldR(PAD_1)) link->mItemButton  |= daAlink_c::BTN_A;

    if (is_zora_swim(link) && is_underwater(link))
        dComIfGp_setRStatusForce(BUTTON_STATUS_DIVE, 2);
}

/* ------------------------------------------------------------------
 *  Surface dash + dive dash
 * ------------------------------------------------------------------ */
DEFINE_HOOK(&daAlink_c::procSwimMove, ProcSwimMove);

static HookAction on_proc_swim_move_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* self = mods::arg<daAlink_c*>(args, 0);
    if (!self->checkZoraWearAbility()) return HOOK_CONTINUE;

    if (mDoCPd_c::getTrigA(PAD_1)) {
        self->mItemTrigger |= daAlink_c::BTN_A;
        self->setTalkStatus();
        self->orderTalk(1);
        self->mItemTrigger &= static_cast<u8>(~daAlink_c::BTN_A);
    }

    if (self->getZoraSwim() || self->checkZoraSwimMove()) {
        if (self->getZoraSwim() && mDoCPd_c::getTrigA(PAD_1) && g_boostTimer == 0) {
            self->mNormalSpeed = kSpinBurstPeak * self->mMaxSpeed;
            g_vfxTimer      = 0;
            g_boostTimer    = self->mpHIO->mSwim.m.field_0x5c;
            g_boostTimerMax = g_boostTimer;
        }
        return HOOK_CONTINUE;
    }

    if (!mDoCPd_c::getTrigA(PAD_1)) return HOOK_CONTINUE;
    if (self->checkHookshotAnime()) return HOOK_CONTINUE;
    if (self->field_0x30d0 != 0 || self->field_0x30d2 != 0) return HOOK_CONTINUE;
    if (!self->checkUnderMove0BckNoArc(daAlink_c::ANM_SWIM_A)) return HOOK_CONTINUE;

    self->onNoResetFlg1(daAlink_c::FLG1_DASH_MODE);
    self->field_0x30d0 = self->mpHIO->mSwim.m.field_0x5c;
    self->setSingleAnimeParam(daAlink_c::ANM_SWIM_DASH, &self->mpHIO->mSwim.m.mDashAnm);
    self->field_0x3198 = daAlink_c::ANM_SWIM_DASH;
    return HOOK_CONTINUE;
}

/* ------------------------------------------------------------------
 *  Dive dash deceleration
 * ------------------------------------------------------------------ */
DEFINE_HOOK(&daAlink_c::setNormalSpeedF, SetNormalSpeedF);

static HookAction on_set_normal_speed_pre(ModContext*, void* args, void*, void*) {
    daAlink_c* self = mods::arg<daAlink_c*>(args, 0);
    if (self->getZoraSwim() && self->mNormalSpeed > self->mMaxSpeed)
        mods::arg_ref<f32>(args, 2) = self->mpHIO->mSwim.m.mDashDeceleration;
    return HOOK_CONTINUE;
}

/* ------------------------------------------------------------------
 *  HUD: action text, dive prompts
 * ------------------------------------------------------------------ */
static void strip_a_suffix(char* buf) {
    if (!buf || !buf[0]) return;

    int len = 0;
    while (buf[len]) {
        u8 c = static_cast<u8>(buf[len]);
        if (c >= 0x80 || c == 0x1B || (c > 0 && c < 0x20)) break;
        len++;
    }
    buf[len] = '\0';

    if (len >= 4 &&
        buf[len - 4] == ' ' && buf[len - 3] == '(' &&
        buf[len - 2] == 'A' && buf[len - 1] == ')')
        buf[len - 4] = '\0';
}

DEFINE_HOOK(&dMeter2Draw_c::getActionString, GetActionString);

static void on_get_action_string_post(ModContext*, void* args, void* ret, void*) {
    char* buf = *static_cast<char**>(ret);
    if (!buf || !buf[0]) return;

    strip_a_suffix(buf);

    if (buf[0] == 'S' && buf[1] == 'w' && buf[2] == 'i' && buf[3] == 'm' &&
        !(buf[4] >= 'a' && buf[4] <= 'z')) {
        buf[0] = 'D'; buf[1] = 'a'; buf[2] = 's'; buf[3] = 'h';
    }
}

DEFINE_HOOK(&dMeter2Draw_c::drawButtonA, DrawButtonA);

static void on_draw_button_a_post(ModContext*, void* args, void*, void*) {
    u8 action = mods::arg<u8>(args, 1);
    if (action != BUTTON_STATUS_DIVE) return;

    daAlink_c* link = static_cast<daAlink_c*>(dComIfGp_getPlayer(0));
    if (link && is_underwater_dive(link)) return;

    dMeter2Draw_c* meter = mods::arg<dMeter2Draw_c*>(args, 0);
    J2DScreen* screen = meter->getMainScreenPtr();
    if (!screen) return;

    J2DPane* aText = screen->search(MULTI_CHAR('cont_at'));
    if (!aText) return;
    const char* raw = static_cast<J2DTextBox*>(aText)->getStringPtr();
    if (!raw || !raw[0]) return;

    char buf[64];
    int len = 0;
    while (raw[len] && len < 63)
        buf[len] = raw[len++];
    buf[len] = '\0';
    strip_a_suffix(buf);

    static constexpr u64 rTags[] = {
        MULTI_CHAR('cont_zt1'), MULTI_CHAR('cont_zt2'),
        MULTI_CHAR('cont_zt3'), MULTI_CHAR('cont_zt4'),
        MULTI_CHAR('cont_rt')
    };
    for (int i = 0; i < 5; i++) {
        if (J2DPane* p = screen->search(rTags[i])) static_cast<J2DTextBox*>(p)->setString(64, buf);
    }

    static constexpr u64 aTags[] = {
        MULTI_CHAR('cont_at1'), MULTI_CHAR('cont_at2'),
        MULTI_CHAR('cont_at3'), MULTI_CHAR('cont_at4'),
        MULTI_CHAR('cont_at')
    };
    for (int i = 0; i < 5; i++) {
        if (J2DPane* p = screen->search(aTags[i])) p->hide();
    }

    for (int i = 0; i < 5; i++) {
        if (J2DPane* p = screen->search(rTags[i])) p->show();
    }
}

DEFINE_HOOK(&dMeterButton_c::setString, MeterButtonSetString);

static HookAction on_meter_button_set_string_pre(ModContext*, void* args, void*, void*) {
    if (dComIfGp_getDoStatus() != BUTTON_STATUS_DIVE) return HOOK_CONTINUE;

    daAlink_c* link = static_cast<daAlink_c*>(dComIfGp_getPlayer(0));
    if (link && is_underwater_dive(link)) return HOOK_CONTINUE;

    u8& button = mods::arg_ref<u8>(args, 2);
    if (button == dMeterButton_c::BUTTON_A_e)
        button = dMeterButton_c::BUTTON_R_e;
    return HOOK_CONTINUE;
}

DEFINE_HOOK(&dMeterButton_c::_execute, MeterButtonExecute);

static HookAction on_meter_button_execute_pre(ModContext*, void* args, void*, void*) {
    u8 status = dComIfGp_getDoStatus();
    daAlink_c* link = static_cast<daAlink_c*>(dComIfGp_getPlayer(0));

    if (status == BUTTON_STATUS_DIVE && link &&
        !is_underwater_dive(link)) {
        bool& drawA = mods::arg_ref<bool>(args, 2);
        bool& drawR = mods::arg_ref<bool>(args, 4);
        if (drawA) { drawA = false; drawR = true; }
    } else if (status == BUTTON_STATUS_SWIM && link &&
               is_zora_swim(link) && is_underwater(link)) {
        mods::arg_ref<bool>(args, 4) = true;
    }
    return HOOK_CONTINUE;
}

/* ------------------------------------------------------------------
 *  Entry points
 * ------------------------------------------------------------------ */
extern "C" {

MOD_EXPORT ModResult mod_initialize(ModError*) {
    if (mods::hook_add_pre<SwimMoveAnime>(svc_hook, on_swim_move_anime_pre) != MOD_OK ||
        mods::hook_add_post<SwimMoveAnime>(svc_hook, on_swim_move_anime_post) != MOD_OK) {
        svc_log->error(mod_ctx, "Enhanced Swimming: failed to hook setSwimMoveAnime");
        return MOD_ERROR;
    }

    if (mods::hook_add_post<SetStickData>(svc_hook, on_set_stick_data_post) != MOD_OK) {
        svc_log->error(mod_ctx, "Enhanced Swimming: failed to hook setStickData");
        return MOD_ERROR;
    }

    if (mods::hook_add_pre<ProcSwimMove>(svc_hook, on_proc_swim_move_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "Enhanced Swimming: failed to hook procSwimMove");
        return MOD_ERROR;
    }

    if (mods::hook_add_pre<SetNormalSpeedF>(svc_hook, on_set_normal_speed_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "Enhanced Swimming: failed to hook setNormalSpeedF");
        return MOD_ERROR;
    }

    if (mods::hook_add_post<GetActionString>(svc_hook, on_get_action_string_post) != MOD_OK) {
        svc_log->error(mod_ctx, "Enhanced Swimming: failed to hook getActionString");
        return MOD_ERROR;
    }

    if (mods::hook_add_post<DrawButtonA>(svc_hook, on_draw_button_a_post) != MOD_OK) {
        svc_log->error(mod_ctx, "Enhanced Swimming: failed to hook drawButtonA");
        return MOD_ERROR;
    }

    if (mods::hook_add_pre<MeterButtonExecute>(svc_hook, on_meter_button_execute_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "Enhanced Swimming: failed to hook dMeterButton::_execute");
        return MOD_ERROR;
    }

    if (mods::hook_add_pre<MeterButtonSetString>(svc_hook, on_meter_button_set_string_pre) != MOD_OK) {
        svc_log->error(mod_ctx, "Enhanced Swimming: failed to hook dMeterButton::setString");
        return MOD_ERROR;
    }

    svc_log->info(mod_ctx, "Enhanced Swimming initialized");
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    if (g_boostTimer > 0) g_boostTimer--;

    if (g_boostTimer > 0) {
        daAlink_c* self = static_cast<daAlink_c*>(dComIfGp_getPlayer(0));
        if (self && self->getZoraSwim()) {
            cXyz scl = {3.5f, 3.5f, 3.5f};

            JPABaseEmitter* em = dComIfGp_particle_set(dPa_RM(ID_ZI_J_LK_ABUKU_A),
                &self->current.pos, nullptr, &scl,
                0xFF, nullptr, -1, nullptr, nullptr, nullptr);
            if (em) em->setParticleCallBackPtr(dPa_control_c::getWaterBubblePcallBack());

            cXyz wakePos(self->current.pos.x, self->mWaterY, self->current.pos.z);
            dComIfGp_particle_set(dPa_RM(ID_ZI_J_HIKINAMI_A), &wakePos, nullptr, &scl);

            g_vfxTimer++;
            if (g_vfxTimer >= 5) {
                g_vfxTimer = 0;
                f32 vol = static_cast<float>(g_boostTimer) / static_cast<float>(g_boostTimerMax);
                Z2GetAudioMgr()->seStart(Z2SE_AL_SWIM_DASH, &self->current.pos,
                    0, 0, vol, 1.0f, -1.0f, -1.0f, 0);
            }
        }
    }

    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    return MOD_OK;
}

}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
