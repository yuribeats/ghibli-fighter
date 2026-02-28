#!/bin/bash
set -e

echo "Building SF2WW for web..."

mkdir -p web_build
mkdir -p web_build/assets/music

emcc \
  -O2 \
  -s ASSERTIONS=2 \
  -s LEGACY_GL_EMULATION=1 \
  -lglut \
  -s ASYNCIFY=1 \
  -s ASYNCIFY_STACK_SIZE=65536 \
  -s INITIAL_MEMORY=67108864 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -I. \
  -Igifdec \
  -Istb \
  -ISiennaBird \
  -IRedHammer \
  -IFistBlue \
  -IFistBlue/ai \
  -IFistBlue/actions \
  -IFistBlue/avatars \
  -IFistBlue/gfxdata \
  -IFistBlue/scrolls \
  -IFistBlue/tests \
  -ISwiftBeam \
  glutBasics.c \
  trackball.c \
  gifdec/gifdec.c \
  stb/stb_image.c \
  RedHammer/pthreads.c \
  RedHammer/redhammer.c \
  SwiftBeam/glwimp.c \
  FistBlue/gif_background.c \
  FistBlue/char_overlay.c \
  FistBlue/music_player.c \
  FistBlue/coinage.c \
  FistBlue/collision.c \
  FistBlue/coll_bonus.c \
  FistBlue/coll_projectile.c \
  FistBlue/computer.c \
  FistBlue/demo.c \
  FistBlue/effects.c \
  FistBlue/endings.c \
  FistBlue/fightgfx.c \
  FistBlue/game.c \
  FistBlue/gemu.c \
  FistBlue/gfx_glut.c \
  FistBlue/gfxlib.c \
  FistBlue/lib.c \
  FistBlue/particle.c \
  FistBlue/player.c \
  FistBlue/playerselect.c \
  FistBlue/playerstate.c \
  FistBlue/projectiles.c \
  FistBlue/reactmode.c \
  FistBlue/rules.c \
  FistBlue/sm.c \
  FistBlue/sound.c \
  FistBlue/sprite.c \
  FistBlue/task.c \
  FistBlue/text.c \
  FistBlue/scrolls/gstate.c \
  FistBlue/scrolls/parallax.c \
  FistBlue/scrolls/scroll_data.c \
  FistBlue/scrolls/scroll_maint.c \
  FistBlue/ai/ai.c \
  FistBlue/tests/aitests.c \
  FistBlue/tests/testlib.c \
  FistBlue/actions/actions.c \
  FistBlue/actions/actions_198a.c \
  FistBlue/actions/actions_530a.c \
  FistBlue/actions/act02_bicycleriders.c \
  FistBlue/actions/act07_elephants.c \
  FistBlue/actions/act16.c \
  FistBlue/actions/act17.c \
  FistBlue/actions/act1e_worldflags.c \
  FistBlue/actions/act29_wwlogo.c \
  FistBlue/actions/act2e_plane.c \
  FistBlue/actions/act3e_capcomlogos.c \
  FistBlue/actions/act_3f.c \
  FistBlue/actions/car.c \
  FistBlue/actions/drums.c \
  FistBlue/actions/reels.c \
  FistBlue/actions/barrels.c \
  FistBlue/avatars/ryu.c \
  FistBlue/avatars/guile.c \
  FistBlue/avatars/ehonda.c \
  FistBlue/avatars/blanka.c \
  FistBlue/avatars/chunli.c \
  FistBlue/avatars/zangeif.c \
  FistBlue/avatars/dhalsim.c \
  FistBlue/avatars/mbison.c \
  FistBlue/avatars/sagat.c \
  FistBlue/avatars/balrog.c \
  FistBlue/avatars/vega.c \
  FistBlue/avatars/blanka_comp.c \
  FistBlue/avatars/chunli_comp.c \
  FistBlue/avatars/ehonda_comp.c \
  FistBlue/avatars/guile_comp.c \
  FistBlue/avatars/ryuken_comp.c \
  FistBlue/avatars/blanka_human.c \
  FistBlue/avatars/chunli_human.c \
  FistBlue/avatars/ehonda_human.c \
  FistBlue/avatars/guile_human.c \
  FistBlue/avatars/ryuken_human.c \
  --preload-file build/allroms.bin@allroms.bin \
  --preload-file build/sf2gfx.bin@sf2gfx.bin \
  --preload-file build/assets/backgrounds@assets/backgrounds \
  --shell-file shell.html \
  -o web_build/index.html

if [ -f "SF2.app/Contents/Resources/assets/music/redrumlake.25minutes.mp3" ]; then
    cp "SF2.app/Contents/Resources/assets/music/redrumlake.25minutes.mp3" web_build/assets/music/
elif [ -f "build/assets/music/redrumlake.25minutes.mp3" ]; then
    cp "build/assets/music/redrumlake.25minutes.mp3" web_build/assets/music/
fi

echo "Build complete. Output in web_build/"
