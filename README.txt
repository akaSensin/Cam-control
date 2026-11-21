================================================================================
  CAM CONTROL  -  version v0.1   (early build - not finished)
  made by sensin
  Camera control mod for Grand Theft Auto V (offline / single player)
================================================================================

An ASI plugin that adds an in-game menu for the third person and first person
camera of the character and of the vehicle the character is driving.

Unlike other mods that inject options into GTA V's pause-menu Settings
(forcing you to pause, adjust blindly, and unpause to check), Cam Control
opens right in front of you over the live game so you can see every camera
change in real time.

It drives the game's OWN camera. Nothing is created and nothing is replaced, so
walking, aiming, cover, driving, camera collision and - most importantly - the
mouse all behave exactly as they normally do. There is no custom camera layer
to fight with the game.

WHAT'S IN v0.1
  * Ten languages: English, Türkçe, Español, Русский, العربية, 中文, 日本語,
    Deutsch, Português, Bahasa Indonesia. Pick one under Settings > Language.
  * English, Turkish, Russian and Indonesian use the game's own text engine.
  * Spanish, Arabic, Chinese, Japanese, German and Portuguese are drawn by the
    mod itself through a DirectWrite overlay, because the game could not show
    them (see "LANGUAGES").
  * Translations can be corrected without rebuilding - see "LANGUAGES".


--------------------------------------------------------------------------------
1. WHAT YOU CAN CHANGE
--------------------------------------------------------------------------------

FOR EVERY CAMERA
  * View / POV        - force third or first person, or leave the game alone
  * Tilt Limit Up     - cap how far the camera may tilt up
  * Tilt Limit Down   - ...and down, independently. Each is off at 0, and
                        setting only one side works.

WEAPONS
  * One Field Of View per weapon class, applied while aiming (see "WEAPONS"
    further down). 0 = leave the game's own value alone.
  * Default (all other weapons) - pistols, SMG, rifles, MG and everything the
    game gives no camera of its own. There is a single camera behind all of
    them, so they cannot be listed separately.
  * Hip Fire, RPG / Launchers, Shotgun, Thrown, Melee / Animals, Digiscanner,
    Jerry Can, Sniper, Sniper (Low Zoom), Other Scopes / Aim
  * Also covers Run-and-Gun and Cinematic Shooting aiming cameras.

ON FOOT - THIRD PERSON
  * Field Of View     - applies across walk, sprint, stealth, cover, and swim
  * Follow Distance   - how far the camera sits behind the character
  * Pivot Offset      - shifts what the camera orbits around
  * Tilt Limit Up / Down
  * Keep FOV While Aiming - see "AIMING" below
  * While Aiming: Aiming Field Of View, Aiming Horizontal Origin, Aiming Follow
    Distance - the aiming camera gets its own values. 0 = same as third person.

ON FOOT - FIRST PERSON
  * Field Of View
  * Tilt Limit Up / Down
  * Unarmed Reticle   - Default / Always / Never
  * Camera Shake      - a shake the mod starts and keeps running; 0 = off

EXTRA CAMERAS
  * Parachute Field Of View / Follow Distance
  * Drive-By FOV (Cars/Bikes/Boats/Helis) - aiming weapons inside vehicles

SHARE PRESET CODE (SETTINGS)
  * Export Code (Copy) - copies a CC2:... preset code to the clipboard
  * Import Code (Paste) - imports a CC1:/CC2: preset code from the clipboard
    without sharing any files (keeps your own keybinds and language)

VEHICLE - THIRD PERSON
  * Field Of View
  * Follow Distance
  * Pivot Scale
  * Follow Zoom       - the game's own 0..4 follow zoom (a plain native)
  * Tilt Limit Up / Down
  * Camera Shake

VEHICLE - FIRST PERSON
  * Field Of View
  * View Offset Z     - nudge the eye point up / down
  * Tilt Limit Up / Down
  * Keep First Person In Water       - the game normally forces third person
  * Keep First Person When Wrecked     when the car submerges or is wrecked


--------------------------------------------------------------------------------
2. LANGUAGES
--------------------------------------------------------------------------------

  Settings > Language offers ten languages. "Applies to the menu and the
  on-screen text."

  HOW THEY ARE DRAWN

    English, Turkish, Russian, Indonesian
        Drawn by GTA's own text engine, exactly like every other mod. Nothing
        extra is loaded, nothing is hooked. This is the safe path.

    Spanish, Arabic, Chinese, Japanese, German, Portuguese
        The game renderer could not show these: its font has no Arabic or CJK
        glyphs at all, Arabic additionally needs letters joined into their
        contextual forms and the line ordered right-to-left, and the accented
        Latin ones (Spanish, German, Portuguese) came out blank on some glyphs
        even though they share the native path with Turkish and Russian.

        Rather than depend on whichever glyphs the game's font happens to
        carry, these six are drawn by the mod, in three steps:
          1. DirectWrite lays the string out with the right reading direction
             and does the Arabic shaping;
          2. Direct2D rasterises it in software into an ordinary bitmap
             (software on purpose - GTA's Direct3D device is created without
             BGRA support, so Direct2D cannot be bound to it);
          3. the bitmap is uploaded as a texture and drawn over the game in the
             swapchain Present callback.

        Strings are cached per (text, size, colour), so nothing is re-rasterised
        every frame.

  IS THE OVERLAY RISKY?
        It is more than the native path, and it is honest to say so:
          * it draws over the frame in the present callback, so it composites
            above everything the game draws - including the menu panel, which is
            still drawn with natives;
          * the fonts come from Windows (Arial for Arabic, Microsoft YaHei for
            Chinese, Yu Gothic for Japanese). If one is missing, DirectWrite
            falls back to another font that covers the glyphs;
          * if any step fails, the overlay disables itself, logs the reason to
            CamControl.log, and the menu falls back to the game renderer. A
            language switch back to English/Turkish/Russian/Indonesian never
            touches any of it.

        If you use ReShade or another fullscreen overlay, they and this overlay
        both draw in Present. Order depends on load order.

  FIXING A TRANSLATION WITHOUT REBUILDING
        Put a file next to the .asi named after the language code:

            lang_tr.txt   lang_es.txt   lang_ru.txt   lang_ar.txt
            lang_zh.txt   lang_ja.txt   lang_de.txt   lang_pt.txt
            lang_id.txt

        Each line is  English text=your text , for example:

            Field Of View=Görüş Açısı

        The English text must match exactly (it is the key - see the tables near
        the top of lang.cpp for the full list). A line that matches replaces the
        built-in translation; the file is read at start-up and whenever the
        language changes. This is the easiest way to review or correct a
        translation: no compiler needed.

  WHAT IS NOT TRANSLATED
        The mod name ("CAM CONTROL"), the log file, and the debug overlay's
        technical lines stay English on purpose.


--------------------------------------------------------------------------------
3. REQUIREMENTS
--------------------------------------------------------------------------------

  * Grand Theft Auto V - single player (offline). This is NOT for GTA Online.
  * ScriptHookV by Alexander Blade in the game folder
    (ScriptHookV.dll + dinput8.dll).  http://www.dev-c.com/gtav/scripthookv/
  * An ASI loader (dinput8.dll, which ships with ScriptHookV).


--------------------------------------------------------------------------------
4. INSTALLATION
--------------------------------------------------------------------------------

  1. Copy  CamControl.asi  into the main Grand Theft Auto V folder
     (the same folder that contains GTA5.exe and ScriptHookV.dll).

  2. Start the game in single player.

  3. Press  8  (number row) to open the menu.

  On the first run the mod writes these next to the .asi:

     CamControl.ini   - your configuration, read back on every game start
     CamControl.log   - what the plugin did, including the camera metadata scan

  Both are plain text. The .ini can be edited by hand while the game is closed.


--------------------------------------------------------------------------------
5. DEFAULT CONTROLS
--------------------------------------------------------------------------------

  8                       Open / close the menu   (primary key)
  F8                      Open / close the menu   (optional extra key)
  (unbound)               Optional second extra key - bind it in the menu

  Arrow Up / Down         Move up / down
  Arrow Left / Right      Decrease / increase a value, change an option
                          (hold them - they repeat, and speed up)
  Enter                   Select / toggle
  Backspace               Back, or close the menu from the main page

  Every one of these can be changed under  Main > Key Bindings.
  Select the row, press the key you want; ESC cancels.
  "Restore Default Keys" puts everything back.

  The arrow keys and Enter / Backspace ALWAYS keep working as a fallback, so a
  bad binding can never lock you out of the menu.

  Because of that, Enter and Backspace are not offered as assignable keys: the
  bind screen simply ignores them (press another key, or ESC to cancel). They
  stay reserved for select / back, so the menu-open key can never become Enter
  - which would make Enter open the menu AND select at the same time.


--------------------------------------------------------------------------------
6. MENU MAP
--------------------------------------------------------------------------------

  Main
   |- Mod Enabled ................. master switch
   |- (status line) ............... what camera you are on right now
   |- On Foot - Third Person ...... reset row, then FOV / distance / pivot /
   |                                tilt up / tilt down / keep FOV while
   |                                aiming, then the while-aiming values
   |- On Foot - First Person ...... reset row, then FOV / tilt / unarmed
   |                                reticle / aiming FOV / camera shake
   |- Vehicle - Third Person ...... reset row, then FOV / distance / pivot /
   |                                zoom / tilt / camera shake
   |- Vehicle - First Person ...... reset row, then FOV / offset / tilt /
   |                                keep first person in water / when wrecked
   |- Weapons ..................... one FOV per weapon class, used while aiming
   |- Colours ..................... accent presets or your own RGB
   |- Key Bindings ................ every key, including the menu-open key
   |- Settings .................... language, view / POV, metadata layer,
   |                                auto save, save / reload / reset, dev options
   '- About ....................... made by sensin

  THE MENU-OPEN KEY LIVES ON THE KEY BINDINGS PAGE
    It is changed under  Main > Key Bindings, together with every other key,
    rather than from a row on the main page. Keeping it out of the way means a
    keypress while moving around the menu cannot rebind it by accident. Enter
    and Backspace are refused as bindings for the same reason - they are always
    select and back.

  THE RESET ROWS
    The first row of every camera section resets that section only. There is
    also "Reset All To Defaults" on the Settings page for everything at once.

  THE TILT LIMIT AND FOLLOW ZOOM ARE NATIVES
    They work on every game build, immediately.

  CAMERA SHAKE
    The game overwrites a shake amplitude set from a script before the frame
    is drawn, so scaling its shake from here had no visible effect. The mod
    therefore starts a gameplay-camera shake of its own and keeps it running
    at the value you set: 0 = off, 100 = clearly visible, up to 300. Setting a
    value back to 0 stops only the shake the mod started - the game's own
    shakes (explosions, crashes) are never touched.

  FOV / FOLLOW DISTANCE / PIVOT NEED THE METADATA LAYER
    See the next section.


--------------------------------------------------------------------------------
7. CAMERA METADATA (FOV, DISTANCE, PIVOT) - READ THIS
--------------------------------------------------------------------------------

  GTA V keeps the numbers that shape its follow cameras (fov, follow distance,
  pivot, ...) in per-camera "metadata" objects, loaded from its own camera .meta
  data. The game exposes NO native to change them - only a getter for the FOV it
  is currently using.

  So those specific sliders work the way other camera mods work: they write the
  value straight into the live metadata object of the camera that is on screen.

  That is inherently build specific - the field offsets move between game
  versions. The layer is therefore written to fail safe:

     * On start-up it scans the game module for the camera metadata pool and
       writes the result to CamControl.log, e.g.
           metadata: layer active
           metadata: layer disabled

     * Before every write it checks that the object it found really is the
       metadata it asked for. If not, it writes nothing.

     * If the scan fails, the layer turns itself off completely. Every other
       option in the mod keeps working; only the FOV / distance / pivot sliders
       go quiet, and the Camera page says so.

  You can turn the layer off yourself with
      Camera > "Metadata Layer (FOV etc.)"
  and everything else keeps working. That is what to do if FOV ever behaves
  oddly on your build.

  AIMING
    Aiming makes the game switch to a DIFFERENT camera with its own metadata, so
    by default your FOV would snap back to vanilla the moment you aim.

    "Keep FOV While Aiming" (on the ON FOOT - THIRD PERSON section, on by
    default) writes the same FOV / distance / pivot into that aiming camera too,
    and clears its "use weapon fov setting" flag so the weapon's own FOV does not
    override ours.

    Turn it off if aiming feels off on your build, or if you actually want the
    vanilla aimed FOV / scope behaviour - then only the normal camera is touched.

  IF THE FOV SLIDERS DO NOTHING
     1. Open CamControl.log and look for the "metadata:" lines.
     2. "layer disabled" means the pool was not found on your game build - tell
        me the version line right above it (the log prints it) and it can be
        mapped.
     3. "layer active" but no visible change means the offsets land on the wrong
        field for your build - again, the logged game version is what identifies
        the right set.


--------------------------------------------------------------------------------
8. WEAPONS - ONE FOV PER WEAPON CLASS
--------------------------------------------------------------------------------

  The game does not aim every weapon through the same camera. When you aim, it
  looks up the weapon in your hands in its own weapons.meta and switches to a
  camera NAMED there. Most weapons name nothing, which is why they all share one
  camera:

    WEAPON_PISTOL, WEAPON_SMG, WEAPON_CARBINERIFLE, ...  ->  no entry in the
                                                            weapon data  ->  the
                                                            default aiming camera

  The ones that DO name a camera of their own:

    HIP_AIM_CAMERA                  grenade launcher, fire extinguisher, petrol can
    RPG_AIM_CAMERA                  RPG, stinger, homing launcher, vehicle rocket
    SHOTGUN_AIM_CAMERA              shotguns, aimed or scoped
    THROWN_AIM_CAMERA               grenade, sticky bomb, smoke, BZ gas, molotov,
                                    flare, ball
    MELEE_AIM_CAMERA                knife, fist, animals
    DIGISCANNER_AIM_CAMERA          digiscanner
    JERRY_CAN_RUN_AND_GUN_CAMERA    petrol can
    SNIPER_AIM_CAMERA               sniper rifle
    SNIPER_LOW_ZOOM_AIM_CAMERA      sniper rifle, low zoom

  Each of those is a separate camera object with its own field of view, so the
  Weapons page gives each of them one. That is nine classes plus everything else.

  WHY PISTOLS, SMG AND RIFLES ARE NOT LISTED SEPARATELY
    Because there is nothing behind them to list. They all resolve to the same
    single camera, so the "Default" row IS the pistol / SMG / rifle row. The game
    never asks for a different camera for them, so there is no way to give them
    different FOVs.

  0 MEANS "DO NOT TOUCH"
    A fresh install writes nothing here, so aiming looks exactly like the game
    made it. Move a slider and that one class changes.

  HOW IT RELATES TO "KEEP FOV WHILE AIMING"
    "Keep FOV While Aiming" is the master switch for the whole aiming layer. With
    it off, nothing on the Weapons page is written at all.

    With it on:
      * "Default" left at 0      -> the default aiming camera keeps the same FOV
                                    as your on-foot third person camera, which is
                                    what the mod did before the Weapons page
                                    existed.
      * "Default" given a value  -> that value wins for the default aiming camera,
                                    and the other classes use their own rows.

  THE TWO SNIPER CAMERAS ARE A DIFFERENT KIND OF CAMERA OBJECT
    They are not the same metadata type as the other seven, so their FOV sits at
    a different offset inside the object (0x24 instead of 0x30). Both offsets come
    from the game's own camera data and are worked out step by step in
    Docs/CAMERA-RESEARCH.md - if the sniper rows ever stop working after a game
    update, that document says how to re-derive them.

  WHY EACH WEAPON CAMERA NEEDS ITS OVERRIDE FLAG CLEARED
    Writing BaseFov is not enough on its own: while a camera's "use weapon fov
    setting" flag is still set, the game prefers the FOV the weapon itself asks
    for and ignores BaseFov. That is why the per-weapon rows did nothing before.
    The flag is cleared on the cameras whose flag offset is verified - Hip Fire,
    RPG, Shotgun, Thrown, Digiscanner and the default aiming camera. Melee, Jerry
    Can and the two sniper cameras are different metadata types whose flag
    offset has not been verified yet, so their BaseFov is written but the flag is
    left alone; they may still follow the weapon's own FOV.


--------------------------------------------------------------------------------
9. FILES & INSTALLATION
--------------------------------------------------------------------------------

  Place CamControl.asi in the GTA V root folder (next to GTA5.exe).
  All mod data files live inside:

      GTA V\scripts\Cam Control\

  CamControl.ini
      Written inside scripts\Cam Control\ whenever a value changes (if Auto
      Save is on) and when the menu closes. Read on start-up.

      Keys are Win32 virtual-key codes in decimal:
        56 = the number-row 8 (default open key)   119 = F8
        38 = UP      40 = DOWN      37 = LEFT      39 = RIGHT
        13 = ENTER   8 = Backspace  118 = F7 (a popular alternative)

  CamControl.log
      Timestamped log inside scripts\Cam Control\. Rotated to CamControl.log.old
      when it passes 1 MB. Says which key opened the menu, which camera values
      were applied, and whether the camera metadata layer came up.


--------------------------------------------------------------------------------
10. NOTES, VERSION SUPPORT, VIRUSTOTAL & LIMITS
--------------------------------------------------------------------------------

  * Single player only. Do not use this in GTA Online.

  * Supported Game Version:
    Works perfectly on the latest GTA V Legacy version. It can also work
    starting from v1.0.3717.0 ("A Safehouse in the Hills" Update) or even older
    versions if you want to try, though at least v1.0.3717.0 is recommended.
    No GTA V Enhanced Edition build is provided because the author does not own
    Enhanced Edition on PC. If the mod does not work on your game version or
    setup, please report it in the comments or open an issue on GitHub
    (akasensin) and I will try to help as much as I can.

  * Antivirus / VirusTotal False Positives:
    Unsigned C++ .asi mods that read/write game memory can trigger 2 to 5
    false-positive flags out of ~70 vendors on VirusTotal. That is normal for
    this kind of mod. This project is fully open source: you can inspect the
    source code yourself (or ask any AI / LLM to review it) and compile it
    directly with Visual Studio / MSBuild before using it.

  * Rockstar Terms of Service:
    No original Rockstar game files (such as cameras.ymt or extracted game
    XMLs) are shipped or shared in this release or repository due to Rockstar
    Terms of Service.

  * Nothing is hooked and nothing in the game's code is patched. The mod only
    calls public natives and, optionally, writes values into a camera metadata
    object it has verified.

  * Forcing a view mode overrides the in-game V key while the mod is enabled.
    Use "Don't touch" to leave the game in control.
    The force is released as soon as you press V: the mod sets the mode once,
    and if the camera then moves to another mode it treats that as your
    decision and switches the setting back to "Don't touch" (logged in
    CamControl.log and saved).

  * The mod does not add a scripted camera, so there is no separate camera to
    get stuck on or to be reclaimed by the game's camera director.

  * ScriptHookV must match your game version. If the menu never opens at all,
    check scripts\Cam Control\CamControl.log - if it was not created, the ASI
    was not loaded (wrong folder, missing ASI loader, or a version mismatch).

  Everything else in this mod is written from scratch against the public
  ScriptHookV SDK and the game's own natives. There is no third-party code in it.


================================================================================
  made by sensin
  akasensin on github
================================================================================
