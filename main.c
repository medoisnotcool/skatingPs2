/* Skate park - PS2 homebrew (ps2sdk + gsKit).  No audio.  See README.txt to build. */
#include "ui.h"

#ifdef HOST_TEST
extern void host_frame_hook(int frame);
void host_report(void) {
    static const char *SN[] = { "FOOT", "CARRY", "RIDE", "GRIND", "BAIL" };
    printf("state=%s pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) grounded=%d score=%ld combo=%d msg='%s' grind='%s'\n",
           SN[P.state], P.pos.x, P.pos.y, P.pos.z, P.vel.x, P.vel.y, P.vel.z, P.grounded, P.score, P.comboPts, P.msg, P.grindName);
}
#endif

static void gs_setup(void) {
    g_gs = gsKit_init_global_custom(3 * 1024 * 1024, 128 * 1024);
    g_gs->PSM = GS_PSM_CT24;
    g_gs->PSMZ = GS_PSMZ_16S;
    g_gs->ZBuffering = GS_SETTING_OFF;           /* painter's algorithm instead */
    g_gs->DoubleBuffering = GS_SETTING_ON;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(g_gs);
    gsKit_set_test(g_gs, GS_ZTEST_OFF);
    gsKit_mode_switch(g_gs, GS_ONESHOT);
    g_sw = g_gs->Width;
    g_sh = g_gs->Height;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    gs_setup();
    pad_init();
    options_defaults();
    tricktab_init();
    level_init();
    player_reset();
    camera_update(0.016f, 1);
    const float DT = (g_gs->Mode == GS_MODE_PAL) ? 1.0f / 50.0f : 1.0f / 60.0f;
    int frame = 0;

    while (!g_quit) {
        pad_poll();
#ifdef HOST_TEST
        { const char *b = getenv("BAILAT"); if (b && frame == atoi(b)) bail_start("BAIL - TEST"); }
#endif
        if (g_ms == MS_PLAY) {
            if (g_in.pressed & PAD_START) { g_ms = MS_PAUSE; g_sel = 0; }
            else {
                if (g_wantRespawn) { g_wantRespawn = 0; int sc = (int)P.score; player_reset(); P.score = sc; camera_update(DT, 1); }
                player_update(DT);
                camera_update(DT, 0);
            }
        } else {
            menu_update();
            if (g_ms == MS_PLAY && g_wantRespawn) { g_wantRespawn = 0; long sc = P.score; player_reset(); P.score = sc; camera_update(DT, 1); }
        }
        if (g_ms == MS_MAIN || (g_optFrom == MS_MAIN && (g_ms == MS_OPT || g_ms == MS_CTRL)) || g_ms == MS_HELP) {
            g_clock += DT;                                           /* slow fly-around behind the menus */
            float a = g_clock * 0.25f;
            gfx_camera(v3(sinf(a) * 20.0f, 7.5f, cosf(a) * 14.0f - 2.0f), v3(0, 1.2f, -2), 60.0f);
        }

        gsKit_clear(g_gs, GS_SETREG_RGBAQ((g_bgRGB >> 16) & 255, (g_bgRGB >> 8) & 255, g_bgRGB & 255, 0x80, 0x00));
        scene_draw();
        if (g_ms == MS_PLAY) hud_draw(); else menu_draw();
        gsKit_queue_exec(g_gs);
        gsKit_sync_flip(g_gs);
#ifdef HOST_TEST
        host_frame_hook(frame);
#endif
        frame++;
    }
    return 0;
}
